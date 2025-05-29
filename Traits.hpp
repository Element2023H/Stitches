#pragma once

#ifdef _HAS_CXX17
#define _INLINE_VAR inline
#else
#define _INLINE_VAR
#endif

// stolen from STL
// keep these type traits available until this project supports STL
namespace traits {
	template <class...> struct Always_false { static constexpr bool value = false; };

	template <class... Args>
	using _Always_false = typename Always_false<Args...>::value;

	template <class _Ty>
	_INLINE_VAR constexpr bool is_enum_v = __is_enum(_Ty);

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
}
