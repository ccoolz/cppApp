/* 第三版
 在自己用时，根据实际需求设计线程池对象出作用域后是否马上析构（任务可能还没有执行，线程就退出了），还是等任务执行完再析构
 但在通用线程池设计中，用户可能不会考虑线程池的作用域问题，那么不执行提交的任务就退出是不合适的
 所以在这一版，我们继续进行优化，改变回收资源的策略，主要改变的逻辑是 --  for 循环内嵌套while判断，有任务直接跳过其他操作，优先执行任务，没任务才能进行回收线程或等待任务的操作
 主要更新了 threadFunc函数
 */
#include "threadpool.h"

typedef unsigned long long uLong;

void Hello()
{
	std::cout << "Hello ";
}

class TaskHello : public Task
{
public:
	TaskHello() = default;

	TaskHello(int begin_, int end_)
		: begin(begin_), end(end_)
	{}

	// 问题一：怎么设计run的返回值，可以接收任意的类型呢？用模板是不行的，run是虚函数，编译器就要确定虚函数表，而模板是运行时确定类型并替换。
	// C++17 的Any类型就可以。那么我们自己写个Any试试
	Any run()
	{
		std::cout << "tId: " << std::this_thread::get_id() << " 开始执行\n";
		Hello();
		std::cout << "thread " << std::this_thread::get_id() << "\n";
		uLong sum = 0;
		for (int i = begin; i <= end; i++)
			sum += i;

		std::this_thread::sleep_for(std::chrono::seconds(3));
		std::cout << "tId: " << std::this_thread::get_id() << " 执行结束\n";
		return sum;		// 返回值sum 传给类型Any 相当于查看Any类是否有一个sum类型参数的构造函数，若有，则以该参数构造
	}					// 而Any的构造函数以函数模板设计 template< typename T> \Any(T data_) ... 所以无论返回什么类型，Any都可以接收
private:
	int begin = 0;
	int end = 0;
};

void test1()
{
	ThreadPool thread_pool;
	thread_pool.start(4);

	// Master - Slave 线程模型
	// Master线程用来分解任务，然后给各个slave线程分配任务
	// 等待各个slave线程执行完任务，返回结果
	// Master线程合并各个任务结果，输出
	Result res1 = thread_pool.submitTask(std::make_shared<TaskHello>(0, 100000000));
	Result res2 = thread_pool.submitTask(std::make_shared<TaskHello>(100000001, 200000000));
	Result res3 = thread_pool.submitTask(std::make_shared<TaskHello>(200000001, 300000000));
	// 如何设计这里的Result机制呢  -- 用return task->Result 这样的设计还是 return Result(task)，答案是后者，因为Task对象的生命周期在上面这一行就结束了，需要用后面的设计来延长到下面一行结束,具体的延长方式就是通过shared_ptr增加其引用计数，所以在Result类中设计了一个shared_ptr对象
	uLong sum1 = res1.getAny().cast<uLong>();		// !Bug: 未经处理的异常: Microsoft C++ 异常: char，位于内存位置 0x0133FAD  主要原因是这里cast<int>里忘了改成cast<uLong>
	uLong sum2 = res2.getAny().cast<uLong>();
	uLong sum3 = res3.getAny().cast<uLong>();
	std::cout << "sum = " << sum1 + sum2 + sum3 << "\n";

	//thread_pool.submitTask(std::make_shared<TaskHello>());		// 可以直接传任务的匿名对象

	//std::shared_ptr<TaskHello> HelloTask = std::make_shared<TaskHello>();
	//thread_pool.submitTask(HelloTask);							// 也可以传入实现创建好的任务对象
	//thread_pool.submitTask(HelloTask);
	//thread_pool.submitTask(HelloTask);
	//thread_pool.submitTask(HelloTask);
	//thread_pool.submitTask(HelloTask);
	//thread_pool.submitTask(HelloTask);
	//thread_pool.submitTask(HelloTask);		// 8个任务提交，两条进入队列被两个线程取走，又两个进入被另外两个取走，又两个进入因没有空闲线程
	//thread_pool.submitTask(HelloTask);

	// 因为线程是detach的，所以主程序运行完，线程资源自动回收，线程函数还没来得及运行完，这不是设计的问题，实际应用中，这个服务就是持续运行的，加上休眠时间测试一下
	// std::this_thread::sleep_for(std::chrono::seconds(5));
	getchar();
}

void test2()
{
	ThreadPool thread_pool;
	thread_pool.start(4);
	thread_pool.setMode(PoolMode::MODE_CACHED);

	thread_pool.submitTask(std::make_shared<TaskHello>(0, 100000000));
	thread_pool.submitTask(std::make_shared<TaskHello>(0, 100000000));
	thread_pool.submitTask(std::make_shared<TaskHello>(0, 100000000));
	thread_pool.submitTask(std::make_shared<TaskHello>(0, 100000000));
	// 初始4条线程，将上面四个任务取走，任务中有延时，不会很快执行完。那么MODE_CACHED下我们预期会创建两条新线程来接下面两个任务
	thread_pool.submitTask(std::make_shared<TaskHello>(0, 100000000));
	thread_pool.submitTask(std::make_shared<TaskHello>(0, 100000000));

	getchar();
}

void test3()
{
	ThreadPool thread_pool;
	thread_pool.start();
	thread_pool.setMode(PoolMode::MODE_CACHED);

	thread_pool.submitTask(std::make_shared<TaskHello>(0, 100000000));
	thread_pool.submitTask(std::make_shared<TaskHello>(0, 100000000));
	thread_pool.submitTask(std::make_shared<TaskHello>(0, 100000000));
	thread_pool.submitTask(std::make_shared<TaskHello>(0, 100000000));

	thread_pool.submitTask(std::make_shared<TaskHello>(0, 100000000));
	thread_pool.submitTask(std::make_shared<TaskHello>(0, 100000000));
	thread_pool.submitTask(std::make_shared<TaskHello>(0, 100000000));
	thread_pool.submitTask(std::make_shared<TaskHello>(0, 100000000));

	thread_pool.submitTask(std::make_shared<TaskHello>(0, 100000000));
	thread_pool.submitTask(std::make_shared<TaskHello>(0, 100000000));
	thread_pool.submitTask(std::make_shared<TaskHello>(0, 100000000));
	thread_pool.submitTask(std::make_shared<TaskHello>(0, 100000000));
	// 加上下面的任务，共13个任务。
	// 测试 std::thread::hardware_concurrency()返回的处理器数量是否正确(12个)，若正确，运行test3会创建一条新线程来处理第13个任务
	thread_pool.submitTask(std::make_shared<TaskHello>(0, 100000000));

	getchar();
}

void test4()
{
	// 项目中遇到的死锁问题			-- 产生原因见 ThreadFunc()中的 while处
	int i = 10;
	while (i--)
	{
		ThreadPool thread_pool;
		thread_pool.start(4);

		Result res = thread_pool.submitTask(std::make_shared<TaskHello>(0, 100000000));
		uLong sum = res.getAny().cast<uLong>();
		std::cout << "sum = " << sum << std::endl;
	}
	std::cout << "MAIN END\n";
}

void test5()
{
	// 测试出作用子线程是否不执行任务直接退出
	// 预期执行完任务才退出，那么主线程也要等待子线程的休眠时间
	ThreadPool thread_pool;
	thread_pool.start(4);

	thread_pool.submitTask(std::make_shared<TaskHello>(0, 100000000));
	thread_pool.submitTask(std::make_shared<TaskHello>(0, 100000000));
	thread_pool.submitTask(std::make_shared<TaskHello>(0, 100000000));
	thread_pool.submitTask(std::make_shared<TaskHello>(0, 100000000));
	// 不调用get获取结果，就不会被自动阻塞直到获取结果，这样如果我们是不等待任务执行完就析构的之前两版线程池，就会直接退出子线程
	// uLong sum = res.getAny().cast<uLong>();
	// 也不会 get()方法阻塞，就看会不会等待任务执行完
	// 把这个代码拿到前面两版线程池，看看效果是不是不一样
}

int main()
{
	test5();
}