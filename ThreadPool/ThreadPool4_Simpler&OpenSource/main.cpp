// 这一版要利用C++11及以后的其他线程相关api简化线程池代码
// 除此之外，这一版将线程池的定义和实现放在一个文件下，使用更方便，实现部分是暴露的

/*
1.	这一版舍弃上一版中线程函数的设计，直接将提交的任务作为线程函数，
	并传入所需参数 std::thread T(func1, arg1, ar2...)这可以让任务的提交更加方便灵活
	需要实现	pool.submitTask(sum1, 4, 5)		pool.submitTask(sum2, 4, 5, 6)		...
	可以用到可变参模板编程

2.	上一版我们自己实现的 Any和 Result联合完成的一些功能
	可以用C++11的 packaged_task和 future联合完成
	std::packaged_task：
		std::packaged_task 是一个模板类，用于包装一个可调用的目标（如函数、lambda 表达式等），以便可以在另一个线程中异步调用它。
		它与 std::future 一起使用，当你调用 std::packaged_task 的 operator() 时，它会执行包装的函数，并将结果存储在关联的 std::future 对象中。
		std::packaged_task 特别适合用于将一个任务包装起来，并在另一个线程中执行，然后在原线程中获取结果。
		std::packaged_task 内部可能管理着一些资源，如分配的内存、文件句柄等。如果允许拷贝构造和拷贝赋值
		那么这些资源就可能被多个 std::packaged_task 对象共享导致资源管理的复杂性增加
		并且可能出现双重删除（double delete）等错误。所以 packaged_task的拷贝构造和拷贝赋值函数被禁用，要用move
	std::future 的一些关键特性和用法包括：
		异步操作的结果：std::future 对象表示一个异步操作的结果。你可以通过调用 std::future::get() 方法来获取这个结果。如果异步操作还没有完成，get() 方法将阻塞调用线程，直到结果可用。
		可移动但不可复制：std::future 对象是不可复制的，但可以通过移动语义进行转移。这意味着你可以将一个 std::future 对象从一个线程移动到另一个线程，但你不能复制一个 std::future 对象。
		与 std::promise 配合使用：你可以使用 std::promise 对象来设置一个值或异常，并允许关联的 std::future 对象检索它。这提供了一种在线程间传递值或异常的机制。
		与 std::packaged_task 配合使用：std::packaged_task 是一种将可调用的目标（如函数或 lambda 表达式）包装起来以便异步执行的方式。当你创建一个 std::packaged_task 对象时，它返回一个与之关联的 std::future 对象，你可以使用这个 std::future 对象来检索异步操作的结果。
		与 std::async 配合使用：std::async 是一个函数模板，它在一个单独的线程中异步执行一个函数，并返回一个 std::future 对象，该对象表示函数的结果。
		异常处理：如果异步操作抛出了异常，并且该异常没有被捕获，则它将被存储在关联的 std::future 对象中。当你调用 std::future::get() 方法时，如果存储了异常，则该异常将被重新抛出。
	异步操作：在未来某个不确定的时间点完成某个任务
	使用实例：
		std::packaged_task<int(int, int)> task(sum1);
		std::future<int> result = task.get_future();
		packaged_task 顾名思义，将某函数打包，通过 packaged_task.get_future对象可以获取future<T>对象
		future对象的 get方法可获取任务执行完成的返回值
*/
#include "threadpool.hpp"

int sum1(int a, int b)
{
	std::this_thread::sleep_for( std::chrono::seconds(2));
	return a + b;
}

int sum2(int a, int b, int c)
{
	std::this_thread::sleep_for(std::chrono::seconds(2));
	return a + b + c;
}

int main()
{
	// 启动线程池对象
	ThreadPool thread_pool;
	thread_pool.start(4);

	// 提交任务，线程池自动取任务执行5
	std::future<int> result1 = thread_pool.submitTask(sum1, 1, 2);
	std::future<int> result2 = thread_pool.submitTask(sum2, 1, 2, 3);
	std::future<int> result3 = thread_pool.submitTask(sum2, 1, 2, 3);
	std::future<int> result4 = thread_pool.submitTask(sum1, 1, 2);
	std::future<int> result5 = thread_pool.submitTask(sum1, 1, 2);
	std::future<int> result6 = thread_pool.submitTask(sum1, 1, 2);
	std::future<int> result7 = thread_pool.submitTask(sum1, 1, 2);
	std::future<int> result8 = thread_pool.submitTask(sum1, 1, 2);
	std::future<int> result9 = thread_pool.submitTask(sum1, 1, 2);

	std::cout << "result1 = " << result1.get() << "\n";
	std::cout << "result2 = " << result2.get() << "\n";
	std::cout << "result3 = " << result3.get() << "\n";
	std::cout << "result4 = " << result4.get() << "\n";
	std::cout << "result5 = " << result5.get() << "\n";
	std::cout << "result6 = " << result6.get() << "\n";
	std::cout << "result7 = " << result7.get() << "\n";
	std::cout << "result8 = " << result8.get() << "\n";
	std::cout << "result9 = " << result9.get() << "\n";

}