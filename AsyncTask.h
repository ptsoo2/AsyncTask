#pragma once

#include <coroutine>

#include "CommonConcept.h"
#include "ThreadPool.h"

namespace coro
{
	template<typename TTag>
	class CAsyncTaskExecutor
		: public TSUtil::CThreadExecutor
	{
	public:
		using super_t = TSUtil::CThreadExecutor;
		using fnTask_t = super_t::fnTask_t;
		using fnCallback_t = std::function<void(fnTask_t&&)>;

	public:
		static auto getInstance() -> CAsyncTaskExecutor<TTag>&
		{
			static CAsyncTaskExecutor<TTag> instance;
			return instance;
		}

	public:
		void			init(size_t threadCount, fnCallback_t&& fnCallback, time_t deadlineMilliSec = 100)
		{
			super_t::init(threadCount, deadlineMilliSec);
			fnCallback_ = std::move(fnCallback);
		}

		void			postCallback(fnTask_t&& task)
		{
			if (fnCallback_ != nullptr)
				fnCallback_(std::forward<fnTask_t>(task));
		}

	protected:
		fnCallback_t	fnCallback_;
	};

	// noop
	template<>
	class CAsyncTaskExecutor<void>
	{
	public:
		using super_t = TSUtil::CThreadExecutor;
		using fnTask_t = super_t::fnTask_t;
		using fnCallback_t = std::function<void(fnTask_t&&)>;
	};

	template <typename TTag>
	inline auto& ASYNC_TASK_EXECUTOR() { return coro::CAsyncTaskExecutor<TTag>::getInstance(); }
}

namespace coro
{
	namespace detail
	{
		template<typename TResult>
		class post_thread_base
		{
		protected:
			using executor_t = TSUtil::CThreadExecutor;
			using fnTask_t = std::function<void(TResult&)>;
			using fnWrappedTask_t = executor_t::fnTask_t;
			using holder_t = std::shared_ptr<TResult>;

			struct deleter_t
			{
				// release 과정이 포함되므로 소멸자를 호출하지 않는다.
				// 코루틴 스택에서 정리되도록 유도한다.
				void operator()(TResult* ptr) const noexcept { free(ptr); }
			};

		public:
			template<FirstArgumentReferenceTrait TFnTask>
			post_thread_base(TFnTask&& fnTask)
				: fnTask_(std::move(fnTask))
			{}
			virtual ~post_thread_base() {}

		public:
			bool				await_ready() const noexcept { return false; }
			TResult				await_resume() noexcept
			{
				// 결과를 꺼내줄 때에는 의도적으로 move 해서 rvo 를 태운다.
				TResult ret = std::move(*result_);
				result_.reset();
				return ret;
			}
			void				await_suspend(auto handle) noexcept
			{
				_post(handle.promise().getExecutor(),
					[result = result_, fnTask = std::move(fnTask_), handle = handle]()
					{
						if (fnTask != nullptr)
							fnTask(*result);

						handle.promise().getExecutor().postCallback(
							[handle]()
							{
								handle.resume();
							}
						);
					}
				);
			}

		protected:
			virtual void		_post(executor_t& executor, fnWrappedTask_t&& wrappedTask) = 0;

		protected:
			fnTask_t			fnTask_;
			holder_t			result_ = holder_t{ new TResult{}, deleter_t{} };
		};

		template<typename TResult>
		inline constexpr auto	getDummyTask()
		{
			return [](TResult&) {};
		}
	}

	template<typename TResult = void*>
	class post_thread : public detail::post_thread_base<TResult>
	{
		using super_t = detail::post_thread_base<TResult>;

	public:
		template<FirstArgumentReferenceTrait TFnTask>
		post_thread(TFnTask&& fnTask = detail::getDummyTask<TResult>())
			: super_t(std::forward<TFnTask>(fnTask))
		{}
		~post_thread() {}

	protected:
		virtual void		_post(super_t::executor_t& executor, super_t::fnWrappedTask_t&& wrappedTask) override
		{
			executor.post(std::move(wrappedTask));
		}
	};

	template<typename TResult = void*>
	class post_thread_at : public detail::post_thread_base<TResult>
	{
		using super_t = detail::post_thread_base<TResult>;

	public:
		template<FirstArgumentReferenceTrait TFnTask>
		post_thread_at(size_t tid, TFnTask&& fnTask = detail::getDummyTask<TResult>())
			: super_t(std::move(fnTask))
			, tid_(tid)
		{}
		~post_thread_at() {}

	protected:
		virtual void		_post(super_t::executor_t& executor, super_t::fnWrappedTask_t&& wrappedTask) override
		{
			executor.postAt(tid_, std::move(wrappedTask));
		}

	protected:
		size_t				tid_ = std::numeric_limits<uint64_t>::max();
	};
}

namespace coro
{
	template<typename TExecutorTag>
	class CAsyncTask {};

	namespace detail
	{
		class task_promise_type_base
		{
		public:
			std::suspend_never		initial_suspend() noexcept { return {}; }	// 중단하지않음
			std::suspend_never		final_suspend() noexcept { return {}; }		// 중단하지않음
			void					unhandled_exception() {}
			void					return_void() noexcept {}
		};
	}

	template<typename TExecutorTag>
	class task_promise_type : public detail::task_promise_type_base
	{
		using handle_t = std::coroutine_handle<task_promise_type<TExecutorTag>>;
		using executor_t = CAsyncTaskExecutor<TExecutorTag>;

	public:
		CAsyncTask<TExecutorTag>	get_return_object() { return {}; }
		auto& getExecutor() noexcept { return executor_t::getInstance(); }
	};
}

// coro::CAsyncTask 로 사용하는 코루틴 함수에서는 포인터 & 레퍼런스 유형의 인자를 사용할 수 없게 강제한다.
template<typename TExecutorTag, NotPointerReferenceTrait... TArgs>
struct std::coroutine_traits<coro::CAsyncTask<TExecutorTag>, TArgs...>
{
	using promise_type = coro::task_promise_type<TExecutorTag>;
};

namespace coro
{
#ifdef BOOST_ASIO_IO_SERVICE_HPP
	inline CAsyncTaskExecutor<void>::fnCallback_t makeAsioCallback(asio::io_service& ioService)
	{
		return [&ioService](auto&& task)
			{
				ioService.post(std::move(task));
			};
	}
#endif // BOOST_ASIO_IO_SERVICE_HPP
}
