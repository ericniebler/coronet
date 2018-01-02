// coronet - An experimental networking library that supports both the
//           Universal Model of the Networking TS and the coroutines of
//           the Coroutines TS.
//
//  Copyright Eric Niebler 2017
//  Copyright Lewis Baker 2017
//
//  Use, modification and distribution is subject to the
//  Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
// Project home: https://github.com/ericniebler/coronet
//
#ifndef CORONET_DETAIL_NOOP_COROUTINE_HPP
#define CORONET_DETAIL_NOOP_COROUTINE_HPP

#include <experimental/coroutine>

namespace coronet
{
    namespace detail
    {
        class _noop_coroutine_gen final
        {
        public:
            struct promise_type final
            {
                _noop_coroutine_gen get_return_object() noexcept;
                std::experimental::suspend_never initial_suspend() noexcept
                {
                    return {};
                }
                std::experimental::suspend_never final_suspend() noexcept
                {
                    return {};
                }
                void unhandled_exception() noexcept {}
                void return_void() noexcept {}
            };

            static std::experimental::coroutine_handle<> value() noexcept
            {
                static const auto s_value = coroutine().coro_;
                return s_value;
            }

        private:
            explicit _noop_coroutine_gen(promise_type& p) noexcept
              : coro_(std::experimental::coroutine_handle<
                      promise_type>::from_promise(p))
            {}

            static _noop_coroutine_gen coroutine()
            {
                for(;;)
                {
                    co_await std::experimental::suspend_always{};
                }
            }

            const std::experimental::coroutine_handle<> coro_;
        };

        inline _noop_coroutine_gen _noop_coroutine_gen::promise_type::
            get_return_object() noexcept
        {
            return _noop_coroutine_gen{*this};
        }
    } // namespace detail

    inline std::experimental::coroutine_handle<> noop_coroutine()
    {
        return detail::_noop_coroutine_gen::value();
    }
}

#endif
