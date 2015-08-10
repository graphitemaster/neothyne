#ifndef U_OPTIONAL_HDR
#define U_OPTIONAL_HDR
#include "u_memory.h"

namespace u {

struct in_place_t {
    // Empty
};

constexpr in_place_t in_place {};

struct nullopt_t {
    explicit constexpr nullopt_t(int) noexcept {
        // Empty
    }
};

constexpr nullopt_t none {0};

namespace detail {
    template <typename T, bool = is_trivially_destructible<T>::value>
    struct storage {
    protected:
        typedef T value_type;

        ~storage() {
            if (m_engaged)
                m_value.~value_type();
        }

        constexpr storage()
            : m_nil('\0')
            , m_engaged(false)
        {
        }

        storage(const storage &x)
            : m_engaged(x.m_engaged)
        {
            if (m_engaged)
                new (u::addressof(m_value)) value_type(x.m_value);
        }

        storage(storage &&x)
            : m_engaged(x.m_engaged)
        {
            if (m_engaged)
                new (u::addressof(m_value)) value_type(u::move(x.m_value));
        }

        constexpr storage(const value_type &v)
            : m_value(v)
            , m_engaged(true)
        {
        }

        constexpr storage(value_type &&v)
            : m_value(u::move(v))
            , m_engaged(true)
        {
        }

        template <typename ...A>
        constexpr explicit storage(in_place_t, A &&...args)
            : m_value(u::forward<A>(args)...)
            , m_engaged(true)
        {
        }

        union {
            char m_nil;
            value_type m_value;
        };
        bool m_engaged;
    };

    template <typename T>
    struct storage<T, true> {
    protected:
        typedef T value_type;

        constexpr storage()
            : m_nil('\0')
            , m_engaged(false)
        {
        }

        storage(const storage &x)
            : m_engaged(x.m_engaged)
        {
            if (m_engaged)
                new (u::addressof(m_value)) value_type(x.m_value);
        }

        storage(storage &&x)
            : m_engaged(x.m_engaged)
        {
            if (m_engaged)
                new (u::addressof(m_value)) value_type(u::move(x.m_value));
        }

        constexpr storage(const value_type &v)
            : m_value(v)
            , m_engaged(true)
        {
        }

        constexpr storage(value_type &&v)
            : m_value(u::move(v))
            , m_engaged(true)
        {
        }

        template <typename ...A>
        constexpr explicit storage(in_place_t, A &&...args)
            : m_value(u::forward<A>(args)...)
            , m_engaged(true)
        {
        }

        union {
            char m_nil;
            value_type m_value;
        };
        bool m_engaged;
    };

    template <typename, typename T>
    struct ignore {
        typedef T type;
    };

    template <typename T>
    struct has_operator_addressof_member {
        template <typename U>
        static auto test(int) ->
            typename ignore<decltype(u::declval<U>().operator&()), true_type>::type;
        template <typename>
        false_type test(long);
        static const bool value = decltype(test<T>(0))::value;
    };

    template <typename T>
    struct has_operator_addressof_free {
        template <typename U>
        static auto test(int) ->
            typename ignore<decltype(operator&(u::declval<U>())), true_type>::type;
        template <typename>
        false_type test(long);
        static const bool value = decltype(test<T>(0))::value;
    };

    template <typename T>
    struct has_operator_addressof
        : integral_constant<bool, has_operator_addressof_member<T>::value ||
                                  has_operator_addressof_free<T>::value> { };
}

template <typename T>
struct optional : detail::storage<T> {
    typedef T value_type;

    constexpr optional() = default;
    optional(const optional &) = default;
    optional(optional &&) = default;
    ~optional() = default;

    constexpr optional(nullopt_t);
    constexpr optional(const value_type &v);
    constexpr optional(value_type &&v);

    optional &operator=(nullopt_t);
    optional &operator=(const optional &);
    optional &operator=(optional &&);

    constexpr value_type const* operator->() const;
    value_type *operator->();
    constexpr const value_type &operator*() const;
    value_type &operator*();

    constexpr explicit operator bool() const;

private:
    typedef detail::storage<T> m_base;

    value_type const *access(true_type) const;
    constexpr value_type const *access(false_type) const;
};

template <typename T>
inline typename optional<T>::value_type const *optional<T>::access(true_type) const {
    return u::addressof(this->m_value);
}

template <typename T>
constexpr typename optional<T>::value_type const *optional<T>::access(false_type) const {
    return &this->m_value;
}

template <typename T>
inline constexpr optional<T>::optional(nullopt_t) {
    // Empty
}

template <typename T>
inline constexpr optional<T>::optional(const value_type &v)
    : m_base(v)
{
    // Empty
}

template <typename T>
inline constexpr optional<T>::optional(value_type &&v)
    : m_base(u::move(v))
{
    // Empty
}

template <typename T>
inline constexpr typename optional<T>::value_type const* optional<T>::operator->() const {
    return access(detail::has_operator_addressof<value_type> {});
}

template <typename T>
inline typename optional<T>::value_type *optional<T>::operator->() {
    return u::addressof(this->m_value);
}

template <typename T>
inline constexpr const typename optional<T>::value_type &optional<T>::operator*() const {
    return this->m_value;
}

template <typename T>
inline typename optional<T>::value_type &optional<T>::operator*() {
    return this->m_value;
}

template <typename T>
inline constexpr optional<T>::operator bool() const {
    return this->m_engaged;
}

}

#endif
