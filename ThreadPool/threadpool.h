/* 本多线程池库的基本使用示例 Example -------

ThreadPool pool;		// 创建线程池对象
pool.start(5);			// 创建你传入数量的线程，开启线程池（默认线程数是4）

class MyTask : public Task	// 你需要创建自己的任务类继承此库的任务基类，然后重写基类的run方法，也就是你要提交的函数的运行方法
{
public:
	void run() { 自定义... }
}

pool.submitTask(std::makeshared<MyTask> )	// 传入你要多线程运行的函数，线程会自动获取并运行

------------------------------------------ */

// #pragma once			// 使头文件不会被重复包含，不过并不是所有编译器都支持，下面写一个通用的
#ifndef THREADPOOL_H	// 虽然是语言级别的，编译器通用，但是也有编译时间更长的缺点 -- 每次都要打开头文件进行检查
#define THREADPOOL_H
#include <iostream>
#include <vector>
#include <queue>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <thread>

// 任务抽象基类		多态思想: 让线程可以接收不同类型的任务（基类指针），派生类重写基类的run方法（指向派生类对象），实现使用相同方法对不同类型任务的不同处理
class Result;
class Any;
class Task
{
public:
	Task();
	~Task() = default;

	//exec() :	 1. 执行任务； 2. 通过passValue方法把任务的返回值传递给Result
	void exec();

	void setResult(Result* res_);

	virtual Any run() = 0;
private:
	Result* result;		// 不能用智能指针，因为Result类里有一个Task智能指针成员，智能指针互相指向可能出现交叉引用的死锁问题。而且在这里我们知道result的生命周期 > task对象（task生命只在提交任务的那一行），所以用裸指针就行
};

// 线程池模式枚举	合理地使用枚举，可以提高代码的可读性和可维护性，使得程序更加清晰易懂
enum class PoolMode		// 之前的枚举使用枚举型不用加枚举类型，导致可能出现不同枚举里的同名枚举项问题，新标准里可以用 enum class防范这个问题。使用 enum class需要用作用域访问枚举项
{
	MODE_FIXED,		// 固定线程数量模式，依机器核数而定
	MODE_CACHED		// 线程数量可动态增长
};

// 线程类
class Thread
{
	using ThreadFunc = std::function<void()>;
public:
	Thread(ThreadFunc func_);

	~Thread();

	void start();	// 启动线程
private:
	ThreadFunc func;
};

// 线程池类
class ThreadPool
{
public:
	ThreadPool();

	~ThreadPool();

	void setMode(PoolMode mode);		// 设置线程池的模式

	void start(size_t init_thread_size = 4);		// 开启线程池，设定默认的初始线程数量为4，用户可以传入参数改变该值

	void setTaskSizeUpperLimit(size_t max_size);		// 设置任务队列数量上限

	Result submitTask(std::shared_ptr<Task> task);	// 用户给线程池提交任务

	ThreadPool(const ThreadPool& thread_pool) = delete;		// 禁用拷贝构造

	ThreadPool& operator=(const ThreadPool& thread_pool) = delete;		// 禁用赋值
			// 用户可以创建线程池对象，不过线程池对象涉及了很多内容，锁，容器.. 最好禁用对线程池对象的拷贝行为
private:
	void threadFunc();		// 定义线程函数
	
private:
	std::vector< std::unique_ptr<Thread>> threads;			// 线程数组，存储线程的容器		有指针就要有析构，自己析构很麻烦，完全可以通过智能指针
	size_t initial_thread_size;				// 初始的线程数量	start()默认初始化为CPU核数，因为start()是使用线程池必须先调用的方法，所以在构造函数中专门初始化该值为0就行
	PoolMode pool_mode;						// 线程池的模式

	std::queue< std::shared_ptr<Task>> task_que;			// 任务队列，为了实现多态，以任务基类的指针或引用初始化。
				/* 要考虑一种情况: 在submitTask(task) 中，若用户传入的是临时任务对象，则该行语句结束后该对象就被析构
				线程接收的指针指向被释放的内存，没有意义，所以形参设为Task对象的智能指针 -- 可以延长临时对象的生命周期到run方法结束后，并自动释放其资源 */
	std::atomic_uint task_size;				// 任务的数量			知道一定不是负数就可以用unsigned int，多线程对任务数量的访问的互斥性可以用原子类型实现
	size_t task_size_upper_limit;			// 任务数量上限
	
	std::mutex task_que_mtx;				// 保证任务队列线程安全的互斥锁
	std::condition_variable not_full;		// 表示任务队列不满
	std::condition_variable not_empty;		// 表示任务队列不空
};

// 自己创建的Any类型，可以接收任意数据的类型 (模板类的代码都只能写在一个头文件中被包含，不能分文件编写声明和定义)
class Any
{
public:
	Any() = default;
	~Any() = default;
	Any(const Any&) = delete;	// 它的成员变量base是用unique_ptr构建的，unique_ptr同一时间只能指向一个对象，所以禁用了左值引用的拷贝构造和赋值操作符，作为包含这个成员变量的Any，当然也要禁用左值引用 -- 左值引用了类型，其中的成员变量自然也一起被左值引用了
	Any& operator=(const Any&) = delete;
	Any(Any&&) = default;		// 右值引用默认，那么每个成员的右值引用就继续保持成员自己的右值引用方式
	Any& operator=(Any&&) = default;	// 实际上，上面四行不写，默认的调用到成员也会报错，只是加强印象和尽量使代码逻辑完美

	// 这个构造函数模板可以让Any接收任意类型的数据，保存在基类Base指针指向的成员类Derived的泛型成员data里
	template<typename T>
	Any(T data_)
		: base( std::make_unique<Derived<T>>(data_))
	{}
	/*	在以Any作为返回值类型，接收到一个返回值，就会以该值进行 Any类型的构造，base是Any的成员变量，类型是Base指针，也就是基类指针
		传入值作为派生类的构造函数的参数，进行派生类对象的构造，派生类是类模板，传入值被赋给派生类的成员 - 泛型的data

	*/

	// 这个方法能把保存的data提取出来
	template<typename T>  // 用户传入什么类型，就转换成什么类型的意思
	T cast()
	{
		// 我们要通过 Any类的成员base指针找到它指向的派生类对象，从里面取出存储的data变量
		// 基类指针 转成 派生类的指针，向下转，要想成功，这个基类指针需要确确实实指向一个派生类对象
		// 四种类型强转方法中，dynamic_cast是支持RTTI类型识别的  ---  RTTI是运行阶段类型识别（Runtime Type Identification）的简称。RTTI旨在为程序再运行阶段确定对象的类型提供一种标准方式。
		// 如果可能的话，dynamic_cast运算符将使用一个指向基类的指针来生成一个指向派生类的指针；否则，该运算符返回0（空指针）；
		// 这是最常用的RTTI组件，它不能回答“指针指向的是哪类对象”这样的问题，但能够回答“是否可以安全地将对象的地址赋给特定类型的指针”这样的问题。
		// 通常，如果指向的对象（*pt）的类型为Type或者是从Type直接或简介派生而来的类型，则下面的表达式将指针pt转换为Type类型的指针：
		// dynamic_cast<Type*>(pt)	否则，结果为0，即空指针。 即发现不可以转换时返回空指针
		Derived<T>* p_d = dynamic_cast<Derived<T>*>(base.get());	  // 智能指针的get方法，可以获取智能指针里存储的裸指针。方便这里的类型转换
		
		if (p_d == nullptr)		// 为空，说明转换失败，那么用户想要cast的类型与他一开始返回的类型是不匹配的
		{
			throw "Err: The type you want to cast does not match the return type of the task you submitted!";	// Bug: 这里没加;........
		}																										// 与模板有关的模块的错误没有具体提示，需要写的更加细致
		// 若成功转换，返回派生类的data
		return p_d->data;
	}

private:
	// 基类
	class Base
	{
	public:
		virtual ~Base() = default;		// 差不多是 !Base() {} 也就是函数体为空，不过在新版里编译器可以对default语法做更多优化
	};

	// 派生类		用模板类，泛型的data包含在类里
	template< typename T>
	class Derived : public Base
	{
	public:                                      
		Derived(T data_)
			: data(data_)
		{}
	
		T data;
	};

private:
	// 基类指针
	std::unique_ptr<Base> base;
};

// 通过互斥锁和条件变量自己实现一个信号量类  --  为了满足主线程中使用 Result类型的.get方法阻塞到子线程结束再获取值的这个时序关系而设计
class Semaphore
{
public:
	Semaphore(int sem_num_ = 0)
		: sem_num(sem_num_)
	{}

	~Semaphore() = default;

	// 获取一个信号量资源
	void wait()
	{
		std::unique_lock<std::mutex> lock(mtx);
		// 等待信号量有资源这个条件，没资源则阻塞
		condition.wait(lock, [&]()->bool { return sem_num > 0; });
		sem_num--;
	}

	// 增加一个信号量资源
	void post()
	{
		std::unique_lock<std::mutex> lock(mtx);
		sem_num++;
		condition.notify_all();
	}

private:
	std::mutex mtx;
	std::condition_variable condition;
	int sem_num;							// 因为信号量不像锁是单一资源，它可以有多个，所以需要资源计数变量来辅助实现
};

// 实现接收用户提交任务完成后的返回值类型Result
class Result
{
public:
	Result(std::shared_ptr<Task> task_, bool is_valid_ = true);
	~Result() = default;

	void passValue(Any any_);

	Any getAny();	// 用户调用这个方法获得提交任务完成后的返回值

private:
	Any any;		// 存储任务的返回值
	Semaphore sem;	// 线程通信信号量		-- 在分离线程没返回之前，需要阻塞住，既然是两个不同线程，当然需要线程通信，而且仅仅满足一前一后的效果即可，最合适的就是用信号量
	std::shared_ptr<Task> task;		// 指向对应获取返回值的任务对象
	std::atomic_bool is_valid;		// 返回值是否有效 -- 若提交任务失败了，则无效。有效才对返回值过程进行阻塞
};

#endif

