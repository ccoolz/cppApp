/* �����̳߳ؿ�Ļ���ʹ��ʾ�� Example -------

ThreadPool pool;		// �����̳߳ض���
pool.start(5);			// �����㴫���������̣߳������̳߳أ�Ĭ���߳�����4��

class MyTask : public Task	// ����Ҫ�����Լ���������̳д˿��������࣬Ȼ����д�����run������Ҳ������Ҫ�ύ�ĺ��������з���
{
public:
	void run() { �Զ���... }
}

pool.submitTask(std::makeshared<MyTask> )	// ������Ҫ���߳����еĺ������̻߳��Զ���ȡ������

------------------------------------------ */

// #pragma once			// ʹͷ�ļ����ᱻ�ظ��������������������б�������֧�֣�����дһ��ͨ�õ�
#ifndef THREADPOOL_H	// ��Ȼ�����Լ���ģ�������ͨ�ã�����Ҳ�б���ʱ�������ȱ�� -- ÿ�ζ�Ҫ��ͷ�ļ����м��
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

// ����������		��̬˼��: ���߳̿��Խ��ղ�ͬ���͵����񣨻���ָ�룩����������д�����run������ָ����������󣩣�ʵ��ʹ����ͬ�����Բ�ͬ��������Ĳ�ͬ����
class Result;
class Any;
class Task
{
public:
	Task();
	~Task() = default;

	//exec() :	 1. ִ������ 2. ͨ��passValue����������ķ���ֵ���ݸ�Result
	void exec();

	void setResult(Result* res_);

	virtual Any run() = 0;
private:
	Result* result;		// ����������ָ�룬��ΪResult������һ��Task����ָ���Ա������ָ�뻥��ָ����ܳ��ֽ������õ��������⡣��������������֪��result���������� > task����task����ֻ���ύ�������һ�У�����������ָ�����
};

// �̳߳�ģʽö��	�����ʹ��ö�٣�������ߴ���Ŀɶ��ԺͿ�ά���ԣ�ʹ�ó�����������׶�
enum class PoolMode		// ֮ǰ��ö��ʹ��ö���Ͳ��ü�ö�����ͣ����¿��ܳ��ֲ�ͬö�����ͬ��ö�������⣬�±�׼������� enum class����������⡣ʹ�� enum class��Ҫ�����������ö����
{
	MODE_FIXED,		// �̶��߳�����ģʽ����������������
	MODE_CACHED		// �߳������ɶ�̬����
};

// �߳���
class Thread
{
	using ThreadFunc = std::function<void()>;
public:
	Thread(ThreadFunc func_);

	~Thread();

	void start();	// �����߳�
private:
	ThreadFunc func;
};

// �̳߳���
class ThreadPool
{
public:
	ThreadPool();

	~ThreadPool();

	void setMode(PoolMode mode);		// �����̳߳ص�ģʽ

	void start(size_t init_thread_size = 4);		// �����̳߳أ��趨Ĭ�ϵĳ�ʼ�߳�����Ϊ4���û����Դ�������ı��ֵ

	void setTaskSizeUpperLimit(size_t max_size);		// �������������������

	Result submitTask(std::shared_ptr<Task> task);	// �û����̳߳��ύ����

	ThreadPool(const ThreadPool& thread_pool) = delete;		// ���ÿ�������

	ThreadPool& operator=(const ThreadPool& thread_pool) = delete;		// ���ø�ֵ
			// �û����Դ����̳߳ض��󣬲����̳߳ض����漰�˺ܶ����ݣ���������.. ��ý��ö��̳߳ض���Ŀ�����Ϊ
private:
	void threadFunc();		// �����̺߳���
	
private:
	std::vector< std::unique_ptr<Thread>> threads;			// �߳����飬�洢�̵߳�����		��ָ���Ҫ���������Լ��������鷳����ȫ����ͨ������ָ��
	size_t initial_thread_size;				// ��ʼ���߳�����	start()Ĭ�ϳ�ʼ��ΪCPU��������Ϊstart()��ʹ���̳߳ر����ȵ��õķ����������ڹ��캯����ר�ų�ʼ����ֵΪ0����
	PoolMode pool_mode;						// �̳߳ص�ģʽ

	std::queue< std::shared_ptr<Task>> task_que;			// ������У�Ϊ��ʵ�ֶ�̬������������ָ������ó�ʼ����
				/* Ҫ����һ�����: ��submitTask(task) �У����û����������ʱ��������������������ö���ͱ�����
				�߳̽��յ�ָ��ָ���ͷŵ��ڴ棬û�����壬�����β���ΪTask���������ָ�� -- �����ӳ���ʱ������������ڵ�run���������󣬲��Զ��ͷ�����Դ */
	std::atomic_uint task_size;				// ���������			֪��һ�����Ǹ����Ϳ�����unsigned int�����̶߳����������ķ��ʵĻ����Կ�����ԭ������ʵ��
	size_t task_size_upper_limit;			// ������������
	
	std::mutex task_que_mtx;				// ��֤��������̰߳�ȫ�Ļ�����
	std::condition_variable not_full;		// ��ʾ������в���
	std::condition_variable not_empty;		// ��ʾ������в���
};

// �Լ�������Any���ͣ����Խ����������ݵ����� (ģ����Ĵ��붼ֻ��д��һ��ͷ�ļ��б����������ܷ��ļ���д�����Ͷ���)
class Any
{
public:
	Any() = default;
	~Any() = default;
	Any(const Any&) = delete;	// ���ĳ�Ա����base����unique_ptr�����ģ�unique_ptrͬһʱ��ֻ��ָ��һ���������Խ�������ֵ���õĿ�������͸�ֵ����������Ϊ���������Ա������Any����ȻҲҪ������ֵ���� -- ��ֵ���������ͣ����еĳ�Ա������ȻҲһ����ֵ������
	Any& operator=(const Any&) = delete;
	Any(Any&&) = default;		// ��ֵ����Ĭ�ϣ���ôÿ����Ա����ֵ���þͼ������ֳ�Ա�Լ�����ֵ���÷�ʽ
	Any& operator=(Any&&) = default;	// ʵ���ϣ��������в�д��Ĭ�ϵĵ��õ���ԱҲ�ᱨ��ֻ�Ǽ�ǿӡ��;���ʹ�����߼�����

	// ������캯��ģ�������Any�����������͵����ݣ������ڻ���Baseָ��ָ��ĳ�Ա��Derived�ķ��ͳ�Աdata��
	template<typename T>
	Any(T data_)
		: base( std::make_unique<Derived<T>>(data_))
	{}
	/*	����Any��Ϊ����ֵ���ͣ����յ�һ������ֵ���ͻ��Ը�ֵ���� Any���͵Ĺ��죬base��Any�ĳ�Ա������������Baseָ�룬Ҳ���ǻ���ָ��
		����ֵ��Ϊ������Ĺ��캯���Ĳ������������������Ĺ��죬����������ģ�壬����ֵ������������ĳ�Ա - ���͵�data

	*/

	// ��������ܰѱ����data��ȡ����
	template<typename T>  // �û�����ʲô���ͣ���ת����ʲô���͵���˼
	T cast()
	{
		// ����Ҫͨ�� Any��ĳ�Աbaseָ���ҵ���ָ�����������󣬴�����ȡ���洢��data����
		// ����ָ�� ת�� �������ָ�룬����ת��Ҫ��ɹ����������ָ����Ҫȷȷʵʵָ��һ�����������
		// ��������ǿת�����У�dynamic_cast��֧��RTTI����ʶ���  ---  RTTI�����н׶�����ʶ��Runtime Type Identification���ļ�ơ�RTTIּ��Ϊ���������н׶�ȷ������������ṩһ�ֱ�׼��ʽ��
		// ������ܵĻ���dynamic_cast�������ʹ��һ��ָ������ָ��������һ��ָ���������ָ�룻���򣬸����������0����ָ�룩��
		// ������õ�RTTI����������ܻش�ָ��ָ���������������������⣬���ܹ��ش��Ƿ���԰�ȫ�ؽ�����ĵ�ַ�����ض����͵�ָ�롱���������⡣
		// ͨ�������ָ��Ķ���*pt��������ΪType�����Ǵ�Typeֱ�ӻ����������������ͣ�������ı��ʽ��ָ��ptת��ΪType���͵�ָ�룺
		// dynamic_cast<Type*>(pt)	���򣬽��Ϊ0������ָ�롣 �����ֲ�����ת��ʱ���ؿ�ָ��
		Derived<T>* p_d = dynamic_cast<Derived<T>*>(base.get());	  // ����ָ���get���������Ի�ȡ����ָ����洢����ָ�롣�������������ת��
		
		if (p_d == nullptr)		// Ϊ�գ�˵��ת��ʧ�ܣ���ô�û���Ҫcast����������һ��ʼ���ص������ǲ�ƥ���
		{
			throw "Err: The type you want to cast does not match the return type of the task you submitted!";	// Bug: ����û��;........
		}																										// ��ģ���йص�ģ��Ĵ���û�о�����ʾ����Ҫд�ĸ���ϸ��
		// ���ɹ�ת���������������data
		return p_d->data;
	}

private:
	// ����
	class Base
	{
	public:
		virtual ~Base() = default;		// ����� !Base() {} Ҳ���Ǻ�����Ϊ�գ��������°�����������Զ�default�﷨�������Ż�
	};

	// ������		��ģ���࣬���͵�data����������
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
	// ����ָ��
	std::unique_ptr<Base> base;
};

// ͨ�������������������Լ�ʵ��һ���ź�����  --  Ϊ���������߳���ʹ�� Result���͵�.get�������������߳̽����ٻ�ȡֵ�����ʱ���ϵ�����
class Semaphore
{
public:
	Semaphore(int sem_num_ = 0)
		: sem_num(sem_num_)
	{}

	~Semaphore() = default;

	// ��ȡһ���ź�����Դ
	void wait()
	{
		std::unique_lock<std::mutex> lock(mtx);
		// �ȴ��ź�������Դ���������û��Դ������
		condition.wait(lock, [&]()->bool { return sem_num > 0; });
		sem_num--;
	}

	// ����һ���ź�����Դ
	void post()
	{
		std::unique_lock<std::mutex> lock(mtx);
		sem_num++;
		condition.notify_all();
	}

private:
	std::mutex mtx;
	std::condition_variable condition;
	int sem_num;							// ��Ϊ�ź����������ǵ�һ��Դ���������ж����������Ҫ��Դ��������������ʵ��
};

// ʵ�ֽ����û��ύ������ɺ�ķ���ֵ����Result
class Result
{
public:
	Result(std::shared_ptr<Task> task_, bool is_valid_ = true);
	~Result() = default;

	void passValue(Any any_);

	Any getAny();	// �û����������������ύ������ɺ�ķ���ֵ

private:
	Any any;		// �洢����ķ���ֵ
	Semaphore sem;	// �߳�ͨ���ź���		-- �ڷ����߳�û����֮ǰ����Ҫ����ס����Ȼ��������ͬ�̣߳���Ȼ��Ҫ�߳�ͨ�ţ����ҽ�������һǰһ���Ч�����ɣ�����ʵľ������ź���
	std::shared_ptr<Task> task;		// ָ���Ӧ��ȡ����ֵ���������
	std::atomic_bool is_valid;		// ����ֵ�Ƿ���Ч -- ���ύ����ʧ���ˣ�����Ч����Ч�ŶԷ���ֵ���̽�������
};

#endif

