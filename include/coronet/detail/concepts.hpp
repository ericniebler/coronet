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

#include <utility>
#include <functional>
#include <meta/meta.hpp>

#define CAT2(X, Y) X ## Y
#define CAT(X, Y) CAT2(X, Y)

#define TEMPLATE(...)                                                           \
    template <__VA_ARGS__ __VA_OPT__(,)                                         \
    /**/

#define REQUIRES(...)                                                           \
    int CAT(requires_, __LINE__) = 0,                                           \
    std::enable_if_t<CAT(requires_, __LINE__) == 0 && (__VA_ARGS__), int> = 0>  \
    /**/

namespace coronet {
    struct CSame {
        TEMPLATE (class T, class U)
            REQUIRES (__is_same(T, U))
        void requires_();
    };

    template <class T, class U>
    inline constexpr bool Same = __is_same(T, U);

    struct CInvocable {
        TEMPLATE (class T, class... Us)
            REQUIRES (std::__invokable<T, Us...>::value)
        void requires_();
    };

    template <class T, class... Us>
    inline constexpr bool Invocable = std::__invokable<T, Us...>::value;

    template <class Concept, class... Ts>
    using _try_requires_ = decltype(&Concept::template requires_<Ts...>);

    template <class Concept, class... Ts>
    inline constexpr bool is_satisfied_by =
        meta::is_trait<meta::defer<_try_requires_, Concept, Ts...>>::value;

    template <class Concept, class... Args>
    struct _placeholder {
        static_assert(Same<Concept, std::decay_t<Concept>>);
        TEMPLATE (class T)
            REQUIRES (is_satisfied_by<Concept, T, Args...>)
        void operator()(T);
    };

    template <class Concept, class... Args>
    struct _placeholder<Concept&&, Args...> {
        TEMPLATE (class T)
            REQUIRES (is_satisfied_by<Concept, T, Args...>)
        void operator()(T&&);
    };

    template <class Concept, class... Args>
    struct _placeholder<Concept&, Args...> {
        TEMPLATE (class T)
            REQUIRES (is_satisfied_by<Concept, T, Args...>)
        void operator()(T&);
    };

    template <class Concept, class... Args>
    struct _placeholder<Concept const&, Args...> {
        TEMPLATE (class T)
            REQUIRES (is_satisfied_by<Concept, T, Args...>)
        void operator()(T const&);
    };

    template <class Concept, class... Args>
    inline constexpr _placeholder<Concept, Args...> satisfies {};

    TEMPLATE (class T, class Concept, class... Args)
        REQUIRES (Invocable<_placeholder<Concept, Args...>, T>)
    void operator->*(T&&, _placeholder<Concept, Args...>);
} // namespace coronet

#endif
