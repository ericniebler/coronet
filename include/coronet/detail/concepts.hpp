// coronet - An experimental networking library that supports both the
//           Universal Model of the Networking TS and the coroutines of
//           the Coroutines TS.
//
//  Copyright Eric Niebler 2017
//
//  Use, modification and distribution is subject to the
//  Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
// Project home: https://github.com/ericniebler/coronet
//
#ifndef CORONET_DETAIL_CONCEPTS_HPP
#define CORONET_DETAIL_CONCEPTS_HPP

#include <functional>
#include <utility>

// standard concatenation macros.

#define CO_PP_CAT(a, ...) CO_PP_PRIMITIVE_CAT(a, __VA_ARGS__)
#define CO_PP_PRIMITIVE_CAT(a, ...) a##__VA_ARGS__

#define CO_PP_THIRD_ARG(A, B, C, ...) C
#define CO_PP_VA_OPT_SUPPORTED_I(...) CO_PP_THIRD_ARG(__VA_OPT__(, ), 1, 0)
#define CO_PP_VA_OPT_SUPPORTED CO_PP_VA_OPT_SUPPORTED_I(?)

#if CO_PP_VA_OPT_SUPPORTED

#define CO_PP_template(...) \
    template<__VA_ARGS__ __VA_OPT__(, ) CO_PP_REQUIRES /**/

#else // CO_PP_VA_OPT_SUPPORTED

// binary intermediate split macro.
//
// An "intermediate" is a single macro argument
// that expands to more than one argument before
// it can be passed to another macro.  E.g.
//
// #define IM x, y
//
// CO_PP_SPLIT(0, IM) // x
// CO_PP_SPLIT(1, IM) // y

#define CO_PP_SPLIT(i, ...) CO_PP_PRIMITIVE_CAT(CO_PP_SPLIT_, i)(__VA_ARGS__)
#define CO_PP_SPLIT_0(a, ...) a
#define CO_PP_SPLIT_1(a, ...) __VA_ARGS__

// parenthetic expression detection on
// parenthetic expressions of any arity
// (hence the name 'variadic').  E.g.
//
// CO_PP_IS_VARIADIC(+)         // 0
// CO_PP_IS_VARIADIC(())        // 1
// CO_PP_IS_VARIADIC(text)      // 0
// CO_PP_IS_VARIADIC((a, b, c)) // 1

#define CO_PP_IS_VARIADIC(...)                                               \
    CO_PP_SPLIT(                                                             \
        0, CO_PP_CAT(CO_PP_IS_VARIADIC_R_, CO_PP_IS_VARIADIC_C __VA_ARGS__)) \
    /**/
#define CO_PP_IS_VARIADIC_C(...) 1
#define CO_PP_IS_VARIADIC_R_1 1,
#define CO_PP_IS_VARIADIC_R_CO_PP_IS_VARIADIC_C 0,

// lazy 'if' construct.
// 'bit' must be 0 or 1 (i.e. Boolean).  E.g.
//
// CO_PP_IIF(0)(T, F) // F
// CO_PP_IIF(1)(T, F) // T

#define CO_PP_IIF(bit) CO_PP_PRIMITIVE_CAT(CO_PP_IIF_, bit)
#define CO_PP_IIF_0(t, ...) __VA_ARGS__
#define CO_PP_IIF_1(t, ...) t

// emptiness detection macro...

#define CO_PP_IS_EMPTY_NON_FUNCTION(...)      \
    CO_PP_IIF(CO_PP_IS_VARIADIC(__VA_ARGS__)) \
    (0, CO_PP_IS_VARIADIC(CO_PP_IS_EMPTY_NON_FUNCTION_C __VA_ARGS__())) /**/
#define CO_PP_IS_EMPTY_NON_FUNCTION_C() ()

#define CO_PP_EMPTY()
#define CO_PP_COMMA() ,
#define CO_PP_COMMA_IIF(X) CO_PP_IIF(X)(CO_PP_EMPTY, CO_PP_COMMA)()

#define CO_PP_template(...)               \
    template<__VA_ARGS__ CO_PP_COMMA_IIF( \
        CO_PP_IS_EMPTY_NON_FUNCTION(__VA_ARGS__)) CO_PP_REQUIRES /**/
#endif // CO_PP_VA_OPT_SUPPORTED

#define CO_PP_REQUIRES(...) \
    CO_PP_REQUIRES_2(CO_PP_CAT(CO_PP_IMPL_, __VA_ARGS__))
#define CO_PP_IMPL_requires
#define CO_PP_REQUIRES_2(...)                                                   \
    int _coronet_requires_ = 0,                                                 \
    std::enable_if_t<(_coronet_requires_ == 0 && (__VA_ARGS__))>* = nullptr >

namespace coronet
{
    template<class Concept, class... Ts>
    constexpr bool _is_not_satisfied_by_(long)
    {
        return true;
    }
    template<class Concept, class... Ts>
    constexpr decltype(&Concept::template requires_<Ts...>)
    _is_not_satisfied_by_(int)
    {
        return nullptr;
    }
    template<class Concept, class... Ts>
    inline constexpr bool is_satisfied_by =
        !_is_not_satisfied_by_<Concept, Ts...>(0);

    struct CSame
    {
        CO_PP_template(class T, class U)(
            requires __is_same(T, U))
        void requires_();
    };

    template<class T, class U>
    inline constexpr bool Same = __is_same(T, U);

    struct CConvertibleTo
    {
        CO_PP_template(class T, class U)(
            requires std::is_convertible_v<T, U>)
        auto requires_(T (&t)()) -> decltype(static_cast<U>(t()));
    };

    template<class T, class U>
    inline constexpr bool ConvertibleTo = is_satisfied_by<CConvertibleTo, T, U>;

    struct CInvocable
    {
        CO_PP_template(class T, class... Us)(
            requires std::__invokable<T, Us...>::value)
        void requires_();
    };

    template<class T, class... Us>
    inline constexpr bool Invocable = std::__invokable<T, Us...>::value;

    template<class... Us>
    using _invokable_archetype = void (&)(Us...);

    // For nested requirements:
    template<class Concept, class... Args>
    inline constexpr std::enable_if_t<is_satisfied_by<Concept, Args...>>*
        requires_{};

    // For placeholder requirements, like:
    //      ( a.foo() ) ->* satisfies<CFoo>
    template<class Concept, class... Args>
    struct _placeholder
    {
        static_assert(Same<Concept, std::decay_t<Concept>>);
        CO_PP_template(class T)(
            requires is_satisfied_by<Concept, T, Args...>)
        void operator()(T);
    };

    template<class Concept, class... Args>
    struct _placeholder<Concept&&, Args...>
    {
        CO_PP_template(class T)(
            requires is_satisfied_by<Concept, T, Args...>)
        void operator()(T&&);
    };

    template<class Concept, class... Args>
    struct _placeholder<Concept&, Args...>
    {
        CO_PP_template(class T)(
            requires is_satisfied_by<Concept, T, Args...>)
        void operator()(T&);
    };

    template<class Concept, class... Args>
    struct _placeholder<Concept const&, Args...>
    {
        CO_PP_template(class T)(
            requires is_satisfied_by<Concept, T, Args...>)
        void operator()(T const&);
    };

    // For placeholder requirements, like:
    //      ( a.foo() ) ->* satisfies<CFoo>
    template<class Concept, class... Args>
    inline constexpr _placeholder<Concept, Args...> satisfies{};

    // For placeholder requirements, like:
    //      ( a.foo() ) ->* satisfies<CFoo>
    CO_PP_template(class T, class Concept, class... Args)(
        requires Invocable<_placeholder<Concept, Args...>, T>)
    void operator->*(T&&, _placeholder<Concept, Args...>);

    // For typename requirements, like:
    //      type<typename T::iterator_category>
    template<class>
    inline constexpr int type = 0;

} // namespace coronet

#endif
