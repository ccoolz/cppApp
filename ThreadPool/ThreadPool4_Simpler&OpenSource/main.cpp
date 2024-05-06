// ��һ��Ҫ����C++11���Ժ�������߳����api���̳߳ش���
// ����֮�⣬��һ�潫�̳߳صĶ����ʵ�ַ���һ���ļ��£�ʹ�ø����㣬ʵ�ֲ����Ǳ�¶��

/*
1.	��һ��������һ�����̺߳�������ƣ�ֱ�ӽ��ύ��������Ϊ�̺߳�����
	������������� std::thread T(func1, arg1, ar2...)�������������ύ���ӷ������
	��Ҫʵ��	pool.submitTask(sum1, 4, 5)		pool.submitTask(sum2, 4, 5, 6)		...
	�����õ��ɱ��ģ����

2.	��һ�������Լ�ʵ�ֵ� Any�� Result������ɵ�һЩ����
	������C++11�� packaged_task�� future�������
	std::packaged_task��
		std::packaged_task ��һ��ģ���࣬���ڰ�װһ���ɵ��õ�Ŀ�꣨�纯����lambda ���ʽ�ȣ����Ա��������һ���߳����첽��������
		���� std::future һ��ʹ�ã�������� std::packaged_task �� operator() ʱ������ִ�а�װ�ĺ�������������洢�ڹ����� std::future �����С�
		std::packaged_task �ر��ʺ����ڽ�һ�������װ������������һ���߳���ִ�У�Ȼ����ԭ�߳��л�ȡ�����
		std::packaged_task �ڲ����ܹ�����һЩ��Դ���������ڴ桢�ļ�����ȡ��������������Ϳ�����ֵ
		��ô��Щ��Դ�Ϳ��ܱ���� std::packaged_task ����������Դ����ĸ���������
		���ҿ��ܳ���˫��ɾ����double delete���ȴ������� packaged_task�Ŀ�������Ϳ�����ֵ���������ã�Ҫ��move
	std::future ��һЩ�ؼ����Ժ��÷�������
		�첽�����Ľ����std::future �����ʾһ���첽�����Ľ���������ͨ������ std::future::get() ��������ȡ������������첽������û����ɣ�get() ���������������̣߳�ֱ��������á�
		���ƶ������ɸ��ƣ�std::future �����ǲ��ɸ��Ƶģ�������ͨ���ƶ��������ת�ơ�����ζ������Խ�һ�� std::future �����һ���߳��ƶ�����һ���̣߳����㲻�ܸ���һ�� std::future ����
		�� std::promise ���ʹ�ã������ʹ�� std::promise ����������һ��ֵ���쳣������������� std::future ��������������ṩ��һ�����̼߳䴫��ֵ���쳣�Ļ��ơ�
		�� std::packaged_task ���ʹ�ã�std::packaged_task ��һ�ֽ��ɵ��õ�Ŀ�꣨�纯���� lambda ���ʽ����װ�����Ա��첽ִ�еķ�ʽ�����㴴��һ�� std::packaged_task ����ʱ��������һ����֮������ std::future ���������ʹ����� std::future �����������첽�����Ľ����
		�� std::async ���ʹ�ã�std::async ��һ������ģ�壬����һ���������߳����첽ִ��һ��������������һ�� std::future ���󣬸ö����ʾ�����Ľ����
		�쳣��������첽�����׳����쳣�����Ҹ��쳣û�б��������������洢�ڹ����� std::future �����С�������� std::future::get() ����ʱ������洢���쳣������쳣���������׳���
	�첽��������δ��ĳ����ȷ����ʱ������ĳ������
	ʹ��ʵ����
		std::packaged_task<int(int, int)> task(sum1);
		std::future<int> result = task.get_future();
		packaged_task ����˼�壬��ĳ���������ͨ�� packaged_task.get_future������Ի�ȡfuture<T>����
		future����� get�����ɻ�ȡ����ִ����ɵķ���ֵ
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
	// �����̳߳ض���
	ThreadPool thread_pool;
	thread_pool.start(4);

	// �ύ�����̳߳��Զ�ȡ����ִ��5
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