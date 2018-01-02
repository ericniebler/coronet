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
#ifndef CORONET_DETAIL_UTILITY_HPP
#define CORONET_DETAIL_UTILITY_HPP

#include <utility>

namespace coronet
{
    struct _ignore
    {
        template<class T>
        _ignore(T&&)
        {}
    };

    template<class>
    struct _back_fn;

    template<std::size_t... Is>
    struct _back_fn<std::index_sequence<Is...>>
    {
        template<std::size_t>
        using _ignore = _ignore;

        template<class T>
        T operator()(_ignore<Is>..., T&& t)
        {
            return static_cast<T&&>(t);
        }
    };

    template<class... Args>
    decltype(auto) _back(Args&&... args)
    {
        using Is = std::make_index_sequence<sizeof...(Args) - 1>;
        return _back_fn<Is>{}(static_cast<Args&&>(args)...);
    }
}

#endif
