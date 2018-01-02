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
#include <functional>
#include <memory>
#include <optional>
#include <type_traits>

#include <meta/meta.hpp>

#include <experimental/coroutine>
#include <experimental/io_context> // for async_result

#include <coronet/detail/allocator.hpp>
#include <coronet/detail/concepts.hpp>
#include <coronet/detail/noop_coroutine.hpp>
#include <coronet/detail/utility.hpp>

namespace coronet
{
    struct CExecutor
    {
        template<class E, class A = _allocator_archetype<void>>
        auto requires_(E& e, _invokable_archetype<> fun, A a)
            -> decltype(requires_<CAllocator, A>, e.post(fun, a));
    };
    template<class E, class A = _allocator_archetype<void>>
    inline constexpr bool Executor = is_satisfied_by<CExecutor, E, A>;

    struct CCompletionToken
    {
        template<class T>
        auto requires_(T t)
            -> decltype((t.get_executor())->*satisfies<CExecutor>,
                        (t.get_allocator())->*satisfies<CAllocator>);
    };
    template<class T>
    inline constexpr bool CompletionToken =
        is_satisfied_by<CCompletionToken, T>;

    struct _executor_archetype
    {
        CO_PP_template(class F, class A)(
            requires Invocable<F&> && Allocator<A>)
        void post(F, A) const;
    };

    struct _completion_token_archetype
    {
        _executor_archetype get_executor();
        _allocator_archetype<void> get_allocator();
        CO_PP_template(class T)(
            requires CompletionToken<T>)
        operator T();
    };

    struct CHasExecutionContext
    {
        template<class P>
        auto requires_(P& p)
            -> decltype(p.get_token()->*satisfies<CCompletionToken>,
                        p.set_token(_completion_token_archetype{}));
    };
    template<class P>
    inline constexpr bool HasExecutionContext =
        is_satisfied_by<CHasExecutionContext, P>;

    struct _implicit_executor
    {
        CO_PP_template(class Fn, class Alloc)(
            requires Invocable<Fn&> && Allocator<Alloc>)
        [[noreturn]] void post(Fn, Alloc) const
        {
            std::terminate();
        }
    };

    template<class A = std::allocator<void>>
    struct _implicit_yield_t
    {
    private:
        A alloc_;

    public:
        constexpr _implicit_yield_t(A alloc = A{})
          : alloc_(alloc)
        {}
        _implicit_executor get_executor() const
        {
            return {};
        }
        A get_allocator() const
        {
            return alloc_;
        }
        CO_PP_template(class A2)(
            requires Allocator<A2>)
        constexpr auto operator()(A2 a) const
        {
            return _implicit_yield_t<A2>{a};
        }
    };

    inline constexpr _implicit_yield_t<> _implicit{};

    template<class E, class A = std::allocator<void>>
    struct yield_t
    {
        static_assert(Executor<E>);
        static_assert(Allocator<A>);
        E exec_{};
        A alloc_{};

        yield_t() = delete;
        constexpr explicit yield_t(E e, A a = A{})
          : exec_(e)
          , alloc_(a)
        {}
        auto get_allocator() const
        {
            return alloc_;
        }
        auto get_executor() const
        {
            return exec_;
        }
    };

    struct yield_gen_t
    {
        CO_PP_template(class E, class A = std::allocator<void>)(
            requires Executor<E> && Allocator<A>)
        constexpr auto operator()(E e, A a = A{}) const
        {
            return yield_t{e, a};
        }
        CO_PP_template(class E, class A)(
            requires Executor<E> && Allocator<A>)
        constexpr auto operator()(A a, E e) const
        {
            return yield_t{e, a};
        }
    };

    inline constexpr yield_gen_t yield{};

    template<class Fn>
    struct[[nodiscard]] callable_with_implicit_context
      : Fn{callable_with_implicit_context(Fn fun) : Fn(std::move(fun)){}};

    template<class T>
    inline constexpr bool WantsExecutionContext =
        meta::is<T, callable_with_implicit_context>::value;

    CO_PP_template(class T)(
        requires CompletionToken<T>)
    auto get_allocator(T t)
    {
        return t.get_allocator();
    }

    CO_PP_template(class T)(
        requires CompletionToken<T>)
    auto get_executor(T t)
    {
        return t.get_executor();
    }

    template<class Token, class Ret, class Args, class Return>
    struct _async_result_impl_;

    template<class T, class Token>
    struct [[nodiscard]] task {
    private:
        template<class, class...>
        friend struct std::experimental::coroutine_traits;
        template<class, class, class, class>
        friend struct _async_result_impl_;
        static_assert(CompletionToken<Token>);

        struct promise_type
        {
            std::exception_ptr eptr_{};
            std::optional<T> value_{};
            std::optional<Token> token_{};
            std::experimental::coroutine_handle<> awaiter_{};
            std::function<void(std::experimental::coroutine_handle<>)> repost_;
            promise_type() = default;
            CO_PP_template(class... Ts)(
                requires Same<Token,
                              std::decay_t<meta::back<meta::list<Ts...>>>>)
            promise_type(Ts&&... args)
              : promise_type()
            {
                token_.emplace(_back(std::forward<Ts>(args)...));
            }
            Token const& get_token() const
            {
                return *token_;
            }
            void set_token(Token token)
            {
                token_.emplace(std::move(token));
            }
            auto get_executor() const
            {
                return coronet::get_executor(*token_);
            }
            auto get_allocator() const
            {
                return coronet::get_allocator(*token_);
            }
            auto initial_suspend() const noexcept
            {
                // For now, the INITIAL_SUSPEND macro is treated as the
                // coroutine's initial_suspend
                return std::experimental::suspend_never{};
                // return std::experimental::suspend_always{};
            }
            auto final_suspend() const noexcept
            {
                struct awaitable
                {
                    static bool await_ready() noexcept
                    {
                        return false;
                    }
                    auto await_suspend(
                        std::experimental::coroutine_handle<promise_type>
                            awaiter) const
                    {
                        assert(awaiter.promise().awaiter_ != nullptr);
                        if constexpr(meta::is<Token, _implicit_yield_t>::value)
                        {
                            return awaiter.promise().awaiter_;
                        }
                        else if(awaiter.promise().repost_)
                        {
                            // The awaiter has an execution context different
                            // than the current one. Repost the work there so
                            // the resume happens in the correct context.
                            awaiter.promise().repost_(
                                awaiter.promise().awaiter_);
                            return noop_coroutine();
                        }
                        // No way to repost since the awaiter didn't have
                        // an execution context.
                        return awaiter.promise().awaiter_;
                    }
                    static void await_resume() noexcept {}
                };
                return awaitable{};
            }
            void unhandled_exception() noexcept
            {
                eptr_ = std::current_exception();
            }
            void return_value(T value)
            {
                value_ = std::move(value);
            }
            task get_return_object() noexcept
            {
                return task{*this};
            }
            template<class U>
            auto await_transform(U t)
            {
                if constexpr(WantsExecutionContext<U>)
                    return t(_implicit(get_allocator()));
                else
                    return t;
            }
        };

        struct _awaitable
        {
            std::experimental::coroutine_handle<promise_type> coro_;
            bool await_ready() const
            {
                return coro_.promise().value_.has_value();
            }
            template<class Promise>
            std::experimental::coroutine_handle<> await_suspend(
                std::experimental::coroutine_handle<Promise> awaiter) const
            {
                coro_.promise().awaiter_ = awaiter;
                // schedule this coroutine to execute via the executor (unless
                // this coroutine and the calling coroutine have the same
                // completion token, in which case, just return coro_).
                auto const& token = coro_.promise().get_token();
                if constexpr(meta::is<Token, _implicit_yield_t>::value)
                {
                    // We're already in the correct execution context, just
                    // execute the coroutine.
                    return coro_;
                }
                // We're about to post this coroutine to another execution
                // context. We must repost back to awaiter's execution context
                // in final_suspend, otherwise the awaiter resumes in the
                // wrong context.
                else if constexpr(HasExecutionContext<Promise>)
                {
                    auto const& calling_token = awaiter.promise().get_token();
                    if constexpr(Same<decltype(token), decltype(calling_token)>)
                    {
                        using Alloc =
                            std::decay_t<decltype(token.get_allocator())>;
                        // Do the execution contexts compare equal?
                        if(token.get_executor() ==
                               calling_token.get_executor() &&
                           (typename std::allocator_traits<
                                Alloc>::is_always_equal() ||
                            token.get_allocator() ==
                                calling_token.get_allocator()))
                        {
                            // We're in the same execution context as our
                            // caller; just execute the coroutine.
                            return coro_;
                        }
                    }
                    // This lambda gets called with awaiter in final_suspend
                    coro_.promise().repost_ =
                        [calling_token](
                            std::experimental::coroutine_handle<> h) {
                            coronet::get_executor(calling_token)
                                .post(h, coronet::get_allocator(calling_token));
                        };
                }
                coronet::get_executor(token).post(
                    coro_, coronet::get_allocator(token));
                return noop_coroutine();
            }
            T await_resume() const
            {
                if(coro_.promise().eptr_)
                    std::rethrow_exception(coro_.promise().eptr_);
                return *coro_.promise().value_;
            }
        };

        std::experimental::coroutine_handle<promise_type> coro_{};

        task(promise_type & p)
          : coro_(
                std::experimental::coroutine_handle<promise_type>::from_promise(
                    p))
        {}

    public:
        task(task && that)
          : coro_(std::exchange(that.coro_, {}))
        {}
        ~task()
        {
            if(coro_)
                coro_.destroy();
        }
        auto operator co_await() const noexcept
        {
            return _awaitable{coro_};
        }
    };

    template<class T, class Token>
    struct void_
    {
    private:
        template<class, class...>
        friend struct std::experimental::coroutine_traits;
        template<class, class, class, class>
        friend struct _async_result_impl_;
        static_assert(CompletionToken<Token>);

        struct promise_type
        {
            std::exception_ptr eptr_{};
            T value_{};
            std::optional<Token> token_{};
            promise_type() = default;
            CO_PP_template(class... Ts)(
                requires Same<Token,
                              std::decay_t<meta::back<meta::list<Ts...>>>>)
                promise_type(Ts&&... args)
              : promise_type()
            {
                token_.emplace(_back(std::forward<Ts>(args)...));
            }
            auto get_executor() const
            {
                return coronet::get_executor(*token_);
            }
            auto get_allocator() const
            {
                return coronet::get_allocator(*token_);
            }
            Token const& get_token() const
            {
                return *token_;
            }
            void set_token(Token token)
            {
                token_.emplace(std::move(token));
                auto coro = std::experimental::coroutine_handle<
                    promise_type>::from_promise(*this);
                // Enque this asynchronous operation (detached)
                get_executor().post(coro, get_allocator());
            }
            auto initial_suspend() const noexcept
            {
                // For now, the INITIAL_SUSPEND macro is treated as the
                // coroutine's initial_suspend
                return std::experimental::suspend_never{};
                // return std::experimental::suspend_always{};
            }
            auto final_suspend() noexcept
            {
                // BUGBUG if the final_suspend is never reached, this coroutine
                // leaks.
                struct awaitable
                {
                    static constexpr bool await_ready() noexcept
                    {
                        return false;
                    }
                    void await_suspend(
                        std::experimental::coroutine_handle<promise_type>
                            awaiter)
                    {
                        auto token = std::move(awaiter.promise().token_);
                        auto value = std::move(awaiter.promise().value_);
                        auto eptr = awaiter.promise().eptr_;
                        awaiter.destroy();
                        if constexpr(!std::is_void_v<T>)
                            (*token)(eptr, value);
                        else
                            (*token)(eptr);
                    }
                    static void await_resume() noexcept {}
                };
                return awaitable{};
            }
            void unhandled_exception() noexcept
            {
                eptr_ = std::current_exception();
            }
            void return_value(T value)
            {
                value_ = std::move(value);
            }
            void_ get_return_object() noexcept
            {
                return void_{};
            }
            template<class U>
            auto await_transform(U t)
            {
                if constexpr(WantsExecutionContext<U>)
                    return t(_implicit(get_allocator()));
                else
                    return t;
            }
        };
    };

    // This becomes the de facto initial_suspend until the promise type
    // constructor gets passed the completion token.
    template<class Token>
    struct _try_set_token_
    {
        Token token_;
        _try_set_token_(Token token)
          : token_(std::move(token))
        {}
        bool await_ready() const noexcept
        {
            return false;
        }
        CO_PP_template(class Promise)(
            requires HasExecutionContext<Promise>)
            void await_suspend(
                std::experimental::coroutine_handle<Promise> awaiter)
        {
            awaiter.promise().set_token(std::move(token_));
        }
        void await_resume() const noexcept {}
    };

// This won't be needed once we can pass the coroutine arguments to the
// promise type's constructor.
#define INITIAL_SUSPEND(token) \
    (void)(co_await ::coronet::_try_set_token_{token})

    // An asynchronous API wrapper that also provides an overload that does
    // not take a completion token, the semantics being to execute as a
    // coroutine with the execution context (allocator and executor) taken
    // from the calling coroutine.
    template<class Fn>
    struct async
    {
    private:
        Fn fn_;

    public:
        constexpr async(Fn fun)
          : fn_(std::move(fun))
        {}

        CO_PP_template(class... Ts)(
            requires Invocable<const Fn&, Ts...>)
        auto operator()(Ts... ts) const
        {
            using Ret = decltype(fn_(std::move(ts)...));
            // If this async operation returns coronet::void_, just return void.
            if constexpr(meta::is<Ret, void_>::value)
            {
                (void)fn_(std::move(ts)...);
            }
            else
            {
                return fn_(std::move(ts)...);
            }
        }

        CO_PP_template(class... Ts)(
            requires Invocable<const Fn&, const Ts&..., _implicit_yield_t<>>)
        auto operator()(Ts... ts) const
        {
            return callable_with_implicit_context{
                [ts..., this](auto token) { return fn_(ts..., token); }};
        }
    };

    template<class Token, class Fn>
    struct _callback_token
      : Token
      , Fn
    {
        _callback_token(Token token, Fn fn)
          : Token(token)
          , Fn(fn)
        {}
    };

    template<class E, class A = std::allocator<void>>
    struct via
    {
        static_assert(Executor<E>);
        static_assert(Allocator<A>);
        E exec_;
        A alloc_;

        CO_PP_template()(
            requires Executor<E> && Allocator<A>)
        constexpr explicit via(E e, A a = A{})
          : exec_(e)
          , alloc_(a)
        {}
        CO_PP_template()(
            requires Executor<E> && Allocator<A>)
        constexpr via(A a, E e)
          : exec_(e)
          , alloc_(a)
        {}
        auto get_allocator() const
        {
            return alloc_;
        }
        auto get_executor() const
        {
            return exec_;
        }

        template<class Fn>
        friend auto operator|(Fn fn, via ctx)
        {
            return _callback_token{ctx, fn};
        }
    };

    template<class Token, class Sig>
    struct _result_;

    template<class Token, class Ret, class... Args>
    struct _result_<Token, Ret(Args...)>
    {
        using type = typename std::experimental::net::async_result<
            Token, meta::if_<std::is_void<Ret>, void(std::exception_ptr),
                             void(std::exception_ptr, Ret)>>::return_type;
    };

    template<class Token, class Sig>
    using result_t = meta::_t<_result_<Token, Sig>>;

    // The completion token stores a callback
    template<class Token, class Ret, class Args, class Return>
    struct _async_result_impl_
    {
    private:
        static_assert(std::is_void_v<Ret>);
        static_assert(meta::size<Args>::value < 3u &&
                      meta::size<Args>::value != 0);
        static_assert(
            Same<std::exception_ptr, std::decay_t<meta::front<Args>>>);
        using _result_type =
            meta::if_c<1 == meta::size<Args>::value, void, meta::back<Args>>;

    public:
        using return_type = meta::invoke<Return, _result_type, Token>;
        using completion_handler_type = typename return_type::promise_type;

        _async_result_impl_(completion_handler_type& handler)
          : ret_{handler.get_return_object()}
        {}
        return_type get()
        {
            return std::move(ret_);
        }

    private:
        return_type ret_;
    };
} // namespace coronet

namespace std::experimental::net
{
    template<class Token, class Fn, class Ret, class... Args>
    struct async_result<coronet::_callback_token<Token, Fn>, Ret(Args...)>
      : coronet::_async_result_impl_<coronet::_callback_token<Token, Fn>, Ret,
                                     meta::list<Args...>,
                                     meta::quote<coronet::void_>>
    {
        static_assert((1 == sizeof...(Args) &&
                       coronet::Invocable<Fn&, std::exception_ptr>) ||
                      coronet::Invocable<Fn&, std::exception_ptr,
                                         meta::back<meta::list<Args...>>>);
        using async_result::_async_result_impl_::_async_result_impl_;
    };

    template<class Executor, class Allocator, class Ret, class... Args>
    struct async_result<coronet::yield_t<Executor, Allocator>, Ret(Args...)>
      : coronet::_async_result_impl_<coronet::yield_t<Executor, Allocator>, Ret,
                                     meta::list<Args...>,
                                     meta::quote<coronet::task>>
    {
        using async_result::_async_result_impl_::_async_result_impl_;
    };

    template<class Executor, class Allocator, class Ret, class... Args>
    struct async_result<coronet::via<Executor, Allocator>, Ret(Args...)>
    {
        static_assert(meta::invoke<meta::id<std::false_type>, Args...>::value,
                      "You must specify a continuation when using "
                      "coronet::via(Executor [, Allocator]).");
        using return_type = void;
        using completion_handler_type = coronet::via<Executor, Allocator>;
        async_result(completion_handler_type& handler);
        return_type get();
    };

    template<class Allocator, class Ret, class... Args>
    struct async_result<coronet::_implicit_yield_t<Allocator>, Ret(Args...)>
      : coronet::_async_result_impl_<coronet::_implicit_yield_t<Allocator>, Ret,
                                     meta::list<Args...>,
                                     meta::quote<coronet::task>>
    {
        using async_result::_async_result_impl_::_async_result_impl_;
    };
} // namespace std::experimental::net

namespace std::experimental
{
    template<class T, class Token, class... Args>
    struct coroutine_traits<coronet::task<T, Token>, Args...>
    {
        using promise_type = typename coronet::task<T, Token>::promise_type;
    };
    template<class T, class Token, class... Args>
    struct coroutine_traits<coronet::void_<T, Token>, Args...>
    {
        using promise_type = typename coronet::void_<T, Token>::promise_type;
    };
} // namespace std::experimental

#endif
