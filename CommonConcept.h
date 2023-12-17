#pragma once

namespace detail
{
	/*
	- https://stackoverflow.com/questions/6512019/can-we-get-the-type-of-a-lambda-argument
		- std::function 은 인자 레퍼런스 유무 체크가 무시된다.
			- std::function<void(int&>) f = [](int){};		// 컴파일 허용됨
		- template meta 로 strict 하게 체크한다.
	*/
	template<typename TFunction>
	class FirstArgTrait
	{
	private:
		// General function
		template<typename TReturn, typename TFirstArg, typename ...TArgs>
		static TFirstArg _check(TReturn(*)(TFirstArg, TArgs...)) { return {}; }

		// Member function
		template<typename TReturn, class TClass, typename TFirstArg, typename ...TArgs>
		static TFirstArg _check(TReturn(TClass::*)(TFirstArg, TArgs...)) { return {}; }

		// Const Member function
		template<typename TReturn, class TClass, typename TFirstArg, typename ...TArgs>
		static TFirstArg _check(TReturn(TClass::*)(TFirstArg, TArgs...) const) { return {}; }

		// Functor
		template<class TClass>
		static decltype(_check(&TClass::operator())) _check(TClass) { return {}; }

	public:
		using type_t = decltype(_check(std::declval<TFunction>()));
	};
	// @using
	// coro::detail::FirstArgTrait<TFunction>::type_t
}

// 첫번째 인자는 lvalue 레퍼런스 유형이어야 한다. (람다 포함)
template<typename TFunction>
concept FirstArgumentReferenceTrait = 
	(std::is_lvalue_reference_v<typename detail::FirstArgTrait<TFunction>::type_t> == true)
	;

// 포인터와 lvalue / rvalue 레퍼런스 유형은 사용할 수 없다.
template<typename TArg>
concept NotPointerReferenceTrait = 
	(std::is_pointer_v<TArg> == false)
	&& (std::is_reference_v<TArg> == false)
	;
