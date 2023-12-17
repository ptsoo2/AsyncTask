# Simple Thread Executor

### Directory
- AsyncTask.h: async task process with coroutine
- CommonConcept.h: common concept trait
- example.cpp: simple async thread process with coroutine

### Content
- (https://github.com/xotn1270/Simple-Thread-Executor) 의 Coroutine 버전
  - 코루틴 전용 로직을 제외하고, 기본적인 로직은 유사함
- Task 가 끝나고, Callback 처리를 위한 로직이 추가됨
- Concept, Coroutine 을 익히기 위한 공개용 문서
  - https://deserted-viscount-8c4.notion.site/coroutine-3d0361ea67344e9c932d565ed949f223

### Content
- Coroutine 을 정리한 내

### TODO
- 성능 테스트
- Linux 에서의 테스트

## EXAMPLE
```c
#include <latch>
#include "AsyncTask.h"

namespace TSUtil
{
	using threadId_t = uint32_t;
	inline static threadId_t get_tid()
	{
#ifdef __linux__
		return (threadId_t)syscall(SYS_gettid);
#else // __linux__
		// 로컬에서 해도될듯?
		std::thread::id id = std::this_thread::get_id();
		return *(threadId_t*)&id;
#endif // __linux__
	}
}

struct stTaskTag_t {};

using executor_t	= coro::CAsyncTaskExecutor<stTaskTag_t>;
using fnTask_t		= executor_t::fnTask_t;
using coroTask_t	= coro::CAsyncTask<stTaskTag_t>;

TSUtil::CMPSCQueue<fnTask_t>	callbackQueue;
auto&							ASYNC_EXECUTOR = coro::ASYNC_TASK_EXECUTOR<stTaskTag_t>();

coroTask_t async_SimpleTaskCoro()
{
	struct stData_t
	{
		int			httpCode_ = 200;
		std::string body_;
	};

	// async process (post)
	stData_t data1 = co_await coro::post_thread<stData_t>(
		[](stData_t& data)
		{
			data.body_ = "hello";

			printf("long term job(%u) \n", TSUtil::get_tid());
		}
	);

	// callback
	printf("callback(body: %s, tid: %u) \n", data1.body_.c_str(), TSUtil::get_tid());

	// async process (post at)
	stData_t data2 = co_await coro::post_thread_at<stData_t>(1,
		[](stData_t& data)
		{
			data.body_ = "world";

			printf("long term job(%u) \n", TSUtil::get_tid());
		}
	);

	// callback
	printf("callback(body: %s %s, tid: %u) \n", 
		data1.body_.c_str(), data2.body_.c_str(), TSUtil::get_tid());
}

int main()
{
	const size_t callbackCount = 2;
	std::latch latch(callbackCount);

#ifdef BOOST_ASIO_IO_SERVICE_HPP
	// boost 를 사용하는 경우
	ASYNC_EXECUTOR.init(10, coro::makeAsioCallback(asioService));
#else // BOOST_ASIO_IO_SERVICE_HPP
	std::jthread callbackThread
	{
		[&latch]()
		{
			printf("spawn callback thread(%u) \n", TSUtil::get_tid());
			while (latch.try_wait() == false)
			{
				if (callbackQueue.swap() == false)
					continue;

				auto readQueue = callbackQueue.getReadQueue();
				while (readQueue->empty() == false)
				{
					readQueue->front()();
					readQueue->pop();

					latch.count_down();
				}
			}
		}
	};

	// init thread executor
	ASYNC_EXECUTOR.init(10,
		[](fnTask_t&& task)
		{
			// Task 처리가 끝났을 때의 Callback 처리: Callback 큐에 삽입
			callbackQueue.push(std::forward<fnTask_t>(task));
		}
	);
#endif // BOOST_ASIO_IO_SERVICE_HPP

	async_SimpleTaskCoro();

	latch.wait();
	ASYNC_EXECUTOR.stop();

	return 0;
}
```
## OUTPUT
```c
long term job(4112)
spawn callback thread(46996)
callback(body: hello, tid: 46996)
long term job(30104)
callback(body: hello world, tid: 46996)
