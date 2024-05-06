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

// �̳߳�ģʽö��
enum class PoolMode		// ֮ǰ��ö��ʹ��ö���Ͳ��ü�ö�����ͣ����¿��ܳ��ֲ�ͬö�����ͬ��ö�������⣬�±�׼������� enum class����������⡣ʹ�� enum class��Ҫ�����������ö����
{
	MODE_FIXED,			// �̶��߳�����ģʽ����������������		-- �����������ʱ������
	MODE_CACHED			// �߳������ɶ�̬����					-- ����������С���������������ȽϽ���
};

// --------------------------------------- �߳��� ---------------------------------------
class Thread
{
	using ThreadFunc = std::function<void(int)>;
public:
	Thread(ThreadFunc func_)
		: func(func_)		// �̶߳�����ʱ���ô�����̳߳ض�������̺߳�����ʼ���Լ����̺߳���
		, thread_id(generate_id++)		// ���̶߳�����ʱ��id���Զ�������
	{}

	~Thread()
	{}

	void start()
	{									// start ���߳̿�ʼִ���̳߳ش��������̺߳���
		std::thread T(func, thread_id);

		// ���� start������T�������ˣ�����Ҫ���߳���Ϊ�����̣߳��̶߳����뿪�������������߳�ջ�Դ��ڣ��̺߳�����Ȼ���������
		T.detach();
	}

	int getId() const
	{
		return thread_id;
	}
private:
	ThreadFunc func;

	static int generate_id;		// �߳�id�Ƕ�������ν��ֻҪ��һ�����Դ�������߳̾��У�����������仯�����߳�id
	int thread_id;				// �����߳�id
};

// ��̬��Ա���������������ʼ������Ϊ���ڳ�ʼ����ʹÿ�����󶼰��������Ա�������������ʼ�����������ڵ�һ�ε��øñ���ʱ���ܻ��ʼ��Ϊ0
int Thread::generate_id = 0;	
// --------------------------------------- �߳��� ---------------------------------------

// --------------------------------------- �̳߳��� ---------------------------------------
// �̳߳���
class ThreadPool
{
public:
	// �̳߳ع���
	ThreadPool()
		: pool_mode(PoolMode::MODE_FIXED)				// Ĭ��Ϊ�̶��߳�����ģʽ
		, initial_thread_size(0)						// ��ʼ�߳�������start()������һ������ֵ��������Ϊ0����
		, idle_thread_size(0)							// ��¼�����̵߳�����
		, cur_thread_size(0)							// ��ǰ�߳�����
		, thread_size_upper_limit(MAX_THREAD_SIZE)		// �߳��������ޣ�����ֹ���ӳ�������
		, task_size(0)									// �������û��ύ������һ��ʼû������
		, is_pool_runnig(false)							// ����start()�����ſ�����Ϊtrue
		, task_size_upper_limit(MAX_TASK_SIZE)			// �趨������������ռ�ù����ڴ�
	{}

	// �̳߳�����
	~ThreadPool()
	{
		// �뿪�̳߳ض���������򣬻��Զ������̳߳ض����������
		// ��ô�������̳߳������е��߳���Դ�����б�Ҫ�ġ������̺߳�����Ҫ�ȴ����е��̻߳���������������
		// �Ϳ����õ��߳�ͨ�ţ��̳߳��뿪������֪ͨ�̺߳������ؽ����������������ȴ��̳߳������е��߳�ָ������������һ�����һ���Ƿ�ȫ��������ȫ���������̳߳سɹ�����
		// ��Ҫ�ǻ����߳�ָ�룬��λ����أ�һ���߳̽�����������������erase�����Զ�����Ԫ�ص�������������ô��Ϊ������ָ�������̶߳������ͳɹ��ͷ���Դ��
		is_pool_runnig = false;

		// ���߳�������û�ã��ڽ���ȴ�״̬����Ȼ������ͷţ�����Щ��Ҫ������Դ���̻߳�ȡ�Ӷ���������
		std::unique_lock<std::mutex> lock(task_que_mtx);
		// ��ʱ���е��߳�������״̬��1.�ȴ�����������Դ����2.����ִ������,��û������ʱ����Ҳ����ִ���������ﵽ�ȴ�������Դ��״̬	����ʵ���е������������Ϊû���Ƿ�����������
		// ��ô��Ҫ�߳�ͨ�ţ�����������һ�������̣߳������̷߳�������ָ��
		not_empty.notify_all();

		all_thread_end.wait(lock, [&]() { return threads.size() == 0; });		// �ȴ����̶߳���Ϊ�ա���Դ

		std::cout << "_______________THREAD POOL END_______________\n";
	}

	// �趨�̳߳�ģʽ
	void setMode(PoolMode mode)
	{
		if (checkRunningState() == false)
		{
			std::cout << "Err: Please start the thread pool before you operating it.\n";
			return;
		}
		this->pool_mode = mode;
	}

	// �û������̳߳ط���
	void start(int init_thread_size = 4)
	{
		std::cout << "_______________THREAD POOL START_______________\n";
		// ��¼��ʼ���̸߳���
		this->initial_thread_size = init_thread_size;
		this->cur_thread_size = init_thread_size;

		// ������ʼ�������߳�
		for (int i = 0; i < initial_thread_size; i++)
		{													// ���� Thread�̶߳����ʱ��ͨ���������̳߳�����̺߳������� Thread����̶߳���
			std::unique_ptr<Thread> p_thread = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));	// ��ָͨ����new������ָ���� make_nuique / make_shared
			int thread_id = p_thread->getId();
			threads.emplace(thread_id, std::move(p_thread));		// unique_ptr ��������ֵ���ã�����Ҫôֱ���� emplace_back�� make_unique��Ҫô��std::move������Դת��
			// emplace_back() �� push_back() �����𣬾����ڵײ�ʵ�ֵĻ��Ʋ�ͬ��push_back() ������β�����Ԫ��ʱ�����Ȼᴴ�����Ԫ�أ�Ȼ���ٽ����Ԫ�ؿ��������ƶ��������У�����ǿ����Ļ����º������������ǰ���������Ԫ�أ���
															// �� emplace_back() ��ʵ��ʱ������ֱ��������β���������Ԫ�أ�ʡȥ�˿������ƶ�Ԫ�صĹ��̡������߳���ô�صĲ������ȴ����ٿ�����Ȼ�ǲ����ʵģ�����thread����Ҳ��ֹ�˿�������͸�ֵ����������������̶߳���ֻ���� emplace_back()
			threads[thread_id]->start();	// ����һ���̺߳���
			idle_thread_size++;		// ��¼�����̵߳�����  �߳���Ȼ����������û��ȡ��������ǿ��е�
		}
		//// ���������߳�
		//for (int i = 0; i < initial_thread_size; i++)
		//{
		//}
		// �ı��̳߳�����״̬
		is_pool_runnig = true;
	}

	// ����������е���������
	void setTaskSizeUpperLimit(int max_size)
	{
		if (checkRunningState() == false)
		{
			std::cout << "Err: Please start the thread pool before you operating it.\n";
			return;
		}
		this->task_size_upper_limit = max_size;
	}

	// cachedģʽ�£������̶߳�����������
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

	// �û����̳߳��ύ����ʹ��C++11�ɱ��ģ���̣��� submitTask ���Խ������������������������Ĳ��� pool.submit(sumN, 1, 2, 3, ..,N)
	template<typename Func, typename... Args>			// &&���ܽ�����ֵ���ã�Ҳ�ܽ�����ֵ����
	auto submitTask(Func&& func, Args&&... args) -> std::future<decltype(func(args...))>
	{
		// �����ڼ����ݴ���ģ��Ĳ�����ֵ�����Ƶ������ʵ��������Ҫ�������ı��滻���������ͼ��ȹ���
		// ������������������������õ�һ����������ִ�еĴ���
		// ͨ�� auto��ȡ->������ͣ���һ��future<T>���ͣ�
		// T������ͨ��C++14�� decltype�Ƶ�func�������������args�ķ�������
		// ��� auto�Ƶ��������ͼ�����ִ�к󷵻صĽ�� future���� sum(1, 2)���� future<int>
		// decltype�ؼ��ֿ����ڱ���ʱ��ȡ���ʽ��������Ϣ��������ʵ��ִ�б��ʽ��
		// ��Ϊ���ͱ�̡�ģ��Ԫ��̺�������Ҫ�����Ƶ��ĳ����ṩ�˸��������Ժͱ��������

		// Task �������� using Task = std::function<void ()>
		// �� std::packaged_task������񣨺���������������ֵdecltype�Ƶ������������� bind�����󶨺��������������args
		// ����֮�⣬��Ϊ����ĺ����Ͳ�������������ֵ����Ҳ��������ֵ���ã�����������ת����֤����ֵ����ֵ���Բ���
		// ��ֵ��rvalue����ָ������ʱ����򼴽������ٵĶ�������û�й̶����ڴ��ַ���������ַ�ڱ��ʽ����������Ч����ֵͨ�������ڸ�ֵ���������Ҳ࣬���Ҳ��ܱ���ֵ��ȡ�����ַ�����˳������ʽ����
			// ��ֵ�����Ƕ���ֵ��һ�����ã�������ֵ�������﷨����һ����ֵ����Ϊ�������֣����Գ����ڸ�ֵ����������࣬Ҳ����ȡ�����ַ��Ȼ������ֵ���õ������봫ͳ����ֵ��ͬ��
			// ��ֵ���ð󶨵���ֵ�����ҿ���ͨ��std::move����ʽ�ؽ���ֵת��Ϊ��ֵ���á�һ��һ�����󱻰󶨵���ֵ���ã��ö���ͱ���Ϊ����Ҫ���ƶ����ģ�����ζ��������Դ�����ڴ桢�ļ�����ȣ����Ա���ȫ�ء���ȡ�����ƶ�������һ�����󣬶��������������������߳�������ܣ��ر��ǵ�������Ͷ������Դ�ܼ��Ͷ���ʱ��
			// ��ֵ���õ���ҪĿ����֧���ƶ����壨move semantics��������������ڲ�����Ҫʱ������Դ���ƶ�������һ�����󣬶����ǽ��а�����������������Լ��ٲ���Ҫ���ڴ������ͷţ���߳�������ܺ�Ч�ʡ�
			// �����۵�����ֵ����������أ���Ϊ��������ģ������ʹ��T&&ʱ��ʵ������ͨ�������۵���ȷ������������ֵ���û�����ֵ���á�
		using RetType = decltype( func(args...));								// �� using�������ͣ������� auto������
		// std::packaged_task<int(int, int)> task(sum1);
		auto task = std::make_shared< std::packaged_task<RetType ()>>(
			// std::bind(std::forward<Func>(func), std::forward<Args>(args)...)	// Ҫ����ĺ��������ð����󶨺�����Ͳ���
			// [&func, &args...]() { return std::invoke(func, args...); }			// ʵ���� bind��֧������ת����C++17��invoke����֧�֣�������invoke
			[&func, &args...]() {
				return std::invoke(std::forward<Func>(func), std::forward<Args>(args)...);
			}
		);
		// ��ȡ�������� future������Ϊ submitTask�ķ���ֵ
		std::future<RetType> future = task->get_future();

		if (checkRunningState() == false)
		{
			std::cout << "Err: Please start the thread pool before you operating it.\n";
			auto task = std::make_shared< std::packaged_task<RetType()>>(
				[]()->RetType { return RetType(); }
			);
			(*task)();		// future.get() ��һ����Ҫִ�еģ�����̳߳�û��������taskִ�в��ˣ���ô getҲ���޴����֣����Է���get()����ʱ�� future_error ��ֱ��ִ�л�abort()�������������Ǹ���ִ�е������� future��get���л���ִ��
			return task->get_future();
		}
		// 1.��ȡ��
		std::unique_lock<std::mutex> lock(task_que_mtx);			// ��ֹ���̲߳���ͬһ�����ڴ���ɵ��̰߳�ȫ����

		// 2.�߳�ͨ�ţ��ȴ���������п��� not_full.wait				// �������û�һֱ�ȴ��������û��ύ��������ܳ���1s��������ֱ�ӷ��أ��ύ����ʧ��
		if (not_full.wait_for(lock, std::chrono::seconds(1), [&]()->bool {							// �ڶ���������ν�������ֵΪ bool���͵ĺ�����α����������ʱ����ִ�У�������ʱ�߳̽���ȴ�״̬
			return task_que.size() < task_size_upper_limit;
			}) == false)														// while (!_Pred()) {   wait(_Lck);	 }  ת�����壬���� while()ѭ��ʵ�ֵķ���
		{	// ˵���ȴ��� 1s��������Ȼû�����㣨���ǵ��ڴ�������ԣ�������е�������һ��Ҫ���õģ�
			std::cerr << "[Err]: Task queue is full, the submit task operation failed.\n";
			// �����ύʧ�ܣ�����һ����װ�� ֱ�ӷ���Ŀ�귵�����͵Ŀ�ֵ��lambda���ʽ ��Task��future
			// ��Ϊ�ǰ�װ���������ֱ�ӷ��أ����Բ��������� get�������ܷ���ԭ����� future����Ȼ�޷��ύ��Ҳ���޷�ִ�У��޷����أ�Ҳ�ͻ�һֱ������get
			auto task = std::make_shared< std::packaged_task<RetType()>>(
				[]()->RetType { return RetType(); }
			);
			(*task)();		// �ύʧ�ܣ�û����ִ�У�get()�޴����֣����Է���get()����ʱ�� future_error ��ֱ��ִ�л�abort()������Ҫ����������ִ�е�
			return task->get_future();
		}

		// 3.����п��࣬����������������
			// task_que.emplace(task);									// emplace �ײ���õ�Ҳ�� emplace_back���������������emplace_back����һ����������Ӧ������ĳλ��ֱ�ӹ����������Ԫ�غ��Ƶ���˼
			// using Task = std::function<void ()> 
			// std::queue<Task> task_que; ��������������������û�з���ֵ��
			// ע������Ĵ�����װһ��û�в���û�з���ֵ��lambda���ʽ����������У�������ִ������Ҫִ�е����񼴿�
		task_que.emplace([task]() {
			(*task)();
			// task�ǹ��� packaged_task�����ָ�룬�����õõ� packaged_task��Ҳ����һ����װ�˵ĺ������󣬺��()������������װ������ִ��
			// һ����� lambda�������̳߳�ִ�У�����ĺ���Ҳ�ͻᱻִ��
		});
		task_size++;												// Ϊʲô��ֱ�ӵ��� task_que.size()����Ϊ�������������õ�

		// 4.�߳�ͨ�ţ��·������񣬶���һ������ not_empty.notify   --   ֪ͨ�߳���ִ������
		not_empty.notify_all();

		// ��Ȼ���ύ������������У�����̳߳���MODE_CACHED,�ͼ��һ���Ƿ�Ӧ�������߳�
		if (pool_mode == PoolMode::MODE_CACHED &&
			task_size > idle_thread_size &&					// �����������ڿ����̵߳�����
			cur_thread_size < thread_size_upper_limit)		// ��ǰ�߳�����С���趨����������
		{
			std::cout << "> > > > create new thread\n";
			// �߳�Ҫ��run�����̺߳�����һ��Ļص�����ֱ�Ӵ��뼴�ɣ���Ա�������У�������ͨ��bind�����Ա������ַ�ͳ�Ա�����Ķ��󣬼����ĸ�������õĳ�Ա������ͬʱ���������Ҫ��������������Ա����������ʹ��ռλ��,_1���ڴ���ص������еĵ�һ���������Դ�����
			std::unique_ptr<Thread> p_thread = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
			int thread_id = p_thread->getId();
			threads.emplace(thread_id, std::move(p_thread));
			// ������map�洢�߳��ˣ�����ͨ���߳�id��Ϊ���������߳�
			threads[thread_id]->start();
			// �޸������Դ����
			cur_thread_size++;
			idle_thread_size++;
		}

		return future;	// Ĭ�� true
	}

	// ����̳߳ص�����״̬
	bool checkRunningState()
	{
		return is_pool_runnig;
	}

	// ���ÿ������죬�̳߳ع���Ķ���ܶ࣬������������
	ThreadPool(const ThreadPool& thread_pool) = delete;

	// ���ø�ֵ���̳߳ع���Ķ���ܶ࣬������������
	ThreadPool& operator=(const ThreadPool& thread_pool) = delete;		

private:
	// �����̺߳���									���������ȡ����ִ��                                                                                                                                                                                                                                                                                
	void threadFunc(int thread_id)
	{
		// �̺߳�����һ�ε��õ�ʱ��
		auto last_time = std::chrono::high_resolution_clock().now();

		for (;;)
		{
			/* ��������
					�����ֽ���ʱ��״̬�Ѿ����ǣ�һ�����߳���wait���ȴ�����һ�����߳���exec()��ִ������
					���ǻ��е��������û�п��� -- �߳�ִ��������ͨ�����while�ļ�����ѭ������ʱ�̳߳ط�������
					���Ҵ�ʱ��������Ѿ�û�������ˣ���ô������Ĵ��룬�߳�ִ�е� not_empty.wait(lock)����
					���������Ի������wait��һ���� ���û��ύ���񣬴�ʱû���µ������ˣ�һ�����̳߳�������������ʱ�Ѿ����ѹ�����wait���̺߳����Ļ��ѣ����ᷢ���ڶ��λ�����
					Ҳ�����������ѵ�ʱ�����������ô���̺߳�������Զ����wait����
					���̵߳ȴ��Ȳ����Ļ��ѣ����̵߳������ȴ����̻߳��ѣ���ô���߳���Զ����������������wait�����������������
					����������������ĺ����߼��ǣ����Ի��Ѹ��̺߳�����wait���¼����Ѿ�����ˣ���ô���߳���Զ����
			*/
			/*
				��ô~ThreadPool()�����������̺߳���֮��������ȱ�ݣ�ֻ�����˵�һ�ֳ����͵ڶ��ֳ���
			*/
			// std::this_thread::sleep_for(std::chrono::seconds(3));	// ����������һ�ᣬ�����׷�������

			Task task;
			{
				// �Ȼ�ȡ��		������ ���������һ�������������߳�ͬʱ��鵽 task_que.size() > 0��������while��ֱ��ȥȡ��������һ����ȡ����������п�����ô�� -- �ǰ��������ѽ��ֻ���������Ĳ����ж��Ƿ���������һ��Ҫ���������̺߳�����ʼλ�õġ���ʵ������û�������ֹ���ݾ�������ĵ��Ͱ���
				std::unique_lock<std::mutex> lock(task_que_mtx);

				// ����̳߳����������Ȼ�����������is_pool_running��ֵ�ѱ��ı䣬��ô��������ж������϶� is_pool_runnig���жϣ��Ϳ�������ѭ�������������޵ȴ���ѭ���������wait��
				// ��Ϊ�̳߳�����Ҫ�ȴ������߳�ִ���꣬����ȡ��������߳�Ҳ��ִ����������������ֱ��û�����񣬲Ż����߳���Դ��������Դ���գ��Ż�����̳߳��������̳߳����������̣߳�������̳߳����������������޷�����ִ��
				while (task_que.size() == 0)		// while�߼���!= 0˵������������������ִ�����������������ڴ�ѭ������ִ��
				{
					// û�����ִ�У���ô�ȼ����Ӧ���������ǵȴ���������������
					if (is_pool_runnig == false)
					{
						// ���յ�ǰ�߳�-------------
						threads.erase(thread_id);	// ����ɾ���߳�			STL����ɾ��Ԫ�ػ��Զ�����Ԫ�ص���������
						std::cout << "> > > > thread " << std::this_thread::get_id() << " recalimed\n";		// ��ӡ��־
						all_thread_end.notify_all();		// ���ѵȴ��е��������������������û�������ڵȴ������� -- ���̳߳���һ���߳�Ҳû���ˣ����������û�����㣬������ȴ��������̲߳��������һ����ɾ�����߳�
					}

					// cachedģʽ�£��ȴ�����������Դ����ÿ����һ�ο���ʱ�䣬���߳̿���ʱ�䳬��60s�����ҵ�ǰ�߳��� > ��ʼ�߳���������ո��߳�
					// ��ǰʱ�� - �ϴ��߳�ִ�������ʱ��
					if (pool_mode == PoolMode::MODE_CACHED)
					{
						// ÿһ�����һ�ε�ǰʱ�䣬�鿴�Ƿ���ϴ�ִ�����񳬹�60s

							// ѭ����ֻҪ�������û��������ôwait_for 1�룬�������������ѡ��򷵻ز���ʱ��������if��������if�ṹ����ִ�У�Ҳ���ǿ�����һ��whileѭ���жϣ���Ϊ����������task_que.size()һ��>0������while����ʼȡ����Ĳ��������Ƿ��س�ʱ����ô�жϿ���ʱ���Ƿ񳬹�60s�ˣ�û�����ͽ�����һ�εȴ���Դ1s�Ĺ���
						if (not_empty.wait_for(lock, std::chrono::seconds(1)) == std::cv_status::timeout)
						{
							auto now = std::chrono::high_resolution_clock().now();
							auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - last_time);
							if (dur.count() > THREAD_IDLE_TIME_LIMIT &&		// 60s����Ҫ�������������֣����� const int ����
								cur_thread_size > initial_thread_size)
							{
								// ���յ�ǰ�߳�-------------
								// ����ɾ���߳�
								threads.erase(thread_id);
								// �����߳�����
								cur_thread_size--;
								idle_thread_size--;
								// ��ӡ��־
								std::cout << "> > > > thread " << std::this_thread::get_id() << " recalimed\n";
								return;
							}
						}

					}
					else	// ����cachedģʽ����ֱ�ӵȴ�����������Դ
					{
						// �ȴ� not_empty ����
						not_empty.wait(lock);
					}

					// ���ܵ����ĸ�ģʽ�����е����ﶼ�Ǳ������ˡ�
					// ���һ���Ǳ� ������ ���ѣ����Ǳ��̳߳�������������
					// �̳߳������������޸�is_pool_runnig��ֵΪfalse���̳߳ؽ���������ձ��̵߳���Դ�� �����Ǳ������ѣ�ȥִ������
					// if (is_pool_runnig == false)
					// {
					// 	// ���յ�ǰ�߳�-------------
					// 	threads.erase(thread_id);	// ����ɾ���߳�		STL����ɾ��Ԫ�ػ��Զ�����Ԫ�ص���������
					// 	std::cout << "> > > > thread " << std::this_thread::get_id() << " recalimed\n";		// ��ӡ��־
					// 	all_thread_end.notify_all();		// ���ѵȴ��е��������������������û�������ڵȴ������� -- ���̳߳���һ���߳�Ҳû���ˣ����������û�����㣬������ȴ��������̲߳��������һ����ɾ�����߳�
					// 	return;		// ��Ȼ�̳߳ض�Ҫ�����ˣ�Ҳû��Ҫ�����߳�������������Դ���ɣ���return�����̺߳���
					// }
					// ���ַ�ʽ���յ��߳��ǵȴ�������̣߳�������һ������ִ��������̣߳�����ִ���������ᷢ��while��ѭ��������--�̳߳������� �Ѿ������㣬����ѭ����������ѭ�����ټ������������̵߳���伴��
				}

				// !!! while �¾��Ǵ������������߼������������ŵ� while ��
				idle_thread_size--;

				// ��������еĶ�ͷȡһ������  ΪʲôҪ�ö��е����ݽṹװ������Ϊ��֤�û��ύ����Թ�ƽ�����нṹ�Ƚ��ȳ��Ľṹ�Ǻ��ʵ�
				task = task_que.front();
				task_que.pop();
				task_size--;
				std::cout << "tId: " << std::this_thread::get_id() << " ��ȡ����ɹ�\n";

				// ���ȡ����������������Ȼ��ʣ�����������֪ͨ�������߳���ȡ����
				if (task_que.size() > 0)
				{
					not_empty.notify_all();
				}

				// ȡ����һ�������������һ��������������Ϊ�������˽���ȴ����е��ύ�����߳�
				not_full.notify_all();
			}							// �������Ӧ�ð����ͷŵ���ִ������Ĺ����л�ռ����������Ȩ�������߳�һֱ�����������ǲ�����ġ���������������һ��������
										// ��Ҳ�Ƕ��̳߳�����ֻ��һ���߳���������ĳ���ԭ�򣺿�ʼִ�������ͬʱû���ͷ���

			// ��ǰ�̸߳���ִ���������			�������õľ��ǵ��͵Ķ�̬��ƣ����������������ǻ����ָ�룬���õ����з������麯����ʵ�ʵ�ʵ����������ʵ�ֵ�
			// task->run();		run �����Ǵ��麯�����޷����������û��ķ���ֵ�����԰�����װ����ͨ�ĳ�Ա����exec()��
			// exec() :	 1. ִ������ 2. ͨ��passValue����������ķ���ֵ���ݸ�Result
			// task->exec();
			// ִ�з�װ��std::function<void ()>���͵� lambda����
			task();

			// ��������Ҫ���������׶��ˣ���������һ��ִ�������ʱ�䣬Ȼ��ÿ1����һ�Σ�����1�����ڵ�ʱ�䣩�ǲ���60sû�л�������ˣ��Ǿͻ��ո��߳�
			last_time = std::chrono::high_resolution_clock().now();

			// �̵߳���������ɣ��ֱ�Ϊ�����߳�
			idle_thread_size++;
		}
	}

private:
	//std::vector< std::unique_ptr<Thread>> threads;			// �߳����飬�洢�̵߳�����		��ָ���Ҫ���������Լ��������鷳����ȫ����ͨ������ָ��
	std::unordered_map<int, std::unique_ptr<Thread>> threads;	// �ĳɼ�ֵ�Ե���ʽ��ͨ��id�����̴߳Ӷ�����ȷ�Ĺ����߳�
	int initial_thread_size;				// ��ʼ���߳�����	start()Ĭ�ϳ�ʼ��ΪCPU��������Ϊstart()��ʹ���̳߳ر����ȵ��õķ����������ڹ��캯����ר�ų�ʼ����ֵΪ0����
	std::atomic_int idle_thread_size;		// ��¼�����̵߳�����	ֻҪ�ǻᱻ��ͬ�߳�ͬʱ�����ı���������ʹ��ԭ������
	int thread_size_upper_limit;			// �߳���������		cachedģʽ���߳�������������ֹ������
	std::atomic_int cur_thread_size;		// ��ǰ�߳�����

	using Task = std::function<void()>;						// ������̳߳ص�����������Ǻ������󣬷���ֵ void�ǽ������Ϊ�м�㣬֮������ʵ�����ͽ��д����������ð���Ҳ���Դ���
	std::queue<Task> task_que;			// ������У�Ϊ��ʵ�ֶ�̬������������ָ������ó�ʼ����
	/* Ҫ����һ�����: ��submitTask(task) �У����û����������ʱ��������������������ö���ͱ�����
	�߳̽��յ�ָ��ָ���ͷŵ��ڴ棬û�����壬�����β���ΪTask���������ָ�� -- �����ӳ���ʱ������������ڵ�run���������󣬲��Զ��ͷ�����Դ */
	std::atomic_int task_size;				// ���������			֪��һ�����Ǹ����Ϳ�����unsigned int�����̶߳����������ķ��ʵĻ����Կ�����ԭ������ʵ��
	int task_size_upper_limit;				// ������������

	std::mutex task_que_mtx;				// ��֤��������̰߳�ȫ�Ļ�����
	std::condition_variable not_full;		// ��ʾ������в���
	std::condition_variable not_empty;		// ��ʾ������в���
	std::condition_variable all_thread_end;		// �̳߳��������ȴ��߳�ȫ������

	PoolMode pool_mode;						// �̳߳ص�ģʽ
	std::atomic_bool is_pool_runnig;		// �̳߳��Ƿ������У������̳߳صĳ�Ա�������ֹ�û��������̳߳�ֱ�ӵ��÷����������
};
// --------------------------------------- �̳߳��� ---------------------------------------




#endif
