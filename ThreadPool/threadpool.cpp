// ��Ҫ������Ŀ��������ߵ�C++�汾��14����������ֵ����ɾ����������ǻ�ƥ����ֵ����
#include "threadpool.h"

const int MAX_TASK_SIZE = 1024;

// -------------------------------------------------Task��-------------------------------------------------------
Task::Task()
	: result(nullptr)
{}

void Task::exec()
{
	if (result != nullptr)	// ���ж�ֻ��Ϊ�˱���һЩ����Ʒ���߿��ܳ��ָ�������
	{
		result->passValue(run());		// run�����᷵��һ��Anyֵ�����ǰ������ݸ�Task���Լ���Result
	}
}

void Task::setResult(Result* res_)
{
	result = res_;
}
// --------------------------------------------------------------------------------------------------------------

// -----------------------------------------------�̳߳���---------------------------------------------------------
// �̳߳ع���
ThreadPool::ThreadPool()
	: pool_mode(PoolMode::MODE_FIXED)			// Ĭ��Ϊ�̶��߳�����ģʽ
	, initial_thread_size(0)					// ��ʼ�߳�������start()������һ������ֵ��������Ϊ0����
	, task_size(0)								// �������û��ύ������һ��ʼû������
	, task_size_upper_limit( MAX_TASK_SIZE)		// �趨������������ռ�ù����ڴ�
{}

// �̳߳�����
ThreadPool::~ThreadPool()
{}

// �趨�̳߳�ģʽ
void ThreadPool::setMode(PoolMode mode)
{
	this->pool_mode = mode;
}

// �û������̳߳ط���
void ThreadPool::start(size_t init_thread_size)
{
	// ��¼��ʼ���̸߳���
	this->initial_thread_size = init_thread_size;

	// ������ʼ�������߳�
	for (int i = 0; i < initial_thread_size; i++)
	{													// ���� Thread�̶߳����ʱ��ͨ���������̳߳�����̺߳������� Thread����̶߳���
		std::unique_ptr<Thread> p_thread = std::make_unique<Thread>( std::bind(&ThreadPool::threadFunc, this));	// ��ָͨ����new������ָ���� make_nuique / make_shared
		threads.emplace_back( std::move(p_thread));		// unique_ptr ��������ֵ���ã�����Ҫôֱ���� emplace_back�� make_unique��Ҫô��std::move������Դת��
		// emplace_back() �� push_back() �����𣬾����ڵײ�ʵ�ֵĻ��Ʋ�ͬ��push_back() ������β�����Ԫ��ʱ�����Ȼᴴ�����Ԫ�أ�Ȼ���ٽ����Ԫ�ؿ��������ƶ��������У�����ǿ����Ļ����º������������ǰ���������Ԫ�أ���
	}													// �� emplace_back() ��ʵ��ʱ������ֱ��������β���������Ԫ�أ�ʡȥ�˿������ƶ�Ԫ�صĹ��̡������߳���ô�صĲ������ȴ����ٿ�����Ȼ�ǲ����ʵģ�����thread����Ҳ��ֹ�˿�������͸�ֵ����������������̶߳���ֻ���� emplace_back()
	
	// ���������߳�
	for (int i = 0; i < initial_thread_size; i++)
	{
		threads[i]->start();
	}
}

// ����������е���������
void ThreadPool::setTaskSizeUpperLimit(size_t max_size)
{
	this->task_size_upper_limit = max_size;
}

// �û����̳߳��ύ����
Result ThreadPool::submitTask(std::shared_ptr<Task> task)
{
	// 1.��ȡ��
	std::unique_lock<std::mutex> lock(task_que_mtx);			// ��ֹ���̲߳���ͬһ�����ڴ���ɵ��̰߳�ȫ����

	// 2.�߳�ͨ�ţ��ȴ���������п��� not_full.wait				// �������û�һֱ�ȴ��������û��ύ��������ܳ���1s��������ֱ�ӷ��أ��ύ����ʧ��
	if ( not_full.wait_for( lock, std::chrono::seconds(1), [&]()->bool {							// �ڶ���������ν�������ֵΪ bool���͵ĺ�����α����������ʱ����ִ�У�������ʱ�߳̽���ȴ�״̬
		return task_que.size() < task_size_upper_limit;
	}) == false )														// while (!_Pred()) {   wait(_Lck);	 }  ת�����壬���� while()ѭ��ʵ�ֵķ���
	{	// ˵���ȴ��� 1s��������Ȼû������
		std::cerr << "[Err]: Task queue is full, the submit task operation failed.\n";
		return Result(task, false);
	}

	// 3.����п��࣬����������������
	task_que.emplace(task);										// emplace �ײ���õ�Ҳ�� emplace_back���������������emplace_back����һ����������Ӧ������ĳλ��ֱ�ӹ����������Ԫ�غ��Ƶ���˼
	task_size++;												// Ϊʲô��ֱ�ӵ��� task_que.size()����Ϊ�������������õ�

	// 4.�߳�ͨ�ţ��·������񣬶���һ������ not_empty.notify   --   ֪ͨ�߳���ִ������
	not_empty.notify_all();

	return Result(task);	// Ĭ�� true
}

// �����̺߳���			���������ȡ����ִ��                                                                                                                                                                                                                                                                                
void ThreadPool::threadFunc()
{
	// �̺߳���ѭ����ִ�� ��������л�ȡ���� -> ִ������ �Ĺ���
	for ( ; ; )
	{
		std::shared_ptr<Task> task = nullptr;
		{
			// �Ȼ�ȡ��
			std::unique_lock<std::mutex> lock(task_que_mtx);

			// �ȴ� not_empty ����
			not_empty.wait(lock, [&]() {
				return task_que.size() > 0;
				});

			// ��������еĶ�ͷȡһ������  �ҵ���⣬ΪʲôҪ�ö��е����ݽṹװ������Ϊ��֤�û��ύ����Թ�ƽ�����нṹ�Ƚ��ȳ��Ľṹ�Ǻ��ʵ�
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
		task->exec();
	}
}
// ------------------------------------------------------------------------------------------------------------

// ------------------------------------------------�߳���------------------------------------------------------
Thread::Thread(ThreadFunc func_)
	: func( func_)					// �̶߳�����ʱ���ô�����̳߳ض�������̺߳�����ʼ���Լ����̺߳���
{}

Thread::~Thread() 
{}

void Thread::start()
{									// start ���߳̿�ʼִ���̳߳ش��������̺߳���
	std::thread T( func);
	
	// ���� start������T�������ˣ�����Ҫ���߳���Ϊ�����̣߳��̶߳����뿪�������������߳�ջ�Դ��ڣ��̺߳�����Ȼ���������
	T.detach();
}
// -------------------------------------------------------------------------------------------------------------

// ------------------------------------------------Result��-----------------------------------------------------
Result::Result( std::shared_ptr<Task> task_, bool is_valid_)
	: task(task_)
	, is_valid(is_valid_)
{
	task->setResult(this);
}

Any Result::getAny()					// �û����õ�
{
	if (is_valid == false)
		return "";

	sem.wait();				// �ȴ����û��ύ������ִ����ɡ������Դ
	return std::move(any);
}

void Result::passValue(Any any_)
{
	// �洢task�ķ���ֵ 
	this->any = std::move(any_);
	// �Ѿ���ȡ������ķ���ֵ�����û��ύ������ִ����ɡ������Դ����
	sem.post();
}
// --------------------------------------------------------------------------------------------------------------


