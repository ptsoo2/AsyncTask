#pragma once

namespace detail
{
	/*
	- https://stackoverflow.com/questions/6512019/can-we-get-the-type-of-a-lambda-argument
		- std::function �� ���� ���۷��� ���� üũ�� ���õȴ�.
			- std::function<void(int&>) f = [](int){};		// ������ ����
		- template meta �� strict �ϰ� üũ�Ѵ�.
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

// ù��° ���ڴ� lvalue ���۷��� �����̾�� �Ѵ�. (���� ����)
template<typename TFunction>
concept FirstArgumentReferenceTrait = 
	(std::is_lvalue_reference_v<typename detail::FirstArgTrait<TFunction>::type_t> == true)
	;

// �����Ϳ� lvalue / rvalue ���۷��� ������ ����� �� ����.
template<typename TArg>
concept NotPointerReferenceTrait = 
	(std::is_pointer_v<TArg> == false)
	&& (std::is_reference_v<TArg> == false)
	;
