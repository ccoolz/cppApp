// 需要设置项目属性里更高的C++版本，14好像不能在左值引用删除的情况下智慧匹配右值引用
#include "threadpool.h"

const int MAX_TASK_SIZE = 1024;

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
	: pool_mode(PoolMode::MODE_FIXED)			// 默认为固定线程数量模式
	, initial_thread_size(0)					// 初始线程数量，start()方法中一定赋初值，这里设为0就行
	, task_size(0)								// 任务由用户提交，所以一开始没有任务
	, task_size_upper_limit( MAX_TASK_SIZE)		// 设定任务上限以免占用过多内存
{}

// 线程池析构
ThreadPool::~ThreadPool()
{}

// 设定线程池模式
void ThreadPool::setMode(PoolMode mode)
{
	this->pool_mode = mode;
}

// 用户开启线程池方法
void ThreadPool::start(size_t init_thread_size)
{
	// 记录初始化线程个数
	this->initial_thread_size = init_thread_size;

	// 创建初始数量的线程
	for (int i = 0; i < initial_thread_size; i++)
	{													// 创建 Thread线程对象的时候，通过绑定器把线程池类的线程函数给到 Thread类的线程对象
		std::unique_ptr<Thread> p_thread = std::make_unique<Thread>( std::bind(&ThreadPool::threadFunc, this));	// 普通指针是new，智能指针是 make_nuique / make_shared
		threads.emplace_back( std::move(p_thread));		// unique_ptr 禁用了左值引用，所以要么直接在 emplace_back里 make_unique，要么用std::move进行资源转移
		// emplace_back() 和 push_back() 的区别，就在于底层实现的机制不同。push_back() 向容器尾部添加元素时，首先会创建这个元素，然后再将这个元素拷贝或者移动到容器中（如果是拷贝的话，事后会自行销毁先前创建的这个元素）；
	}													// 而 emplace_back() 在实现时，则是直接在容器尾部创建这个元素，省去了拷贝或移动元素的过程。创建线程这么重的操作，先创建再拷贝显然是不合适的，再者thread本身也禁止了拷贝构造和赋值操作符，总上添加线程对象只能用 emplace_back()
	
	// 启动所有线程
	for (int i = 0; i < initial_thread_size; i++)
	{
		threads[i]->start();
	}
}

// 设置任务队列的数量上限
void ThreadPool::setTaskSizeUpperLimit(size_t max_size)
{
	this->task_size_upper_limit = max_size;
}

// 用户给线程池提交任务
Result ThreadPool::submitTask(std::shared_ptr<Task> task)
{
	// 1.获取锁
	std::unique_lock<std::mutex> lock(task_que_mtx);			// 防止多线程操作同一队列内存造成的线程安全问题

	// 2.线程通信：等待任务队列有空余 not_full.wait				// 不能让用户一直等待，所以用户提交任务，最长不能超过1s，超过则直接返回，提交任务失败
	if ( not_full.wait_for( lock, std::chrono::seconds(1), [&]()->bool {							// 第二个参数传谓语，即返回值为 bool类型的函数或伪函数，满足时继续执行，不满足时线程进入等待状态
		return task_que.size() < task_size_upper_limit;
	}) == false )														// while (!_Pred()) {   wait(_Lck);	 }  转到定义，是用 while()循环实现的方法
	{	// 说明等待了 1s，条件仍然没有满足
		std::cerr << "[Err]: Task queue is full, the submit task operation failed.\n";
		return Result(task, false);
	}

	// 3.如果有空余，把任务放入任务队列
	task_que.emplace(task);										// emplace 底层调用的也是 emplace_back，如果不传参数与emplace_back功能一样，传参数应该是在某位置直接构造对象，其他元素后移的意思
	task_size++;												// 为什么不直接调用 task_que.size()，因为这个变量后面会用到

	// 4.线程通信：新放了任务，队列一定不空 not_empty.notify   --   通知线程来执行任务
	not_empty.notify_all();

	return Result(task);	// 默认 true
}

// 定义线程函数			从任务队列取任务并执行                                                                                                                                                                                                                                                                                
void ThreadPool::threadFunc()
{
	// 线程函数循环的执行 从任务队列获取任务 -> 执行任务 的过程
	for ( ; ; )
	{
		std::shared_ptr<Task> task = nullptr;
		{
			// 先获取锁
			std::unique_lock<std::mutex> lock(task_que_mtx);

			// 等待 not_empty 条件
			not_empty.wait(lock, [&]() {
				return task_que.size() > 0;
				});

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
	}
}
// ------------------------------------------------------------------------------------------------------------

// ------------------------------------------------线程类------------------------------------------------------
Thread::Thread(ThreadFunc func_)
	: func( func_)					// 线程对象构造时，用传入的线程池对象里的线程函数初始化自己的线程函数
{}

Thread::~Thread() 
{}

void Thread::start()
{									// start 让线程开始执行线程池传进来的线程函数
	std::thread T( func);
	
	// 出了 start，对象T就析构了，所以要把线程设为分离线程，线程对象离开作用域析构后线程栈仍存在，线程函数仍然会继续运行
	T.detach();
}
// -------------------------------------------------------------------------------------------------------------

// ------------------------------------------------Result类-----------------------------------------------------
Result::Result( std::shared_ptr<Task> task_, bool is_valid_)
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


