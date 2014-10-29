#ifndef U_PAIR_HDR
#define U_PAIR_HDR
#include "u_traits.h"

namespace u {

// unlike the standard librarie's implementation of pair. This one implements
// empty-base-class-optimization. If one of the template parameters contains
// an incomplete type or an empty type (void, empty class, etc), the representation
// of the class will consume only the memory of the one type which isn't.
// in other words, sizeof(pair<int, void>) == sizeof(int) or, struct foo {};
// sizeof(pair<foo, int>) == sizeof(int).
namespace detail {
    template <typename T>
    struct pair_first {
        T first;
        template <typename U>
        pair_first(U&& value);
    };

    template <typename T>
    template <typename U>
    inline pair_first<T>::pair_first(U &&value)
        : first(value)
    {
    }
}

template <typename T1, typename T2>
struct pair : detail::pair_first<T1> {
    T2 second;
    pair() = default;
    ~pair() = default;

    template <typename U1, typename U2>
    pair(U1 &&first, U2 &&second);

    pair(pair &&other);

    pair(const pair &other);

    template <size_t N>
    typename conditional<N == 0, T1,
        typename conditional<N == 1, T2, void>::type>::type
    &get();

    template <size_t N>
    typename conditional<N == 0, const T1,
        typename conditional<N == 1, const T2, void>::type>::type
    &get() const;

    pair &operator=(const pair &other);
    pair &operator=(pair &&other);

private:
    enum class get_first { value };
    enum class get_second { value };

private:
    T1 &query(get_first);
    T2 &query(get_second);
};

template <typename T1, typename T2>
template <typename U1, typename U2>
inline pair<T1, T2>::pair(U1 &&first, U2 &&second)
    : detail::pair_first<T1>(forward<U1>(first))
    , second(forward<U2>(second))
{
}

template <typename T1, typename T2>
inline pair<T1, T2>::pair(pair<T1, T2> &&other)
    : detail::pair_first<T1>(move(other.first))
    , second(move(other.second))
{
}

template <typename T1, typename T2>
inline T1 &pair<T1, T2>::query(pair<T1, T2>::get_first) {
    return this->first;
}

template <typename T1, typename T2>
inline T2 &pair<T1, T2>::query(pair<T1, T2>::get_second) {
    return second;
}

template <typename T1, typename T2>
template <size_t N>
inline typename conditional<N == 0, T1,
    typename conditional<N == 1, T2, void>::type
>::type &pair<T1, T2>::get() {
    return query(conditional<N == 0,
        pair<T1, T2>::get_first,
        pair<T1, T2>::get_second>::type::value
    );
}

template <typename T1, typename T2>
template <size_t N>
inline typename conditional<N == 0, const T1,
    typename conditional<N == 1, const T2, void>::type
>::type &pair<T1, T2>::get() const {
    return query(conditional<N == 0,
        pair<T1, T2>::get_first,
        pair<T1, T2>::get_second>::type::value
    );
}

template <typename T1, typename T2>
inline pair<T1, T2> &pair<T1, T2>::operator=(const pair<T1, T2> &other) {
    this->first = other.first;
    this->second = other.second;
    return *this;
}

template <typename T1, typename T2>
inline pair<T1, T2> &pair<T1, T2>::operator=(pair<T1, T2> &&other) {
    this->first = move(other.first);
    this->second = move(other.second);
    return *this;
}

// std::get like support on pair
template <size_t N, typename T1, typename T2>
inline typename conditional<N == 0, T1,
    typename conditional<N == 1, T2, void>::type
>::type get(pair<T1, T2> &p) {
    return p.template get<N>();
}

template <size_t N, typename T1, typename T2>
inline typename conditional<N == 0, typename add_const<T1>::type,
    typename conditional<N == 1, typename add_const<T2>::type, void>::type
>::type get(const pair<T1, T2> &p) {
    return p.template get<N>();
}

}
#endif
