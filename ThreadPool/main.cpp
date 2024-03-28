#include "threadpool.h"

void Hello();

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
		int sum = 0;
		for (int i = begin; i <= end; i++)
			sum += i;

		std::this_thread::sleep_for( std::chrono::seconds(3));
		std::cout << "tId: " << std::this_thread::get_id() << " 执行结束\n";
		return sum;		// 返回值sum 传给类型Any 相当于查看Any类是否有一个sum类型参数的构造函数，若有，则以该参数构造
	}					// 而Any的构造函数以函数模板设计 template< typename T> \Any(T data_) ... 所以无论返回什么类型，Any都可以接收
private:
	int begin = 0;
	int end = 0;
};

void Hello()
{
	std::cout << "Hello ";
}


void Test1()
{
	ThreadPool thread_pool;
	thread_pool.start(4);

	Result res = thread_pool.submitTask(std::make_shared<TaskHello>(0, 4));
	// 如何设计这里的Result机制呢  -- 用return task->Result 这样的设计还是 return Result(task)，答案是后者，因为Task对象的生命周期在上面这一行就结束了，需要用后面的设计来延长到下面一行结束,具体的延长方式就是通过shared_ptr增加其引用计数，所以在Result类中设计了一个shared_ptr对象
	int sum = res.getAny().cast<int>();
	std::cout << "sum = " << sum;

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
	std::this_thread::sleep_for(std::chrono::seconds(10));
}

int main()
{
	Test1();
}