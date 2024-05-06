#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <iostream>
#include <queue>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <mutex>
#include <thread>
#include <atomic>
#include <future>
#include <condition_variable>

const int MAX_TASK_SIZE = INT32_MAX;
const int MAX_THREAD_SIZE = 100;
const int THREAD_IDLE_TIME_LIMIT = 60;

// 线程池模式枚举
enum class PoolMode		// 之前的枚举使用枚举型不用加枚举类型，导致可能出现不同枚举里的同名枚举项问题，新标准里可以用 enum class防范这个问题。使用 enum class需要用作用域访问枚举项
{
	MODE_FIXED,			// 固定线程数量模式，依机器核数而定		-- 场景：处理耗时的任务
	MODE_CACHED			// 线程数量可动态增长					-- 场景：处理小而快的任务，任务处理比较紧急
};

// --------------------------------------- 线程类 ---------------------------------------
class Thread
{
	using ThreadFunc = std::function<void(int)>;
public:
	Thread(ThreadFunc func_)
		: func(func_)		// 线程对象构造时，用传入的线程池对象里的线程函数初始化自己的线程函数
		, thread_id(generate_id++)		// 在线程对象构造时，id就自动产生了
	{}

	~Thread()
	{}

	void start()
	{									// start 让线程开始执行线程池传进来的线程函数
		std::thread T(func, thread_id);

		// 出了 start，对象T就析构了，所以要把线程设为分离线程，线程对象离开作用域析构后线程栈仍存在，线程函数仍然会继续运行
		T.detach();
	}

	int getId() const
	{
		return thread_id;
	}
private:
	ThreadFunc func;

	static int generate_id;		// 线程id是多少无所谓，只要不一样可以代表这个线程就行，用这个变量变化产生线程id
	int thread_id;				// 保存线程id
};

// 静态成员变量必须在类外初始化，因为类内初始化会使每个对象都包含这个成员，若不在类外初始化，编译器在第一次调用该变量时可能会初始化为0
int Thread::generate_id = 0;	
// --------------------------------------- 线程类 ---------------------------------------

// --------------------------------------- 线程池类 ---------------------------------------
// 线程池类
class ThreadPool
{
public:
	// 线程池构造
	ThreadPool()
		: pool_mode(PoolMode::MODE_FIXED)				// 默认为固定线程数量模式
		, initial_thread_size(0)						// 初始线程数量，start()方法中一定赋初值，这里设为0就行
		, idle_thread_size(0)							// 记录空闲线程的数量
		, cur_thread_size(0)							// 当前线程数量
		, thread_size_upper_limit(MAX_THREAD_SIZE)		// 线程数量上限，无休止增加程序会崩掉
		, task_size(0)									// 任务由用户提交，所以一开始没有任务
		, is_pool_runnig(false)							// 调用start()方法才可以设为true
		, task_size_upper_limit(MAX_TASK_SIZE)			// 设定任务上限以免占用过多内存
	{}

	// 线程池析构
	~ThreadPool()
	{
		// 离开线程池对象的作用域，会自动调用线程池对象的析构。
		// 那么，回收线程池中所有的线程资源，是有必要的。而且线程函数需要等待所有的线程回收完才能析构完成
		// 就可以用到线程通信，线程池离开作用域通知线程函数返回结束，设条件变量等待线程池中所有的线程指针析构，析构一个检查一次是否全部析构，全部析构则线程池成功析构
		// 主要是回收线程指针，如何回收呢？一个线程结束，将它从容器中erase，会自动调用元素的析构函数，那么作为被智能指针管理的线程对象，它就成功释放资源了
		is_pool_runnig = false;

		// 本线程抢到锁没用，在进入等待状态后依然会把锁释放，给那些需要回收资源的线程获取从而继续流程
		std::unique_lock<std::mutex> lock(task_que_mtx);
		// 此时所有的线程有两种状态：1.等待“有任务资源”，2.正在执行任务,当没有任务时，它也会在执行完任务后达到等待任务资源的状态	（其实还有第三种情况，因为没考虑发生了死锁）
		// 那么需要线程通信，析构函数（一般是主线程）所在线程发出唤醒指令
		not_empty.notify_all();

		all_thread_end.wait(lock, [&]() { return threads.size() == 0; });		// 等待“线程队列为空”资源

		std::cout << "_______________THREAD POOL END_______________\n";
	}

	// 设定线程池模式
	void setMode(PoolMode mode)
	{
		if (checkRunningState() == false)
		{
			std::cout << "Err: Please start the thread pool before you operating it.\n";
			return;
		}
		this->pool_mode = mode;
	}

	// 用户开启线程池方法
	void start(int init_thread_size = 4)
	{
		std::cout << "_______________THREAD POOL START_______________\n";
		// 记录初始化线程个数
		this->initial_thread_size = init_thread_size;
		this->cur_thread_size = init_thread_size;

		// 创建初始数量的线程
		for (int i = 0; i < initial_thread_size; i++)
		{													// 创建 Thread线程对象的时候，通过绑定器把线程池类的线程函数给到 Thread类的线程对象
			std::unique_ptr<Thread> p_thread = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));	// 普通指针是new，智能指针是 make_nuique / make_shared
			int thread_id = p_thread->getId();
			threads.emplace(thread_id, std::move(p_thread));		// unique_ptr 禁用了左值引用，所以要么直接在 emplace_back里 make_unique，要么用std::move进行资源转移
			// emplace_back() 和 push_back() 的区别，就在于底层实现的机制不同。push_back() 向容器尾部添加元素时，首先会创建这个元素，然后再将这个元素拷贝或者移动到容器中（如果是拷贝的话，事后会自行销毁先前创建的这个元素）；
															// 而 emplace_back() 在实现时，则是直接在容器尾部创建这个元素，省去了拷贝或移动元素的过程。创建线程这么重的操作，先创建再拷贝显然是不合适的，再者thread本身也禁止了拷贝构造和赋值操作符，总上添加线程对象只能用 emplace_back()
			threads[thread_id]->start();	// 启动一个线程函数
			idle_thread_size++;		// 记录空闲线程的数量  线程虽然启动，但是没有取到任务就是空闲的
		}
		//// 启动所有线程
		//for (int i = 0; i < initial_thread_size; i++)
		//{
		//}
		// 改变线程池运行状态
		is_pool_runnig = true;
	}

	// 设置任务队列的数量上限
	void setTaskSizeUpperLimit(int max_size)
	{
		if (checkRunningState() == false)
		{
			std::cout << "Err: Please start the thread pool before you operating it.\n";
			return;
		}
		this->task_size_upper_limit = max_size;
	}

	// cached模式下，设置线程队列数量上限
	void setThreadSizeUpperLimit(int max_size)
	{
		if (checkRunningState() == false)
		{
			std::cout << "Err: Please start the thread pool before you operating it.\n";
			return;
		}

		if (pool_mode == PoolMode::MODE_FIXED)
		{
			std::cout << "Tip: You can't set the upper limit of thread size in MODE_FIXED.\n";
			return;
		}

		this->thread_size_upper_limit = max_size;
	}

	// 用户给线程池提交任务，使用C++11可变参模板编程，让 submitTask 可以接收任意任务函数和任意数量的参数 pool.submit(sumN, 1, 2, 3, ..,N)
	template<typename Func, typename... Args>			// &&既能接收左值引用，也能接收右值引用
	auto submitTask(Func&& func, Args&&... args) -> std::future<decltype(func(args...))>
	{
		// 编译期间会根据传入模板的参数和值类型推导后进行实例化，主要工作是文本替换，还有类型检查等功能
		// 编译器完成上述工作，编译后得到一个期望类型执行的代码
		// 通过 auto获取->后的类型，即一个future<T>类型，
		// T的类型通过C++14的 decltype推导func函数传入参数包args的返回类型
		// 最后 auto推导出的类型即任务执行后返回的结果 future，如 sum(1, 2)返回 future<int>
		// decltype关键字可以在编译时获取表达式的类型信息，而无需实际执行表达式。
		// 这为泛型编程、模板元编程和其他需要类型推导的场景提供了更大的灵活性和表达能力。

		// Task 的声明是 using Task = std::function<void ()>
		// 用 std::packaged_task打包任务（函数），函数返回值decltype推导出来，参数用 bind绑定器绑定函数对象与参数包args
		// 除此之外，因为传入的函数和参数包可能是左值引用也可能是右值引用，所以用完美转发保证其左值或右值属性不变
		// 右值（rvalue）：指的是临时对象或即将被销毁的对象，它们没有固定的内存地址，或者其地址在表达式结束后不再有效。右值通常出现在赋值操作符的右侧，并且不能被赋值或取得其地址（除了常量表达式）。
			// 右值引用是对右值的一个引用，但是右值引用在语法上是一个左值，因为它有名字，可以出现在赋值操作符的左侧，也可以取得其地址。然而，右值引用的语义与传统的左值不同：
			// 右值引用绑定到右值，并且可以通过std::move来显式地将左值转换为右值引用。一旦一个对象被绑定到右值引用，该对象就被视为“将要被移动”的，这意味着它的资源（如内存、文件句柄等）可以被安全地“窃取”或“移动”到另一个对象，而无需进行深拷贝。这可以提高程序的性能，特别是当处理大型对象或资源密集型对象时。
			// 右值引用的主要目的是支持移动语义（move semantics），即允许对象在不再需要时将其资源“移动”到另一个对象，而不是进行昂贵的深拷贝操作。这可以减少不必要的内存分配和释放，提高程序的性能和效率。
			// 引用折叠与右值引用密切相关，因为当我们在模板编程中使用T&&时，实际上是通过引用折叠来确定它最终是左值引用还是右值引用。
		using RetType = decltype( func(args...));								// 用 using代表类型，不能用 auto存类型
		// std::packaged_task<int(int, int)> task(sum1);
		auto task = std::make_shared< std::packaged_task<RetType ()>>(
			// std::bind(std::forward<Func>(func), std::forward<Args>(args)...)	// 要打包的函数对象用绑定器绑定函数体和参数
			// [&func, &args...]() { return std::invoke(func, args...); }			// 实际上 bind不支持完美转发，C++17的invoke可以支持，可以用invoke
			[&func, &args...]() {
				return std::invoke(std::forward<Func>(func), std::forward<Args>(args)...);
			}
		);
		// 获取打包任务的 future对象，作为 submitTask的返回值
		std::future<RetType> future = task->get_future();

		if (checkRunningState() == false)
		{
			std::cout << "Err: Please start the thread pool before you operating it.\n";
			auto task = std::make_shared< std::packaged_task<RetType()>>(
				[]()->RetType { return RetType(); }
			);
			(*task)();		// future.get() 是一定需要执行的，如果线程池没有启动，task执行不了，那么 get也就无措下手，调试发现get()调用时的 future_error ，直接执行会abort()，所以这里我们给他执行掉，这样 future的get才有机会执行
			return task->get_future();
		}
		// 1.获取锁
		std::unique_lock<std::mutex> lock(task_que_mtx);			// 防止多线程操作同一队列内存造成的线程安全问题

		// 2.线程通信：等待任务队列有空余 not_full.wait				// 不能让用户一直等待，所以用户提交任务，最长不能超过1s，超过则直接返回，提交任务失败
		if (not_full.wait_for(lock, std::chrono::seconds(1), [&]()->bool {							// 第二个参数传谓语，即返回值为 bool类型的函数或伪函数，满足时继续执行，不满足时线程进入等待状态
			return task_que.size() < task_size_upper_limit;
			}) == false)														// while (!_Pred()) {   wait(_Lck);	 }  转到定义，是用 while()循环实现的方法
		{	// 说明等待了 1s，条件仍然没有满足（考虑到内存的有限性，任务队列的限制是一定要设置的）
			std::cerr << "[Err]: Task queue is full, the submit task operation failed.\n";
			// 任务提交失败，返回一个包装了 直接返回目标返回类型的空值的lambda表达式 的Task的future
			// 因为是包装的任务就是直接返回，所以不会阻塞在 get处，不能返回原任务的 future，既然无法提交，也就无法执行，无法返回，也就会一直阻塞在get
			auto task = std::make_shared< std::packaged_task<RetType()>>(
				[]()->RetType { return RetType(); }
			);
			(*task)();		// 提交失败，没机会执行，get()无从下手，调试发现get()调用时的 future_error ，直接执行会abort()，所以要让它在这里执行掉
			return task->get_future();
		}

		// 3.如果有空余，把任务放入任务队列
			// task_que.emplace(task);									// emplace 底层调用的也是 emplace_back，如果不传参数与emplace_back功能一样，传参数应该是在某位置直接构造对象，其他元素后移的意思
			// using Task = std::function<void ()> 
			// std::queue<Task> task_que; 任务队列里的任务类型是没有返回值的
			// 注意这里的处理！封装一层没有参数没有返回值的lambda表达式传入任务队列，在里面执行真正要执行的任务即可
		task_que.emplace([task]() {
			(*task)();
			// task是管理 packaged_task对象的指针，解引用得到 packaged_task，也就是一个封装了的函数对象，后加()操作符触发包装函数的执行
			// 一旦这个 lambda函数被线程池执行，里面的函数也就会被执行
		});
		task_size++;												// 为什么不直接调用 task_que.size()，因为这个变量后面会用到

		// 4.线程通信：新放了任务，队列一定不空 not_empty.notify   --   通知线程来执行任务
		not_empty.notify_all();

		// 既然新提交了任务到任务队列，如果线程池是MODE_CACHED,就检查一下是否应该新增线程
		if (pool_mode == PoolMode::MODE_CACHED &&
			task_size > idle_thread_size &&					// 任务数量大于空闲线程的数量
			cur_thread_size < thread_size_upper_limit)		// 当前线程数量小于设定的数量上限
		{
			std::cout << "> > > > create new thread\n";
			// 线程要用run启动线程函数，一般的回调函数直接传入即可，成员函数不行，但可以通过bind传入成员函数地址和成员函数的对象，即是哪个对象调用的成员函数，同时，如果还需要传入参数到这个成员函数，可以使用占位符,_1用于代替回调函数中的第一个参数，以此类推
			std::unique_ptr<Thread> p_thread = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
			int thread_id = p_thread->getId();
			threads.emplace(thread_id, std::move(p_thread));
			// 现在是map存储线程了，可以通过线程id作为键，启动线程
			threads[thread_id]->start();
			// 修改相关资源数量
			cur_thread_size++;
			idle_thread_size++;
		}

		return future;	// 默认 true
	}

	// 检查线程池的运行状态
	bool checkRunningState()
	{
		return is_pool_runnig;
	}

	// 禁用拷贝构造，线程池管理的对象很多，拷贝发生问题
	ThreadPool(const ThreadPool& thread_pool) = delete;

	// 禁用赋值，线程池管理的对象很多，拷贝发生问题
	ThreadPool& operator=(const ThreadPool& thread_pool) = delete;		

private:
	// 定义线程函数									从任务队列取任务并执行                                                                                                                                                                                                                                                                                
	void threadFunc(int thread_id)
	{
		// 线程函数第一次调用的时间
		auto last_time = std::chrono::high_resolution_clock().now();

		for (;;)
		{
			/* 死锁问题
					有两种结束时的状态已经考虑，一个是线程在wait处等待任务，一个是线程在exec()处执行任务
					但是还有第三种情况没有考虑 -- 线程执行完任务，通过这个while的检查进入循环，此时线程池发生析构
					而且此时任务队列已经没有任务了，那么看下面的代码，线程执行到 not_empty.wait(lock)处。
					有两处可以唤醒这个wait，一个是 有用户提交任务，此时没有新的任务了；一个是线程池析构函数，此时已经唤醒过，在wait本线程函数的唤醒，不会发生第二次唤醒了
					也就是两个唤醒的时机都错过。那么本线程函数就永远卡在wait里了
					本线程等待等不来的唤醒，主线程的析构等待本线程唤醒，那么主线程永远不会析构，卡在了wait那里，发生了死锁问题
					所以这个死锁产生的核心逻辑是：可以唤醒该线程函数的wait的事件都已经错过了，那么该线程永远阻塞
			*/
			/*
				那么~ThreadPool()析构函数和线程函数之间的设计有缺陷，只考虑了第一种场景和第二种场景
			*/
			// std::this_thread::sleep_for(std::chrono::seconds(3));	// 在这里休眠一会，更容易发生死锁

			Task task;
			{
				// 先获取锁		我在想 任务队列有一个任务，若两个线程同时检查到 task_que.size() > 0，跳过了while，直接去取任务，其中一个先取到，任务队列空了怎么办 -- 额，前面上锁了呀，只有抢到锁的才能判断是否有任务，另一个要被阻塞在线程函数开始位置的。其实这就是用互斥锁防止数据竞争问题的典型案例
				std::unique_lock<std::mutex> lock(task_que_mtx);

				// 针对线程池析构函数先获得锁的情况，is_pool_running的值已被改变，那么在这里的判断语句加上对 is_pool_runnig的判断，就可以跳出循环，而不会无限等待在循环里的两种wait里
				// 因为线程池析构要等待所有线程执行完，所以取到任务的线程也会执行完任务，再找任务，直到没有任务，才回收线程资源，所有资源回收，才会完成线程池析构，线程池析构在主线程，不完成线程池析构，主函数就无法继续执行
				while (task_que.size() == 0)		// while逻辑：!= 0说明有任务，有任务优先执行任务，其他操作放在此循环体内执行
				{
					// 没任务待执行，那么先检查是应该析构还是等待新任务进任务队列
					if (is_pool_runnig == false)
					{
						// 回收当前线程-------------
						threads.erase(thread_id);	// 容器删除线程			STL容器删除元素会自动调用元素的析构函数
						std::cout << "> > > > thread " << std::this_thread::get_id() << " recalimed\n";		// 打印日志
						all_thread_end.notify_all();		// 唤醒等待中的条件变量，让它检查有没有满足在等待的条件 -- 即线程池中一个线程也没有了，如果它发现没有满足，会继续等待，即本线程并不是最后一个被删除的线程
					}

					// cached模式下，等待“有任务”资源，且每秒检查一次空闲时间，若线程空闲时间超过60s，而且当前线程数 > 初始线程数，则回收该线程
					// 当前时间 - 上次线程执行任务的时间
					if (pool_mode == PoolMode::MODE_CACHED)
					{
						// 每一秒更新一次当前时间，查看是否距上次执行任务超过60s

							// 循环，只要任务队列没有任务，那么wait_for 1秒，若被“有任务唤醒”则返回不超时，不满足if条件跳出if结构继续执行，也就是开启下一次while循环判断，因为有任务，所以task_que.size()一定>0，跳出while，开始取任务的操作；若是返回超时，那么判断空闲时间是否超过60s了，没超过就进入下一次等待资源1s的过程
						if (not_empty.wait_for(lock, std::chrono::seconds(1)) == std::cv_status::timeout)
						{
							auto now = std::chrono::high_resolution_clock().now();
							auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - last_time);
							if (dur.count() > THREAD_IDLE_TIME_LIMIT &&		// 60s，不要出现无意义数字，都用 const int 代替
								cur_thread_size > initial_thread_size)
							{
								// 回收当前线程-------------
								// 容器删除线程
								threads.erase(thread_id);
								// 更新线程数量
								cur_thread_size--;
								idle_thread_size--;
								// 打印日志
								std::cout << "> > > > thread " << std::this_thread::get_id() << " recalimed\n";
								return;
							}
						}

					}
					else	// 不是cached模式，则直接等待“有任务”资源
					{
						// 等待 not_empty 条件
						not_empty.wait(lock);
					}

					// 不管到了哪个模式，进行到这里都是被唤醒了。
					// 检查一下是被 有任务 唤醒，还是被线程池析构函数唤醒
					// 线程池析构函数会修改is_pool_runnig的值为false，线程池结束，则回收本线程的资源； 否则是被任务唤醒，去执行任务
					// if (is_pool_runnig == false)
					// {
					// 	// 回收当前线程-------------
					// 	threads.erase(thread_id);	// 容器删除线程		STL容器删除元素会自动调用元素的析构函数
					// 	std::cout << "> > > > thread " << std::this_thread::get_id() << " recalimed\n";		// 打印日志
					// 	all_thread_end.notify_all();		// 唤醒等待中的条件变量，让它检查有没有满足在等待的条件 -- 即线程池中一个线程也没有了，如果它发现没有满足，会继续等待，即本线程并不是最后一个被删除的线程
					// 	return;		// 既然线程池都要结束了，也没必要更新线程数量，回收资源即可，并return结束线程函数
					// }
					// 这种方式回收的线程是等待任务的线程，对于另一种正在执行任务的线程，它们执行完任务后会发现while大循环的条件--线程池在运行 已经不满足，跳出循环，所以在循环后再加上上述回收线程的语句即可
				}

				// !!! while 下就是纯纯的任务处理逻辑，其他操作放到 while 内
				idle_thread_size--;

				// 从任务队列的队头取一个任务  为什么要用队列的数据结构装任务：因为保证用户提交的相对公平，队列结构先进先出的结构是合适的
				task = task_que.front();
				task_que.pop();
				task_size--;
				std::cout << "tId: " << std::this_thread::get_id() << " 获取任务成功\n";

				// 如果取走任务后，任务队列依然有剩余任务，则继续通知其他的线程来取任务
				if (task_que.size() > 0)
				{
					not_empty.notify_all();
				}

				// 取出了一个任务，任务队列一定不满，唤醒因为队列满了进入等待队列的提交任务线程
				not_full.notify_all();
			}							// 在这里就应该把锁释放掉，执行任务的过程中还占有锁的所有权，其他线程一直在阻塞，这是不合理的。所以在这里设了一个作用域
										// 这也是多线程程序里只有一个线程在做事情的常见原因：开始执行事情的同时没有释放锁

			// 当前线程负责执行这个任务			这里运用的就是典型的多态设计，传入的任务类对象是基类的指针，调用的运行方法是虚函数，实际的实现是子类里实现的
			// task->run();		run 方法是纯虚函数，无法用来处理用户的返回值，所以把他封装在普通的成员方法exec()里
			// exec() :	 1. 执行任务； 2. 通过passValue方法把任务的返回值传递给Result
			// task->exec();
			// 执行封装的std::function<void ()>类型的 lambda函数
			task();

			// 任务处理完要进入等任务阶段了，这就是最后一次执行任务的时间，然后每1秒检查一次（更新1次现在的时间）是不是60s没有获得任务了，是就回收该线程
			last_time = std::chrono::high_resolution_clock().now();

			// 线程的任务处理完成，又变为空闲线程
			idle_thread_size++;
		}
	}

private:
	//std::vector< std::unique_ptr<Thread>> threads;			// 线程数组，存储线程的容器		有指针就要有析构，自己析构很麻烦，完全可以通过智能指针
	std::unordered_map<int, std::unique_ptr<Thread>> threads;	// 改成键值对的形式，通过id访问线程从而更精确的管理线程
	int initial_thread_size;				// 初始的线程数量	start()默认初始化为CPU核数，因为start()是使用线程池必须先调用的方法，所以在构造函数中专门初始化该值为0就行
	std::atomic_int idle_thread_size;		// 记录空闲线程的数量	只要是会被不同线程同时操作的变量，尽量使用原子类型
	int thread_size_upper_limit;			// 线程数量上限		cached模式下线程数量不能无休止地增加
	std::atomic_int cur_thread_size;		// 当前线程数量

	using Task = std::function<void()>;						// 在这个线程池的设计里，任务就是函数对象，返回值 void是将这个设为中间层，之后会根据实际类型进行处理，而参数用绑定器也可以处理
	std::queue<Task> task_que;			// 任务队列，为了实现多态，以任务基类的指针或引用初始化。
	/* 要考虑一种情况: 在submitTask(task) 中，若用户传入的是临时任务对象，则该行语句结束后该对象就被析构
	线程接收的指针指向被释放的内存，没有意义，所以形参设为Task对象的智能指针 -- 可以延长临时对象的生命周期到run方法结束后，并自动释放其资源 */
	std::atomic_int task_size;				// 任务的数量			知道一定不是负数就可以用unsigned int，多线程对任务数量的访问的互斥性可以用原子类型实现
	int task_size_upper_limit;				// 任务数量上限

	std::mutex task_que_mtx;				// 保证任务队列线程安全的互斥锁
	std::condition_variable not_full;		// 表示任务队列不满
	std::condition_variable not_empty;		// 表示任务队列不空
	std::condition_variable all_thread_end;		// 线程池析构，等待线程全部回收

	PoolMode pool_mode;						// 线程池的模式
	std::atomic_bool is_pool_runnig;		// 线程池是否在运行，放在线程池的成员函数里防止用户不开启线程池直接调用方法的误操作
};
// --------------------------------------- 线程池类 ---------------------------------------




#endif
