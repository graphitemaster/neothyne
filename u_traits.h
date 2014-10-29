#ifndef U_TRAITS_HDR
#define U_TRAITS_HDR

namespace u {

namespace detail {
    struct true_type {
        static constexpr bool value = true;
    };
    struct false_type {
        static constexpr bool value = false;
    };

    template <typename T>
    struct is_integral : false_type { };

    template <> struct is_integral<char>               : true_type {};
    template <> struct is_integral<signed char>        : true_type {};
    template <> struct is_integral<unsigned char>      : true_type {};
    template <> struct is_integral<short>              : true_type {};
    template <> struct is_integral<unsigned short>     : true_type {};
    template <> struct is_integral<int>                : true_type {};
    template <> struct is_integral<unsigned int>       : true_type {};
    template <> struct is_integral<long>               : true_type {};
    template <> struct is_integral<unsigned long>      : true_type {};
    template <> struct is_integral<long long>          : true_type {};
    template <> struct is_integral<unsigned long long> : true_type {};

    template <typename T>
    struct is_floating_point : false_type {};

    template <> struct is_floating_point<float>       : true_type {};
    template <> struct is_floating_point<double>      : true_type {};
    template <> struct is_floating_point<long double> : true_type {};

    template <typename T>
    struct is_pointer : false_type {};
    template <typename T>
    struct is_pointer<T*> : true_type {};

    template <typename T>
    struct is_reference : false_type {};
    template <typename T>
    struct is_reference<T&> : true_type {};
    template <typename T>
    struct is_reference<T&&> : true_type {};

    template <typename T>
    struct is_const : false_type {};
    template <typename T>
    struct is_const<T const> : true_type {};
}

/// enable_if
template <bool B, typename T = void>
struct enable_if {};

template <typename T>
struct enable_if<true, T> {
    typedef T type;
};

/// conditional
template <bool B, typename A, typename B>
struct conditional;

template <typename A, typename B>
struct conditional<true, A, B> {
    typedef A type;
};

template <typename A, typename B>
struct conditional<false, A, B> {
    typedef B type;
};

/// remove_volatile
template <typename T>
struct remove_volatile {
    typedef T type;
};

template <typename T>
struct remove_volatile<volatile T> {
    typedef T type;
};

/// remove_const
template <typename T>
struct remove_const {
    typedef T type;
};

template <typename T>
struct remove_const<const T> {
    typedef T type;
};

/// remove_cv
template <typename T>
struct remove_cv {
    typedef typename remove_volatile<
        typename remove_const<T>::type>::type type;
};

/// is_integral
template <typename T>
struct is_integral
    : detail::is_integral<typename remove_cv<T>::type> {};

/// is_floating_point
template <typename T>
struct is_floating_point
    : detail::is_floating_point<typename remove_cv<T>::type> {};

/// is_pointer
template <typename T>
struct is_pointer
    : detail::is_pointer<typename remove_cv<T>::type> {};

/// is_reference
template <typename T>
struct is_reference
    : detail::is_reference<T> {};

/// remove_reference
template <typename T>
struct remove_reference {
    typedef T type;
};

template <typename T>
struct remove_reference<T&> {
    typedef T type;
};

template <typename T>
struct remove_reference<T&&> {
    typedef T type;
};

/// add_const
namespace detail {
    // the following add_const will add const to a function type. This is not the
    // same as the standard. This cannot be checked without compiler support
    template <typename T, bool = is_reference<T>::value || is_const<T>::value>
    struct add_const {
        typedef T type;
    };

    template <typename T>
    struct add_const<T, false> {
        typedef const T type;
    };
}

template <typename T>
struct add_const {
    typedef typename detail::add_const<T>::type type;
};

/// move
template <typename T>
constexpr typename remove_reference<T>::type &&move(T &&t) {
    // static_cast is required here
    return static_cast<typename remove_reference<T>::type&&>(t);
}

/// forward
template <typename T>
constexpr T &&forward(typename remove_reference<T>::type &ref) {
    // static_cast is required here
    return static_cast<T&&>(ref);
}

template <typename T>
constexpr T &&forward(typename remove_reference<T>::type &&ref) {
    // static_cast is required here
    return static_cast<T&&>(ref);
}

}

#endif
