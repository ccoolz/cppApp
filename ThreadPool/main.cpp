#include "threadpool.h"

void Hello();

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
		int sum = 0;
		for (int i = begin; i <= end; i++)
			sum += i;

		std::this_thread::sleep_for( std::chrono::seconds(3));
		std::cout << "tId: " << std::this_thread::get_id() << " ִ�н���\n";
		return sum;		// ����ֵsum ��������Any �൱�ڲ鿴Any���Ƿ���һ��sum���Ͳ����Ĺ��캯�������У����Ըò�������
	}					// ��Any�Ĺ��캯���Ժ���ģ����� template< typename T> \Any(T data_) ... �������۷���ʲô���ͣ�Any�����Խ���
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
	// �����������Result������  -- ��return task->Result ��������ƻ��� return Result(task)�����Ǻ��ߣ���ΪTask���������������������һ�оͽ����ˣ���Ҫ�ú����������ӳ�������һ�н���,������ӳ���ʽ����ͨ��shared_ptr���������ü�����������Result���������һ��shared_ptr����
	int sum = res.getAny().cast<int>();
	std::cout << "sum = " << sum;

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
	std::this_thread::sleep_for(std::chrono::seconds(10));
}

int main()
{
	Test1();
}