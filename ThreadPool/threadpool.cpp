// ��Ҫ������Ŀ��������ߵ�C++�汾��14����������ֵ����ɾ����������ǻ�ƥ����ֵ����
#include "threadpool.h"

const int MAX_TASK_SIZE = INT32_MAX;
const int MAX_THREAD_SIZE = 100;
const int THREAD_IDLE_TIME_LIMIT = 60;

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
	: pool_mode(PoolMode::MODE_FIXED)				// Ĭ��Ϊ�̶��߳�����ģʽ
	, initial_thread_size(0)						// ��ʼ�߳�������start()������һ������ֵ��������Ϊ0����
	, idle_thread_size(0)							// ��¼�����̵߳�����
	, cur_thread_size(0)							// ��ǰ�߳�����
	, thread_size_upper_limit( MAX_THREAD_SIZE)		// �߳��������ޣ�����ֹ���ӳ�������
	, task_size(0)									// �������û��ύ������һ��ʼû������
	, is_pool_runnig(false)							// ����start()�����ſ�����Ϊtrue
	, task_size_upper_limit( MAX_TASK_SIZE)			// �趨������������ռ�ù����ڴ�
{}

// �̳߳�����
ThreadPool::~ThreadPool()
{}

// �趨�̳߳�ģʽ
void ThreadPool::setMode(PoolMode mode)
{
	if (checkRunningState() == false)
	{
		std::cout << "Err: Please start the thread pool before you operating it.\n";
		return;
	}
	this->pool_mode = mode;
}

// �û������̳߳ط���
void ThreadPool::start(int init_thread_size)
{
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
void ThreadPool::setTaskSizeUpperLimit(int max_size)
{
	if (checkRunningState() == false)
	{
		std::cout << "Err: Please start the thread pool before you operating it.\n";
		return;
	}
	this->task_size_upper_limit = max_size;
}

// cachedģʽ�£������̶߳�����������
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

// �û����̳߳��ύ����
Result ThreadPool::submitTask(std::shared_ptr<Task> task)
{
	if (checkRunningState() == false)
	{
		std::cout << "Err: Please start the thread pool before you operating it.\n";
		return Result(nullptr);
	}

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

	// ��Ȼ���ύ������������У�����̳߳���MODE_CACHED,�ͼ��һ���Ƿ�Ӧ�������߳�
	if (pool_mode == PoolMode::MODE_CACHED &&
		task_size > idle_thread_size &&					// �����������ڿ����̵߳�����
		cur_thread_size < thread_size_upper_limit)		// ��ǰ�߳�����С���趨����������
	{
		std::cout << "> > > > create new thread\n";
		// �߳�Ҫ��run�����̺߳�����һ��Ļص�����ֱ�Ӵ��뼴�ɣ���Ա�������У�������ͨ��bind�����Ա������ַ�ͳ�Ա�����Ķ��󣬼����ĸ�������õĳ�Ա������ͬʱ���������Ҫ��������������Ա����������ʹ��ռλ��,_1���ڴ���ص������еĵ�һ���������Դ�����
		std::unique_ptr p_thread = std::make_unique<Thread>(std::bind( &ThreadPool::threadFunc, this, std::placeholders::_1));
		int thread_id = p_thread->getId();
		threads.emplace( thread_id, std::move(p_thread));
		// ������map�洢�߳��ˣ�����ͨ���߳�id��Ϊ���������߳�
		threads[thread_id]->start();
		// �޸������Դ����
		cur_thread_size++;
		idle_thread_size++;
	}

	return Result(task);	// Ĭ�� true
}

// �����̺߳���			���������ȡ����ִ��                                                                                                                                                                                                                                                                                
void ThreadPool::threadFunc(int thread_id)
{
	// �̺߳�����һ�ε��õ�ʱ��
	auto last_time = std::chrono::high_resolution_clock().now();

	// �̺߳���ѭ����ִ�� ��������л�ȡ���� -> ִ������ �Ĺ���
	for ( ; ; )
	{
		std::shared_ptr<Task> task = nullptr;
		{
			// �Ȼ�ȡ��
			std::unique_lock<std::mutex> lock(task_que_mtx);

			// cachedģʽ�£��ȴ�����������Դ����ÿ����һ�ο���ʱ�䣬���߳̿���ʱ�䳬��60s�����ҵ�ǰ�߳��� > ��ʼ�߳���������ո��߳�
			// ��ǰʱ�� - �ϴ��߳�ִ�������ʱ��
			if (pool_mode == PoolMode::MODE_CACHED)
			{
				// ÿһ�����һ�ε�ǰʱ�䣬�鿴�Ƿ���ϴ�ִ�����񳬹�60s
				while (task_que.size() == 0)	// ����������û�������������Ҫ���
				{
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
			}
			else	// ����cachedģʽ����ֱ�ӵȴ�����������Դ
			{
				// �ȴ� not_empty ����
				not_empty.wait(lock, [&]() {
					return task_que.size() > 0;
				});
			}

			// �����������߳�ͬʱ�����ѣ�wait���������½�����������������һ��Ҫ���������������Ļ�������������¼���ִ�У���ô���߳�һ�����õ����񣬿����߳�-1
			idle_thread_size--;

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

		// ��������Ҫ���������׶��ˣ���������һ��ִ�������ʱ�䣬Ȼ��ÿ1����һ�Σ�����1�����ڵ�ʱ�䣩�ǲ���60sû�л�������ˣ��Ǿͻ��ո��߳�
		last_time = std::chrono::high_resolution_clock().now();

		// �̵߳���������ɣ��ֱ�Ϊ�����߳�
		idle_thread_size++;
	}
}

// ����̳߳ص�����״̬
bool ThreadPool::checkRunningState()
{
	return is_pool_runnig;
}
// ------------------------------------------------------------------------------------------------------------

// ------------------------------------------------�߳���------------------------------------------------------
int Thread::generate_id = 0;

Thread::Thread(ThreadFunc func_)
	: func( func_)		// �̶߳�����ʱ���ô�����̳߳ض�������̺߳�����ʼ���Լ����̺߳���
	, thread_id(generate_id++)		// ���̶߳�����ʱ��id���Զ�������
{}

Thread::~Thread() 
{}

void Thread::start()
{									// start ���߳̿�ʼִ���̳߳ش��������̺߳���
	std::thread T( func, thread_id);
	
	// ���� start������T�������ˣ�����Ҫ���߳���Ϊ�����̣߳��̶߳����뿪�������������߳�ջ�Դ��ڣ��̺߳�����Ȼ���������
	T.detach();
}

int Thread::getId() const
{
	return thread_id;
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


