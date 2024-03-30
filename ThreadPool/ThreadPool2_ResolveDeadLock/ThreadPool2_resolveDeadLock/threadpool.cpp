// 需要设置项目属性里更高的C++版本，14好像不能在左值引用删除的情况下智慧匹配右值引用
#include "threadpool.h"

const int MAX_TASK_SIZE = INT32_MAX;
const int MAX_THREAD_SIZE = 100;
const int THREAD_IDLE_TIME_LIMIT = 60;

// -------------------------------------------------Task类-------------------------------------------------------
Task::Task()
	: result(nullptr)
{}

void Task::exec()
{
	if (result != nullptr)	// 加判断只是为了保险一些，产品上线可能出现各种问题
	{
		result->passValue(run());		// run方法会返回一个Any值，我们把它传递给Task类自己的Result
	}
}

void Task::setResult(Result* res_)
{
	result = res_;
}
// --------------------------------------------------------------------------------------------------------------

// -----------------------------------------------线程池类---------------------------------------------------------
// 线程池构造
ThreadPool::ThreadPool()
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
ThreadPool::~ThreadPool()
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
void ThreadPool::setMode(PoolMode mode)
{
	if (checkRunningState() == false)
	{
		std::cout << "Err: Please start the thread pool before you operating it.\n";
		return;
	}
	this->pool_mode = mode;
}

// 用户开启线程池方法
void ThreadPool::start(int init_thread_size)
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
void ThreadPool::setTaskSizeUpperLimit(int max_size)
{
	if (checkRunningState() == false)
	{
		std::cout << "Err: Please start the thread pool before you operating it.\n";
		return;
	}
	this->task_size_upper_limit = max_size;
}

// cached模式下，设置线程队列数量上限
void ThreadPool::setThreadSizeUpperLimit(int max_size)
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

// 用户给线程池提交任务
Result ThreadPool::submitTask(std::shared_ptr<Task> task)
{
	if (checkRunningState() == false)
	{
		std::cout << "Err: Please start the thread pool before you operating it.\n";
		return Result(nullptr);
	}

	// 1.获取锁
	std::unique_lock<std::mutex> lock(task_que_mtx);			// 防止多线程操作同一队列内存造成的线程安全问题

	// 2.线程通信：等待任务队列有空余 not_full.wait				// 不能让用户一直等待，所以用户提交任务，最长不能超过1s，超过则直接返回，提交任务失败
	if (not_full.wait_for(lock, std::chrono::seconds(1), [&]()->bool {							// 第二个参数传谓语，即返回值为 bool类型的函数或伪函数，满足时继续执行，不满足时线程进入等待状态
		return task_que.size() < task_size_upper_limit;
		}) == false)														// while (!_Pred()) {   wait(_Lck);	 }  转到定义，是用 while()循环实现的方法
	{	// 说明等待了 1s，条件仍然没有满足
		std::cerr << "[Err]: Task queue is full, the submit task operation failed.\n";
		return Result(task, false);
	}

		// 3.如果有空余，把任务放入任务队列
		task_que.emplace(task);										// emplace 底层调用的也是 emplace_back，如果不传参数与emplace_back功能一样，传参数应该是在某位置直接构造对象，其他元素后移的意思
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
			std::unique_ptr p_thread = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
			int thread_id = p_thread->getId();
			threads.emplace(thread_id, std::move(p_thread));
			// 现在是map存储线程了，可以通过线程id作为键，启动线程
			threads[thread_id]->start();
			// 修改相关资源数量
			cur_thread_size++;
			idle_thread_size++;
		}

		return Result(task);	// 默认 true
}

// 定义线程函数			从任务队列取任务并执行                                                                                                                                                                                                                                                                                
void ThreadPool::threadFunc(int thread_id)
{
	// 线程函数第一次调用的时间
	auto last_time = std::chrono::high_resolution_clock().now();

	// 若线程池是运行状态，那么线程函数循环的执行 从任务队列获取任务 -> 执行任务 的过程
	while (is_pool_runnig == true)
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
		std::this_thread::sleep_for(std::chrono::seconds(3));	// 在这里休眠一会，更容易发生死锁

		std::shared_ptr<Task> task = nullptr;
		{
			// 先获取锁		我在想 任务队列有一个任务，若两个线程同时检查到 task_que.size() > 0，跳过了while，直接去取任务，其中一个先取到，任务队列空了怎么办 -- 额，前面上锁了呀，只有抢到锁的才能判断是否有任务，另一个要被阻塞在线程函数开始位置的。其实这就是用互斥锁防止数据竞争问题的典型案例
			std::unique_lock<std::mutex> lock(task_que_mtx);

			// 针对线程池析构函数先获得锁的情况，is_pool_running的值已被改变，那么在这里的判断语句加上对 is_pool_runnig的判断，就可以跳出循环，而不会无限等待在循环里的两种wait里
			while (task_que.size() == 0 && is_pool_runnig == true)		// 没任务就等待，有任务即被唤醒时，wait_for和wait会停止等待进入下轮循环，发现不满足while的条件，跳出循环
			{
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

			// 若获得锁后不满足 while 种的is_pool_runnig == true，会跳出while到这里，发现不满足就直接跳出外层while循环进行线程回收的操作了，若是因为被有任务唤醒而不是线程池结束唤醒的会继续取任务执行
			if (is_pool_runnig == false)
			{
				break;
			}

			// 假设有两个线程同时被唤醒，wait操作会重新进行上锁操作，其中一个要被阻塞，抢到锁的会从上面的语句向下继续执行，那么该线程一定会拿到任务，空闲线程-1
			idle_thread_size--;

			// 从任务队列的队头取一个任务  我的理解，为什么要用队列的数据结构装任务：因为保证用户提交的相对公平，队列结构先进先出的结构是合适的
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
		task->exec();

		// 任务处理完要进入等任务阶段了，这就是最后一次执行任务的时间，然后每1秒检查一次（更新1次现在的时间）是不是60s没有获得任务了，是就回收该线程
		last_time = std::chrono::high_resolution_clock().now();

		// 线程的任务处理完成，又变为空闲线程
		idle_thread_size++;
	}

	// 跳出循环，说明不满足线程池正在运行状态的条件，那么回收该线程
	if (is_pool_runnig == false)
	{
		// 回收当前线程-------------
		threads.erase(thread_id);	// 容器删除线程			STL容器删除元素会自动调用元素的析构函数
		std::cout << "> > > > thread " << std::this_thread::get_id() << " recalimed\n";		// 打印日志
		all_thread_end.notify_all();		// 唤醒等待中的条件变量，让它检查有没有满足在等待的条件 -- 即线程池中一个线程也没有了，如果它发现没有满足，会继续等待，即本线程并不是最后一个被删除的线程
	}
	// 这里到了线程函数尾部，自动返回了，无需return
}

// 检查线程池的运行状态
bool ThreadPool::checkRunningState()
{
	return is_pool_runnig;
}
// ------------------------------------------------------------------------------------------------------------

// ------------------------------------------------线程类------------------------------------------------------
int Thread::generate_id = 0;

Thread::Thread(ThreadFunc func_)
	: func(func_)		// 线程对象构造时，用传入的线程池对象里的线程函数初始化自己的线程函数
	, thread_id(generate_id++)		// 在线程对象构造时，id就自动产生了
{}

Thread::~Thread()
{}

void Thread::start()
{									// start 让线程开始执行线程池传进来的线程函数
	std::thread T(func, thread_id);

	// 出了 start，对象T就析构了，所以要把线程设为分离线程，线程对象离开作用域析构后线程栈仍存在，线程函数仍然会继续运行
	T.detach();
}

int Thread::getId() const
{
	return thread_id;
}
// -------------------------------------------------------------------------------------------------------------

// ------------------------------------------------Result类-----------------------------------------------------
Result::Result(std::shared_ptr<Task> task_, bool is_valid_)
	: task(task_)
	, is_valid(is_valid_)
{
	task->setResult(this);
}

Any Result::getAny()					// 用户调用的
{
	if (is_valid == false)
		return "";

	sem.wait();				// 等待“用户提交的任务执行完成”这个资源
	return std::move(any);
}

void Result::passValue(Any any_)
{
	// 存储task的返回值 
	this->any = std::move(any_);
	// 已经获取了任务的返回值，“用户提交的任务执行完成”这个资源增加
	sem.post();
}
// --------------------------------------------------------------------------------------------------------------


