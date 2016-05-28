#ifndef U_ALGORITHM_HDR
#define U_ALGORITHM_HDR
#include "u_traits.h" // move

namespace u {

template <typename T>
inline void swap(T &lhs, T &rhs) {
    T temp = u::move(rhs);
    rhs = u::move(lhs);
    lhs = u::move(temp);
}

// leazy sorting shamefully taken from tesseract
template <typename T, typename F>
static inline void insertion_sort(T *start, T *end, F fun) {
    for (T *i = start+1; i < end; i++) {
        if (fun(*i, i[-1])) {
            T temp = u::move(*i);
            *i = u::move(i[-1]);
            T *j = i-1;
            for (; j > start && fun(temp, j[-1]); --j)
                *j = u::move(j[-1]);
            *j = u::move(temp);
        }
    }
}

template <typename T, typename F>
static inline void sort(T *start, T *end, F fun) {
    while (end - start > 10) {
        T *mid = &start[(end-start)/2];
        T *i = start+1;
        T *j = end-2;
        T pivot;
        if (fun(*start, *mid)) {
            // start < mid
            if (fun(end[-1], *start)) {
                // end < start < mid
                pivot = u::move(*start);
                *start = u::move(end[-1]);
                end[-1] = u::move(*mid);
            } else if (fun(end[-1], *mid)) {
                // start <= end < mid
                pivot = u::move(end[-1]);
                end[-1] = u::move(*mid);
            } else {
                pivot = u::move(*mid);
            }
        } else if (fun(*start, end[-1])) {
            // mid <= start < end
            pivot = u::move(*start);
            *start = u::move(*mid);
        } else if (fun(*mid, end[-1])) {
            // mid < end <= start
            pivot = u::move(end[-1]);
            end[-1] = u::move(*start);
            *start = u::move(*mid);
        } else {
            pivot = u::move(*mid);
            u::swap(*start, end[-1]);
        }
        *mid = u::move(end[-2]);
        do {
            while (fun(*i, pivot))
                if (++i >= j)
                    goto partitioned;
            while (fun(pivot, *--j))
                if (i >= j)
                    goto partitioned;
            u::swap(*i, *j);
        } while (++i < j);
partitioned:
        end[-2] = u::move(*i);
        *i = u::move(pivot);
        if (i-start < end-i+1) {
            sort(start, i, fun);
            start = i+1;
        } else {
            sort(i+1, end, fun);
            end = i;
        }
    }
    insertion_sort(start, end, fun);
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

template <typename T>
inline constexpr T square(const T &value) {
    return value * value;
}

template <typename T>
typename enable_if<is_floating_point<T>::value, int>::type round(T value) {
    return int(value + T(0.5));
}

}

#endif
