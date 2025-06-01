#pragma once

#ifdef _HAS_CXX17
#define _INLINE_VAR inline
#else
#define _INLINE_VAR
#endif

// some traits were stolen from STL
// keep these type traits available until this project supports STL
namespace traits {
	template <class...> struct Always_false { static constexpr bool value = false; };

	template <class... Args>
	using _Always_false = typename Always_false<Args...>::value;

	template <class _Ty>
	_INLINE_VAR constexpr bool is_enum_v = __is_enum(_Ty);

	template <class... _Types>
	using void_t = void;

	template <class _Ty1, class _Ty2>
	struct is_same
	{
		static _INLINE_VAR constexpr bool value = false;
	};

	template <class _Uty>
	struct is_same<_Uty, _Uty>
	{
		static _INLINE_VAR constexpr bool value = true;
	};

	template <class _Ty1, class _Ty2>
	_INLINE_VAR constexpr bool is_same_v = is_same<_Ty1, _Ty2>::value;

	template <class _Ty, bool = is_enum_v<_Ty>>
	struct _Underlying_type {
		using type = __underlying_type(_Ty);
	};

	template <class _Ty>
	struct _Underlying_type<_Ty, false> {};

	template <class _Ty>
	struct underlying_type : _Underlying_type<_Ty> {}; // determine underlying type for enum

	template <class _Ty>
	using underlying_type_t = typename _Underlying_type<_Ty>::type;

	template <bool _Test, class _Ty = void>
	struct enable_if {}; // no member "type" when !_Test

	template <class _Ty>
	struct enable_if<true, _Ty> { // type is _Ty for _Test
		using type = _Ty;
	};

	template <bool _Test, class _Ty = void>
	using enable_if_t = typename enable_if<_Test, _Ty>::type;

	template <class _Ty>
	_INLINE_VAR constexpr bool is_default_constructable_v = __is_constructible(_Ty);

	template <class _Fty>
	struct function_type
	{
		using return_type = void_t<>;
		static _INLINE_VAR constexpr size_t args_count = 0;
	};

	template <class _R, class... _Types>
	struct function_type<_R(*)(_Types...)>
	{
		using return_type = _R;
		static _INLINE_VAR constexpr size_t args_count = sizeof... (_Types);
	};

	template <class _Fx, class _R, class... _Types>
	struct function_type<_R(_Fx::*)(_Types...) const>
	{
		using return_type = _R;
		static _INLINE_VAR constexpr size_t args_count = sizeof... (_Types);
	};

	template <class _Fx, class _R, class... _Types>
	struct function_type<_R(_Fx::*)(_Types...) noexcept>
	{
		using return_type = _R;
		static _INLINE_VAR constexpr size_t args_count = sizeof... (_Types);
	};

	template <class _Fty, typename = void_t<>>
	struct is_call_once_compatible
	{
		using _trait = function_type<_Fty>;

		static _INLINE_VAR constexpr bool value = is_same_v<typename _trait::return_type, void> &&
			_trait::args_count == 0;
	};

	template <class _Fx>
	struct is_call_once_compatible<_Fx, void_t<decltype(&_Fx::operator())>>
	{
		using _trait = function_type<decltype(&_Fx::operator())>;

		static _INLINE_VAR constexpr bool value = is_same_v<typename _trait::return_type, void> &&
			_trait::args_count == 0;
	};

	/// <summary>
	/// check if delegate protype constraints is satisfied in Once
	/// </summary>
	/// <typeparam name="_Ty">target delegate type</typeparam>
	template <class _Ty>
	_INLINE_VAR constexpr bool is_call_once_compatible_v = is_call_once_compatible<_Ty>::value;
}
