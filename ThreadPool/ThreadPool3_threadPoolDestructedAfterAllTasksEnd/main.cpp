/* ������
 ���Լ���ʱ������ʵ����������̳߳ض������������Ƿ�����������������ܻ�û��ִ�У��߳̾��˳��ˣ������ǵ�����ִ����������
 ����ͨ���̳߳�����У��û����ܲ��ῼ���̳߳ص����������⣬��ô��ִ���ύ��������˳��ǲ����ʵ�
 ��������һ�棬���Ǽ��������Ż����ı������Դ�Ĳ��ԣ���Ҫ�ı���߼��� --  for ѭ����Ƕ��while�жϣ�������ֱ��������������������ִ������û������ܽ��л����̻߳�ȴ�����Ĳ���
 ��Ҫ������ threadFunc����
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

	// ����һ����ô���run�ķ���ֵ�����Խ�������������أ���ģ���ǲ��еģ�run���麯������������Ҫȷ���麯������ģ��������ʱȷ�����Ͳ��滻��
	// C++17 ��Any���;Ϳ��ԡ���ô�����Լ�д��Any����
	Any run()
	{
		std::cout << "tId: " << std::this_thread::get_id() << " ��ʼִ��\n";
		Hello();
		std::cout << "thread " << std::this_thread::get_id() << "\n";
		uLong sum = 0;
		for (int i = begin; i <= end; i++)
			sum += i;

		std::this_thread::sleep_for(std::chrono::seconds(3));
		std::cout << "tId: " << std::this_thread::get_id() << " ִ�н���\n";
		return sum;		// ����ֵsum ��������Any �൱�ڲ鿴Any���Ƿ���һ��sum���Ͳ����Ĺ��캯�������У����Ըò�������
	}					// ��Any�Ĺ��캯���Ժ���ģ����� template< typename T> \Any(T data_) ... �������۷���ʲô���ͣ�Any�����Խ���
private:
	int begin = 0;
	int end = 0;
};

void test1()
{
	ThreadPool thread_pool;
	thread_pool.start(4);

	// Master - Slave �߳�ģ��
	// Master�߳������ֽ�����Ȼ�������slave�̷߳�������
	// �ȴ�����slave�߳�ִ�������񣬷��ؽ��
	// Master�̺߳ϲ����������������
	Result res1 = thread_pool.submitTask(std::make_shared<TaskHello>(0, 100000000));
	Result res2 = thread_pool.submitTask(std::make_shared<TaskHello>(100000001, 200000000));
	Result res3 = thread_pool.submitTask(std::make_shared<TaskHello>(200000001, 300000000));
	// �����������Result������  -- ��return task->Result ��������ƻ��� return Result(task)�����Ǻ��ߣ���ΪTask���������������������һ�оͽ����ˣ���Ҫ�ú����������ӳ�������һ�н���,������ӳ���ʽ����ͨ��shared_ptr���������ü�����������Result���������һ��shared_ptr����
	uLong sum1 = res1.getAny().cast<uLong>();		// !Bug: δ��������쳣: Microsoft C++ �쳣: char��λ���ڴ�λ�� 0x0133FAD  ��Ҫԭ��������cast<int>�����˸ĳ�cast<uLong>
	uLong sum2 = res2.getAny().cast<uLong>();
	uLong sum3 = res3.getAny().cast<uLong>();
	std::cout << "sum = " << sum1 + sum2 + sum3 << "\n";

	//thread_pool.submitTask(std::make_shared<TaskHello>());		// ����ֱ�Ӵ��������������

	//std::shared_ptr<TaskHello> HelloTask = std::make_shared<TaskHello>();
	//thread_pool.submitTask(HelloTask);							// Ҳ���Դ���ʵ�ִ����õ��������
	//thread_pool.submitTask(HelloTask);
	//thread_pool.submitTask(HelloTask);
	//thread_pool.submitTask(HelloTask);
	//thread_pool.submitTask(HelloTask);
	//thread_pool.submitTask(HelloTask);
	//thread_pool.submitTask(HelloTask);		// 8�������ύ������������б������߳�ȡ�ߣ����������뱻��������ȡ�ߣ�������������û�п����߳�
	//thread_pool.submitTask(HelloTask);

	// ��Ϊ�߳���detach�ģ����������������꣬�߳���Դ�Զ����գ��̺߳�����û���ü������꣬�ⲻ����Ƶ����⣬ʵ��Ӧ���У����������ǳ������еģ���������ʱ�����һ��
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
	// ��ʼ4���̣߳��������ĸ�����ȡ�ߣ�����������ʱ������ܿ�ִ���ꡣ��ôMODE_CACHED������Ԥ�ڻᴴ���������߳�����������������
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
	// ������������񣬹�13������
	// ���� std::thread::hardware_concurrency()���صĴ����������Ƿ���ȷ(12��)������ȷ������test3�ᴴ��һ�����߳��������13������
	thread_pool.submitTask(std::make_shared<TaskHello>(0, 100000000));

	getchar();
}

void test4()
{
	// ��Ŀ����������������			-- ����ԭ��� ThreadFunc()�е� while��
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
	// ���Գ��������߳��Ƿ�ִ������ֱ���˳�
	// Ԥ��ִ����������˳�����ô���߳�ҲҪ�ȴ����̵߳�����ʱ��
	ThreadPool thread_pool;
	thread_pool.start(4);

	thread_pool.submitTask(std::make_shared<TaskHello>(0, 100000000));
	thread_pool.submitTask(std::make_shared<TaskHello>(0, 100000000));
	thread_pool.submitTask(std::make_shared<TaskHello>(0, 100000000));
	thread_pool.submitTask(std::make_shared<TaskHello>(0, 100000000));
	// ������get��ȡ������Ͳ��ᱻ�Զ�����ֱ����ȡ�����������������ǲ��ȴ�����ִ�����������֮ǰ�����̳߳أ��ͻ�ֱ���˳����߳�
	// uLong sum = res.getAny().cast<uLong>();
	// Ҳ���� get()�����������Ϳ��᲻��ȴ�����ִ����
	// ����������õ�ǰ�������̳߳أ�����Ч���ǲ��ǲ�һ��
}

int main()
{
	test5();
}