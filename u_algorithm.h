#ifndef U_ALGORITHM_HDR
#define U_ALGORITHM_HDR
#include "u_traits.h" // move

namespace u {

template <typename T>
inline void swap(T &lhs, T &rhs) {
    T temp = move(rhs);
    rhs = move(lhs);
    lhs = move(temp);
}

template <typename I, typename T>
inline I find(I first, I last, const T &value) {
    while (first != last) {
        if (*first == value)
            return first;
        ++first;
    }
    return last;
}

template <typename I1, typename I2>
I1 search(I1 first1, I1 last1, I2 first2, I2 last2) {
    for (; ; ++first1) {
        I1 it1 = first1;
        for (I2 it2 = first2; ; ++it1, ++it2) {
            if (it2 == last2)
                return first1;
            if (it1 == last1)
                return last1;
            if (!(*it1 == *it2))
                break;
        }
    }
}

template <typename T>
inline constexpr T min(const T &lhs, const T &rhs) {
    return lhs < rhs ? lhs : rhs;
}

template <typename T>
inline constexpr T max(const T &lhs, const T &rhs) {
    return lhs > rhs ? lhs : rhs;
}

template <typename T>
inline constexpr T abs(const T &lhs) {
    return lhs < 0 ? -lhs : lhs;
}

}

#endif
