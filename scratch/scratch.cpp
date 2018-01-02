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

#include <coronet/coronet.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/task.hpp>
#include <experimental/executor>
#include <experimental/io_context>

#include <cstdio>
#include <functional>
#include <optional>

// Define some "universal" asynchronous APIs:
inline constexpr coronet::async async_stuff1 =
    [](int arg, auto token) -> coronet::result_t<decltype(token), int(int)> {
    INITIAL_SUSPEND(token);
    std::printf("async_stuff1\n");
    co_return arg + 1;
};

inline constexpr coronet::async async_stuff2 =
    [](int arg, auto token) -> coronet::result_t<decltype(token), int(int)> {
    INITIAL_SUSPEND(token);
    std::printf("async_stuff2\n");
    // allocator and executor of this coroutine are implicit.
    int i = co_await async_stuff1(arg);
    co_return i + i;
};

struct S
{
    // virtual coronet::callable_task<int> async_member_helper

    CO_PP_TEMPLATE(class Token)
    CO_PP_REQUIRES(coronet::CompletionToken<Token>)
    auto async_member(int arg, Token token)
        -> coronet::result_t<Token, int(int)>
    {
        INITIAL_SUSPEND(token);
        co_return co_await async_stuff2(arg + member);
    }
    auto async_member(int arg)
    {
        return coronet::callable_with_implicit_context{
            [this, arg](auto token) { return this->async_member(arg, token); }};
    }
    int member = 42;
};

// cppcoro::task<int> async_thingie(coronet::allocator<>) {
//     // TODO: get the allocator into the task's promise
//     co_return 42;
// }

int
main()
{
    // An asynchronous work queue on which to schedule the coroutines
    std::experimental::net::io_context ctx;
    // Keep the work queue alive even if there are no work items.
    auto g = std::experimental::net::make_work_guard(ctx);
    // Drive the work queue from another thread.
    auto t = std::thread([&ctx] { ctx.run(); });
    // Fetch an executor so we can schedule work on this thread pool.
    auto e = ctx.get_executor();

    // Use coroutines, return a coronet::task
    auto i = cppcoro::sync_wait(async_stuff2(20, coronet::yield(e)));
    std::printf("hello coroutine! %d\n", i);

    // use callbacks, return void.
    async_stuff2(20, [state = 1234](std::exception_ptr, int i) {
        std::printf("hello callback! %d (state = %d)\n", i, state);
    } | coronet::via(e));

    S s;
    // Use coroutines, return a coronet::task
    i = cppcoro::sync_wait(s.async_member(20, coronet::yield(e)));
    std::printf("hello coroutine member! %d\n", i);

    g.reset();
    t.join();
}
