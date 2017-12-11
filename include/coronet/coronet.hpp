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
#ifndef CORONET_CORONET_HPP
#define CORONET_CORONET_HPP

#include <cassert>
#include <experimental/coroutine>
#include <functional>
#include <memory>
#include <optional>
#include <type_traits>

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

    template <class T, class U>
    inline constexpr bool Same = __is_same(T, U);

    template <class T, class... Us>
    inline constexpr bool Invocable = std::__invokable<T, Us...>::value;

    TEMPLATE(class T, class U)
        REQUIRES(Same<T, U>)
    void has_type(U&&);

    template <class Concept, class... Ts>
    using try_requires_ = decltype(&Concept::template requires_<Ts...>);

    template <class Concept, class... Ts>
    inline constexpr bool is_satisfied_by =
        meta::is_trait<meta::defer<try_requires_, Concept, Ts...>>::value;

    struct CExecutor {
        template <class E>
        auto requires_(E& e, void (&fun)(), std::allocator<void> a) -> decltype(
            e.post(fun, a)
        );
    };
    template <class E>
    inline constexpr bool Executor = is_satisfied_by<CExecutor, E>;

    struct CAllocator {
        template <class A>
        auto requires_(typename A::template rebind<int>::other& a) -> decltype(
            has_type<int*>(a.allocate(static_cast<std::size_t>(0)))
        );
    };
    template <class A>
    inline constexpr bool Allocator = is_satisfied_by<CAllocator, A>;

    template <class E, class A = std::allocator<void>>
    struct yield_t {
        static_assert(Executor<E>);
        static_assert(Allocator<A>);
        E exec_ {};
        A alloc_ {};

        yield_t() = delete;
        constexpr explicit yield_t(E e, A a = A{})
            : exec_(e), alloc_(a) {
        }
        auto get_allocator() const {
            return alloc_;
        }
        auto get_executor() const {
            return exec_;
        }
    };

    struct yield_gen_t {
        TEMPLATE(class E, class A = std::allocator<void>)
            REQUIRES(Executor<E> && Allocator<A>)
        constexpr auto operator()(E e, A a = A{}) const {
            return yield_t{e, a};
        }
        TEMPLATE(class E, class A)
            REQUIRES(Executor<E> && Allocator<A>)
        constexpr auto operator()(A a, E e) const {
            return yield_t{e, a};
        }
    };

    inline constexpr yield_gen_t yield {};

    struct with_implicit_context {};

    template <class Fn>
    struct [[nodiscard]] callable_with_implicit_context
        : with_implicit_context, Fn {
        callable_with_implicit_context(Fn fun)
            : Fn(std::move(fun)) {
        }
    };

    template <class T>
    inline constexpr bool has_implicit_context =
        std::is_base_of_v<with_implicit_context, T>;

    struct _ignore {
        template <class T>
        _ignore(T &&) {
        }
    };

    template <class T>
    auto get_allocator(T t) -> decltype(t.get_allocator()) {
        return t.get_allocator();
    }

    inline auto get_allocator(_ignore) {
        return std::allocator<void>{};
    }

    template <class T>
    auto get_executor(T t) -> decltype(t.get_executor()) {
        return t.get_executor();
    }

    template <class T, class Token>
    struct [[nodiscard]] task {
        struct promise_type {
            std::exception_ptr eptr_ {};
            std::optional<T> value_ {};
            std::optional<Token> token_ {};
            std::experimental::coroutine_handle<> awaiter_ {};
            void set_token(Token token) {
                token_.emplace(std::move(token));
            }
            auto get_executor() const {
                return coronet::get_executor(*token_);
            }
            auto get_allocator() const {
                return coronet::get_allocator(*token_);
            }
            auto initial_suspend() const noexcept {
                // For now, the INITIAL_SUSPEND macro is treated as the
                // coroutine's initial_suspend
                return std::experimental::suspend_never{};
            }
            auto final_suspend() const noexcept {
                struct awaitable {
                    promise_type const* promise_;
                    static bool await_ready() noexcept {
                        return false;
                    }
                    std::experimental::coroutine_handle<> await_suspend(
                        std::experimental::coroutine_handle<>) const {
                        assert(promise_->awaiter_ != nullptr);
                        return promise_->awaiter_;
                    }
                    static void await_resume() noexcept {
                    }
                };
                return awaitable{this};
            }
            void unhandled_exception() noexcept {
                eptr_ = std::current_exception();
            }
            void return_value(T value) {
                value_ = std::move(value);
            }
            task get_return_object() noexcept {
                return task{*this};
            }
            template <class U>
            auto await_transform(U t) {
                if constexpr (has_implicit_context<U>)
                    return t(yield(get_executor(), get_allocator()));
                else
                    return t;
            }
        };

        std::experimental::coroutine_handle<promise_type> coro_ {};

        task(promise_type& p)
          : coro_(std::experimental::coroutine_handle<promise_type>::from_promise(p))
        {}
        task(task&& that)
          : coro_(std::exchange(that.coro_, {}))
        {}
        ~task() {
            if (coro_)
                coro_.destroy();
        }
        auto operator co_await() const noexcept {
            struct awaitable {
                std::experimental::coroutine_handle<promise_type> coro_;
                bool await_ready() const {
                    return coro_.promise().value_.has_value();
                }
                void await_suspend(std::experimental::coroutine_handle<> awaiter) const {
                    // schedule the continuation to be called from this coroutine's
                    // final_suspend.
                    coro_.promise().awaiter_ = awaiter;
                    // schedule this coroutine to execute via the executor.
                    auto& token = *coro_.promise().token_;
                    coronet::get_executor(token).post(coro_, coronet::get_allocator(token));
                }
                T await_resume() const {
                    if (coro_.promise().eptr_)
                        std::rethrow_exception(coro_.promise().eptr_);
                    return *coro_.promise().value_;
                }
            };
            return awaitable{coro_};
        }
    };

    template <class T, class Token>
    struct void_ {
        struct promise_type {
            std::exception_ptr eptr_ {};
            T value_ {};
            std::optional<Token> token_ {};
            auto get_executor() const {
                return coronet::get_executor(*token_);
            }
            auto get_allocator() const {
                return coronet::get_allocator(*token_);
            }
            void set_token(Token token) {
                token_.emplace(std::move(token));
                auto coro = std::experimental::coroutine_handle<promise_type>::
                    from_promise(*this);
                // Enque this asynchronous operation (detached)
                get_executor().post(coro, get_allocator());
            }
            auto initial_suspend() const noexcept {
                // For now, the INITIAL_SUSPEND macro is treated as the
                // coroutine's initial_suspend
                return std::experimental::suspend_never{};
            }
            auto final_suspend() noexcept {
                struct awaitable {
                    promise_type* this_;
                    static constexpr bool await_ready() noexcept {
                        return false;
                    }
                    void await_suspend(std::experimental::coroutine_handle<>) {
                        auto token = std::move(this_->token_);
                        auto value = std::move(this_->value_);
                        auto eptr = this_->eptr_;
                        std::experimental::coroutine_handle<promise_type>
                            ::from_promise(*this_).destroy();
                        if constexpr (!std::is_void_v<T>)
                            (*token)(eptr, value);
                        else
                            (*token)(eptr);
                    }
                    static void await_resume() noexcept {
                    }
                };
                return awaitable{this};
            }
            void unhandled_exception() noexcept {
                eptr_ = std::current_exception();
            }
            void return_value(T value) {
                value_ = std::move(value);
            }
            void_ get_return_object() noexcept {
                return void_{};
            }
            template <class U>
            auto await_transform(U t) {
                if constexpr (has_implicit_context<U>)
                    return t(yield(get_executor(), get_allocator()));
                else
                    return t;
            }
        };
    };

    // This becomes the de facto initial_suspend until the promise type constructor
    // gets passed the completion token.
    template <class Token>
    struct _try_set_token_ {
        Token token_;
        _try_set_token_(Token token)
            : token_(std::move(token)) {
        }
        bool await_ready() const noexcept {
            return false;
        }
        template <class Promise>
        void await_suspend(std::experimental::coroutine_handle<Promise> awaiter) {
            awaiter.promise().set_token(std::move(token_));
        }
        void await_resume() const noexcept {
        }
    };

    // This won't be needed once we can pass the coroutine arguments to the
    // promise type's constructor.
    #define INITIAL_SUSPEND(token) \
        (void)(co_await ::coronet::_try_set_token_{token})

    struct _executor_type {
        void post(_ignore, _ignore) const;
    };
    using _yield_type = coronet::yield_t<_executor_type>;

    // An asynchronous API wrapper that also provides an overload that does
    // not take a completion token, the semantics being to execute as a
    // coroutine with the execution context (allocator and executor) taken
    // from the calling coroutine.
    template <class Fn>
    struct async {
    private:
        Fn fn_;
    public:
        constexpr async(Fn fun)
            : fn_(std::move(fun)) {
        }

        TEMPLATE(class... Ts)
            REQUIRES(Invocable<const Fn&, Ts...>)
        auto operator()(Ts... ts) const {
            using Ret = decltype(fn_(std::move(ts)...));
            // If this async operation returns coronet::void_, just return void.
            if constexpr (meta::is<Ret, coronet::void_>::value) {
                (void) fn_(std::move(ts)...);
            } else {
                return fn_(std::move(ts)...);
            }
        }

        TEMPLATE(class... Ts)
            REQUIRES(Invocable<const Fn&, const Ts&..., _yield_type&>)
        auto operator()(Ts... ts) const {
            return callable_with_implicit_context{
                [ts..., this](auto token) {
                    return fn_(ts..., token);
                }
            };
        }
    };

    TEMPLATE (class Ex, class Fn)
        REQUIRES(Executor<Ex>)
    auto schedule_on(Ex ex, Fn fn) {
        struct token : Fn {
            Ex ex_;
            token() = default;
            token(Ex ex, Fn fn) : Fn(std::move(fn)), ex_(std::move(ex)) {}
            auto get_executor() const {
                return ex_;
            }
        };
        return token{std::move(ex), std::move(fn)};
    }

    // The mechanism from the Networking TS to support the Universal Model for
    // Asynchronous Operations (N3747).
    template <class Sig, class Token>
    struct async_result {
    };

    template <class Sig, class Token>
    using async_result_t = typename async_result<Sig, Token>::type;

    template <class Ret, class... Args, class Token>
    struct async_result<Ret(Args...), Token> {
        using type = void_<Ret, Token>; // assume Token represents a callback
    };

    template <class Ret, class... Args, class Executor, class Allocator>
    struct async_result<Ret(Args...), yield_t<Executor, Allocator>> {
        using type = task<Ret, yield_t<Executor, Allocator>>;
    };
} // namespace coronet

#endif
