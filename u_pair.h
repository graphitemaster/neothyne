#ifndef U_PAIR_HDR
#define U_PAIR_HDR
#include "u_traits.h"

namespace u {

namespace detail {
    enum class pair_type {
        none, first, second, both
    };

    template <bool isSame, bool firstEmpty, bool secondEmpty>
    struct choose_pair;
    template <bool isSame>
    struct choose_pair<isSame, false, false> : integral_constant<pair_type, pair_type::none> {};
    template <bool isSame>
    struct choose_pair<isSame, true, false> : integral_constant<pair_type, pair_type::first> {};
    template <bool isSame>
    struct choose_pair<isSame, false, true> : integral_constant<pair_type, pair_type::second> {};
    template <>
    struct choose_pair<false, true, true> : integral_constant<pair_type, pair_type::both> {};
    template <>
    struct choose_pair<true, true, true> : integral_constant<pair_type, pair_type::first> {};

    template <typename T1, typename T2>
    using choose_compress = choose_pair<
        is_same<typename remove_cv<T1>::type,
                typename remove_cv<T2>::type>::value,
        is_empty<T1>::value,
        is_empty<T2>::value
    >;

    template <typename T1, typename T2, pair_type = choose_compress<T1, T2>::value>
    struct compressed_pair;

    ///! NONE
    template <typename T1, typename T2>
    struct compressed_pair<T1, T2, pair_type::none> {
        using first_type = T1;
        using second_type = T2;
        using first_reference = typename remove_reference<T1>::type &;
        using second_reference = typename remove_reference<T2>::type &;
        using first_const_reference = typename remove_reference<T1>::type const &;
        using second_const_reference = typename remove_reference<T2>::type const &;

        constexpr compressed_pair();
        constexpr compressed_pair(T1 f, T2 s);

        first_reference first();
        constexpr first_const_reference first() const;
        second_reference second();
        constexpr second_const_reference second() const;

        void swap(compressed_pair &other);

    private:
        T1 m_first;
        T2 m_second;
    };

    template <typename T1, typename T2>
    inline constexpr compressed_pair<T1, T2, pair_type::none>::compressed_pair()
        : m_first()
        , m_second()
    {
    }

    template <typename T1, typename T2>
    inline constexpr compressed_pair<T1, T2, pair_type::none>::compressed_pair(T1 f, T2 s)
        : m_first(f)
        , m_second(s)
    {
    }

    template <typename T1, typename T2>
    inline typename compressed_pair<T1, T2, pair_type::none>::first_reference
    compressed_pair<T1, T2, pair_type::none>::first() {
        return m_first;
    }

    template <typename T1, typename T2>
    inline constexpr typename compressed_pair<T1, T2, pair_type::none>::first_const_reference
    compressed_pair<T1, T2, pair_type::none>::first() const {
        return m_first;
    }

    template <typename T1, typename T2>
    inline typename compressed_pair<T1, T2, pair_type::none>::second_reference
    compressed_pair<T1, T2, pair_type::none>::second() {
        return m_second;
    }

    template <typename T1, typename T2>
    inline constexpr typename compressed_pair<T1, T2, pair_type::none>::second_const_reference
    compressed_pair<T1, T2, pair_type::none>::second() const {
        return m_second;
    }

    template <typename T1, typename T2>
    inline void compressed_pair<T1, T2, pair_type::none>::swap(compressed_pair &other) {
        swap(m_first, other.m_first);
        swap(m_second, other.m_second);
    }

    ///! FIRST
    template <typename T1, typename T2>
    struct compressed_pair<T1, T2, pair_type::first> : private T1 {
        using first_type = T1;
        using second_type = T2;
        using first_reference = T1 &;
        using second_reference = typename remove_reference<T2>::type &;
        using first_const_reference = T1 const &;
        using second_const_reference = typename remove_reference<T2>::type const &;

        constexpr compressed_pair();
        constexpr compressed_pair(T1 f, T2 s);

        first_reference first();
        constexpr first_const_reference first() const;
        second_reference second();
        constexpr second_const_reference second() const;

        void swap(compressed_pair &other);

    private:
        T2 m_second;
    };

    template <typename T1, typename T2>
    inline constexpr compressed_pair<T1, T2, pair_type::first>::compressed_pair()
        : T1()
        , m_second()
    {
    }

    template <typename T1, typename T2>
    inline constexpr compressed_pair<T1, T2, pair_type::first>::compressed_pair(T1 f, T2 s)
        : T1(forward<T1>(f))
        , m_second(forward<T2>(s))
    {
    }

    template <typename T1, typename T2>
    inline typename compressed_pair<T1, T2, pair_type::first>::first_reference
    compressed_pair<T1, T2, pair_type::first>::first() {
        return *this;
    }

    template <typename T1, typename T2>
    inline constexpr typename compressed_pair<T1, T2, pair_type::first>::first_const_reference
    compressed_pair<T1, T2, pair_type::first>::first() const {
        return *this;
    }

    template <typename T1, typename T2>
    inline typename compressed_pair<T1, T2, pair_type::first>::second_reference
    compressed_pair<T1, T2, pair_type::first>::second() {
        return m_second;
    }

    template <typename T1, typename T2>
    inline constexpr typename compressed_pair<T1, T2, pair_type::first>::second_const_reference
    compressed_pair<T1, T2, pair_type::first>::second() const {
        return m_second;
    }

    template <typename T1, typename T2>
    inline void compressed_pair<T1, T2, pair_type::first>::swap(compressed_pair &other) {
        swap(m_second, other.m_second);
    }

    ///! SECOND
    template <typename T1, typename T2>
    struct compressed_pair<T1, T2, pair_type::second> : private T2 {
        using first_type = T1;
        using second_type = T2;
        using first_reference = typename remove_reference<T1>::type &;
        using second_reference = T2 &;
        using first_const_reference = typename remove_reference<T2>::type const &;
        using second_const_reference = T2 const &;

        constexpr compressed_pair();
        constexpr compressed_pair(T1 f, T2 s);

        first_reference first();
        constexpr first_const_reference first() const;
        second_reference second();
        constexpr second_const_reference second() const;

        void swap(compressed_pair &);

    private:
        T1 m_first;
    };

    template <typename T1, typename T2>
    inline constexpr compressed_pair<T1, T2, pair_type::second>::compressed_pair()
        : T2()
        , m_first()
    {
    }

    template <typename T1, typename T2>
    inline constexpr compressed_pair<T1, T2, pair_type::second>::compressed_pair(T1 f, T2 s)
        : T2(forward<T2>(s))
        , m_first(forward<T1>(f))
    {
    }

    template <typename T1, typename T2>
    inline typename compressed_pair<T1, T2, pair_type::second>::first_reference
    compressed_pair<T1, T2, pair_type::second>::first() {
        return m_first;
    }

    template <typename T1, typename T2>
    inline constexpr typename compressed_pair<T1, T2, pair_type::second>::first_const_reference
    compressed_pair<T1, T2, pair_type::second>::first() const {
        return m_first;
    }

    template <typename T1, typename T2>
    inline typename compressed_pair<T1, T2, pair_type::second>::second_reference
    compressed_pair<T1, T2, pair_type::second>::second() {
        return *this;
    }

    template <typename T1, typename T2>
    inline constexpr typename compressed_pair<T1, T2, pair_type::second>::second_const_reference
    compressed_pair<T1, T2, pair_type::second>::second() const {
        return *this;
    }

    template <typename T1, typename T2>
    inline void compressed_pair<T1, T2, pair_type::second>::swap(compressed_pair &other) {
        swap(m_first, other.m_first);
    }

    ///! BOTH
    template <typename T1, typename T2>
    struct compressed_pair<T1, T2, pair_type::both> : private T1, T2 {
        using first_type = T1;
        using second_type = T2;
        using first_reference = T1 &;
        using second_reference = T2 &;
        using first_const_reference = T1 const &;
        using second_const_reference = T2 const &;

        constexpr compressed_pair();
        constexpr compressed_pair(T1 f, T2 s);

        first_reference first();
        constexpr first_const_reference first() const;
        second_reference second();
        constexpr second_const_reference second() const;

        void swap(compressed_pair &);
    };

    template <typename T1, typename T2>
    inline constexpr compressed_pair<T1, T2, pair_type::both>::compressed_pair()
        : T1()
        , T2()
    {
    }

    template <typename T1, typename T2>
    inline constexpr compressed_pair<T1, T2, pair_type::both>::compressed_pair(T1 f, T2 s)
        : T1(forward<T1>(f))
        , T2(forward<T2>(s))
    {
    }

    template <typename T1, typename T2>
    inline typename compressed_pair<T1, T2, pair_type::both>::first_reference
    compressed_pair<T1, T2, pair_type::both>::first() {
        return *this;
    }

    template <typename T1, typename T2>
    inline constexpr typename compressed_pair<T1, T2, pair_type::both>::first_const_reference
    compressed_pair<T1, T2, pair_type::both>::first() const {
        return *this;
    }

    template <typename T1, typename T2>
    inline typename compressed_pair<T1, T2, pair_type::both>::second_reference
    compressed_pair<T1, T2, pair_type::both>::second() {
        return *this;
    }

    template <typename T1, typename T2>
    inline constexpr typename compressed_pair<T1, T2, pair_type::both>::second_const_reference
    compressed_pair<T1, T2, pair_type::both>::second() const {
        return *this;
    }

    template <typename T1, typename T2>
    inline void compressed_pair<T1, T2, pair_type::both>::swap(compressed_pair &) {
        // Cannot swap
    }
};

template <typename T1, typename T2>
struct pair : private detail::compressed_pair<T1, T2> {
private:
    using base = detail::compressed_pair<T1, T2>;
public:
    using first_type = typename base::first_type;
    using second_type = typename base::second_type;
    using first_reference = typename base::first_reference;
    using second_reference = typename base::second_reference;
    using first_const_reference = typename base::first_const_reference;
    using second_const_reference = typename base::second_const_reference;

    constexpr pair();
    constexpr pair(T1 f, T2 s);

    first_reference first();
    constexpr first_const_reference first() const;
    second_reference second();
    constexpr second_const_reference second() const;

    void swap(pair &other);
};

template <typename T1, typename T2>
inline constexpr pair<T1, T2>::pair()
    : base()
{
}

template <typename T1, typename T2>
inline constexpr pair<T1, T2>::pair(T1 f, T2 s)
    : base(forward<T1>(f), forward<T2>(s))
{
}

template <typename T1, typename T2>
inline typename pair<T1, T2>::first_reference pair<T1, T2>::first() {
    return base::first();
}

template <typename T1, typename T2>
inline constexpr typename pair<T1, T2>::first_const_reference pair<T1, T2>::first() const {
    return base::first();
}

template <typename T1, typename T2>
inline typename pair<T1, T2>::second_reference pair<T1, T2>::second() {
    return base::second();
}

template <typename T1, typename T2>
inline constexpr typename pair<T1, T2>::second_const_reference pair<T1, T2>::second() const {
    return base::second();
}

template <typename T1, typename T2>
inline void pair<T1, T2>::swap(pair &other) {
    base::swap(other);
}

template <typename T1, typename T2>
inline pair<typename decay<T1>::type, typename decay<T2>::type>
make_pair(T1 &&first, T2 &&second) {
    return pair<typename decay<T1>::type, typename decay<T2>::type>(forward<T1>(first), forward<T2>(second));
}

}
#endif
