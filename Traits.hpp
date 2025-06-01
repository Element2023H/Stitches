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

	// indicates an invalid type tag
	struct invalid_type_tag {};

	template <bool _Test, class _Ty1, class _Ty2>
	struct conditional
	{

	};

	template <class _Ty1, class _Ty2>
	struct conditional<true, _Ty1, _Ty2>
	{
		using type = _Ty1;
	};

	template <class _Ty1, class _Ty2>
	struct conditional<false, _Ty1, _Ty2>
	{
		using type = _Ty2;
	};

	template <bool _Test, class _Ty1, class _Ty2>
	using conditional_t = typename conditional<_Test, _Ty1, _Ty2>::type;

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

	template <class _Fty, class = void_t<>>
	struct has_callable_member
	{
		static _INLINE_VAR constexpr bool value = false;
	};

	template <class _Fx>
	struct has_callable_member<_Fx, void_t<decltype(&_Fx::operator())>>
	{
		static _INLINE_VAR constexpr bool value = true;
	};

	template <class _Fty>
	_INLINE_VAR constexpr bool has_callable_member_v = has_callable_member<_Fty>::value;
	
	// A type for packing a list of types
	template <class... _Types>
	struct type_list {};

	/// <summary>
	/// NOTE
	/// specializations of `function_type` only target on x64
	/// and it is not the whole implementation
	/// </summary>
	/// <typeparam name="_Fty">target function type to test</typeparam>
	template <class _Fty>
	struct function_type
	{
		// DO NOT use void_t<> here, since void_t<> is treated as "void" in type deducing
		using return_type = invalid_type_tag;
		using argument_type = type_list<>;
		static _INLINE_VAR constexpr size_t args_count = 0;
	};

	template <class _R, class... _Types>
	struct function_type<_R(*)(_Types...)>
	{
		using return_type = _R;
		using argument_type = type_list<_Types...>;
		static _INLINE_VAR constexpr size_t args_count = sizeof... (_Types);
	};

	template <class _Fx, class _R, class... _Types>
	struct function_type<_R(_Fx::*)(_Types...) const>
	{
		using return_type = _R;
		using argument_type = type_list<_Types...>;
		static _INLINE_VAR constexpr size_t args_count = sizeof... (_Types);
	};

	template <class _Fx, class _R, class... _Types>
	struct function_type<_R(_Fx::*)(_Types...) noexcept>
	{
		using return_type = _R;
		using argument_type = type_list<_Types...>;
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
	[[deprecated("using is_invokable_v or is_invokable_r_v instead")]]
	_INLINE_VAR constexpr bool is_call_once_compatible_v = is_call_once_compatible<_Ty>::value;

	/// <summary>
	/// A Policy type for selecting specialization for `is_invokable` or `is_invokable_r`
	/// </summary>
	template <class _Fty, class> struct _SelectInvokableTrait;

	template <class _Fty, class = void_t<>>
	struct _SelectInvokableTrait
	{
		using trait = function_type<_Fty>;
	};

	template <class _Fx>
	struct _SelectInvokableTrait<_Fx, void_t<decltype(&_Fx::operator())>>
	{
		using trait = function_type<decltype(&_Fx::operator())>;
	};

	/// <summary>
	/// check if a type `_Fx` is invokable with arguments type `_Types`
	/// </summary>
	/// <typeparam name="_Fx">target type to test</typeparam>
	/// <typeparam name="..._Types">arguments type for calling _Fx</typeparam>
	template <class _Fx, class... _Types>
	struct is_invokable : _SelectInvokableTrait<_Fx>
	{
		using _Base = _SelectInvokableTrait<_Fx>;
		using _Trait = typename _Base::trait;

		// 1.function type trait deduce failed if `_trait::return_type` is void_t<> which indicates is not a function pointer type
		// 2.deduced argument type is exactly match the input `_Types`
		static _INLINE_VAR constexpr bool value =
			!is_same_v<typename _Trait::return_type, invalid_type_tag> &&
			is_same_v<typename _Trait::argument_type, type_list<_Types...>>;
	};

	template <class _Fx, class... _Types>
	_INLINE_VAR constexpr bool is_invokable_v = is_invokable<_Fx, _Types...>::value;

	/// <summary>
	/// check if a type `_Fx` is invokable with arguments type `_Types` and return type `_R`
	/// </summary>
	/// <typeparam name="_R">target type to test</typeparam>
	/// <typeparam name="_Fx">arguments type for calling _Fx</typeparam>
	/// <typeparam name="..._Types">return type of _Fx</typeparam>
	template <class _R, class _Fx, class... _Types>
	struct is_invokable_r : _SelectInvokableTrait<_Fx>
	{
		using _Base = _SelectInvokableTrait<_Fx>;
		using _Trait = typename _Base::trait;

		// 1.function type trait deduce failed if `_trait::return_type` is void_t<> which indicates is not a function pointer type
		// 2.deduced argument type must exactly match the input `_Types`
		// 3.deduced return type of `_Fx` must exactly match the input `_R`
		static _INLINE_VAR constexpr bool value =
			!is_same_v<typename _Trait::return_type, invalid_type_tag> &&
			is_same_v<typename _Trait::return_type, _R> &&
			is_same_v<typename _Trait::argument_type, type_list<_Types...>>;
	};

	template <class _R, class _Fx, class... _Types>
	_INLINE_VAR constexpr bool is_invokable_r_v = is_invokable_r<_R, _Fx, _Types...>::value;
}
