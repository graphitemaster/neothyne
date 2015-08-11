#ifndef U_PAIR_HDR
#define U_PAIR_HDR
#include "u_traits.h"

namespace u {

template <typename T1, typename T2>
struct pair {
    typedef T1 first_type;
    typedef T2 second_type;

    T1 first;
    T2 second;

    constexpr pair();
    constexpr pair(const T1 &x, const T2 &y);

    pair(const pair &p) = default;
    pair(pair &&p) = default;

    pair &operator=(const pair &p);
    pair &operator=(pair &&p);
};

template <typename T1, typename T2>
inline constexpr pair<T1, T2>::pair()
    : first()
    , second()
{
}

template <typename T1, typename T2>
inline constexpr pair<T1, T2>::pair(const T1 &x, const T2 &y)
    : first(x)
    , second(y)
{
}

template <typename T1, typename T2>
inline pair<T1, T2> &pair<T1, T2>::operator=(const pair &p) {
    first = p.first;
    second = p.second;
    return *this;
}

template <typename T1, typename T2>
inline pair<T1, T2> &pair<T1, T2>::operator=(pair &&p) {
    first = u::forward<first_type>(p.first);
    second = u::forward<second_type>(p.second);
    return *this;
}

namespace detail {
    template <typename T>
    struct reference_wrapper;

    template <typename T>
    struct make_pair_type {
        typedef T type;
    };
    template <typename T>
    struct make_pair_type<reference_wrapper<T>> {
        typedef T &type;
    };

    template <typename T>
    struct make_pair_return {
        typedef typename make_pair_type<typename u::decay<T>::type>::type type;
    };
}

template <typename T1, typename T2>
inline constexpr pair<typename detail::make_pair_return<T1>::type,
                      typename detail::make_pair_return<T2>::type>
make_pair(T1 &&t1, T2 &&t2) {
    return pair<typename detail::make_pair_return<T1>::type,
                typename detail::make_pair_return<T2>::type>(u::forward<T1>(t1),
                                                             u::forward<T2>(t2));
}

}
#endif
