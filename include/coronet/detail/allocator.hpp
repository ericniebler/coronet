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
#ifndef CORONET_DETAIL_ALLOCATOR_HPP
#define CORONET_DETAIL_ALLOCATOR_HPP

#include <memory>
#include <new>
#include <utility>

#include <coronet/detail/concepts.hpp>

namespace coronet
{
    struct CAllocator
    {
        template<class A, class T, class = typename A::value_type>
        using _rebind_alloc =
            typename std::allocator_traits<A>::template rebind_alloc<T>;

        template<class A, class T = typename _rebind_alloc<A, int>::value_type>
        auto requires_(_rebind_alloc<A, int>& a, T* p, std::size_t n)
            -> decltype(requires_<CSame, T*, int*>,
                        a.allocate(n)->*satisfies<CSame, T*>,
                        a.deallocate(p, n),
                        (a == a)->*satisfies<CConvertibleTo, bool>,
                        (a != a)->*satisfies<CConvertibleTo, bool>);
    };

    template<class A>
    inline constexpr bool Allocator = is_satisfied_by<CAllocator, A>;

    CO_PP_template(class A, class T)(
        requires Allocator<A> && std::is_object_v<T>)
    using rebind_alloc = CAllocator::_rebind_alloc<A, T>;

    template<class T>
    struct _allocator_archetype
    {
        using value_type = T;
        T* allocate(std::size_t);
        void deallocate(T*, std::size_t);
        friend bool operator==(_allocator_archetype, _allocator_archetype);
        friend bool operator!=(_allocator_archetype, _allocator_archetype);
    };

    struct allocator_base
    {
    private:
        struct interface
        {
            virtual ~interface() {}
            virtual void* allocate(std::size_t) = 0;
            virtual void deallocate(void*, std::size_t) = 0;
            virtual std::unique_ptr<interface> clone() const = 0;
            virtual std::type_info const& type() const noexcept = 0;
            virtual bool equal_to(interface const* that) const noexcept = 0;
        };
        template<class T>
        struct model
          : interface
          , T
        {
            model(T t)
              : T(std::move(t))
            {}
            void* allocate(std::size_t n) override
            {
                return static_cast<T&>(*this).allocate(n);
            }
            void deallocate(void* p, std::size_t n) override
            {
                static_cast<T&>(*this).deallocate(static_cast<char*>(p), n);
            }
            std::unique_ptr<interface> clone() const override
            {
                return std::make_unique<model<T>>(static_cast<T const&>(*this));
            }
            std::type_info const& type() const noexcept override
            {
                return typeid(T);
            }
            bool equal_to(interface const* that) const noexcept override
            {
                if(auto* p = dynamic_cast<model const*>(that))
                    return static_cast<T const&>(*p) ==
                           static_cast<T const&>(*this);
                return false;
            }
        };
        std::unique_ptr<interface> impl_;

    public:
        allocator_base() = default;
        allocator_base(allocator_base&&) = default;
        allocator_base(allocator_base const& that)
          : impl_(that.impl_ ? nullptr : that.impl_->clone())
        {}
        template<class T_, class T = rebind_alloc<std::decay_t<T_>, char>>
        allocator_base(T_&& t)
          // TODO: use the allocator to allocate the model
          : impl_(std::make_unique<model<T>>(std::forward<T_>(t)))
        {}
        allocator_base& operator=(allocator_base&&) = default;
        allocator_base& operator=(allocator_base const& that)
        {
            impl_ = that.impl_ ? nullptr : that.impl_->clone();
            return *this;
        }
        template<class T_, class T = rebind_alloc<std::decay_t<T_>, char>>
        allocator_base& operator=(T_&& t)
        {
            // TODO: use the allocator to allocate the model
            impl_ = std::make_unique<model<T>>(std::forward<T_>(t));
            return *this;
        }
        void* allocate(std::size_t n)
        {
            return impl_->allocate(n);
        }
        void deallocate(void* p, std::size_t n)
        {
            impl_->deallocate(p, n);
        }
        friend bool operator==(allocator_base const& a, allocator_base const& b)
        {
            return (!a.impl_ && !b.impl_) || a.impl_->equal_to(a.impl_.get());
        }
        friend bool operator!=(allocator_base const& a, allocator_base const& b)
        {
            return !(a == b);
        }
    };

    template<class T = void>
    struct allocator : private allocator_base
    {
        using value_type = T;

        using allocator_base::allocator_base;
        allocator() = default;
        CO_PP_template(class U)(
            requires !Same<T, U>)
        allocator(allocator<U> other)
          : allocator_base(std::move(other))
        {}
        CO_PP_template(class U)(
            requires !Same<T, U>)
        allocator& operator=(allocator<U> other)
        {
            static_cast<allocator_base&>(*this) = std::move(other);
            return *this;
        }
        T* allocate(std::size_t n)
        {
            return static_cast<T*>(allocator_base::allocate(n * sizeof(T)));
        }
        void deallocate(T* p, std::size_t n)
        {
            allocator_base::deallocate(p, n * sizeof(T));
        }
    };
}

#endif
