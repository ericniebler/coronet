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

#include <experimental/io_context>
#include <experimental/executor>
#include <coronet/coronet.hpp>
#include <cppcoro/sync_wait.hpp>

#include <cstdio>
#include <optional>
#include <functional>

template <class Ex>
struct tagged_executor {
    Ex ex_;
    std::string tag_;
    tagged_executor(Ex ex, std::string tag)
        : ex_(std::move(ex)), tag_(std::move(tag)) {
    }
    TEMPLATE(class Fn, class Al)
        REQUIRES(coronet::Invocable<Fn&> && coronet::Allocator<Al>)
    void post(Fn fn, Al al) {
        std::printf("Scheduling through executor : %s\n", tag_.c_str());
        ex_.post(fn, al);
    }
};

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

int main() {
    // An asynchronous work queue on which to schedule the coroutines
    std::experimental::net::io_context ctx;
    // Keep the work queue alive even if there are no work items.
    auto g = std::experimental::net::make_work_guard(ctx);
    // Drive the work queue from another thread.
    auto t = std::thread([&ctx]{ctx.run();});
    // Fetch an executor so we can schedule work on this thread pool.
    auto e = ctx.get_executor();

    // Use coroutines, return a coronet::task
    auto i = cppcoro::sync_wait(async_stuff2(20, coronet::yield(e)));
    std::printf("hello coroutine! %d\n", i);

    // use callbacks, return void.
    async_stuff2(
        20,
        [state = 1234](std::exception_ptr, int i) {
            std::printf("hello callback! %d (state = %d)\n", i, state);
        } | coronet::in(e)
    );

    g.reset();
    t.join();
}
