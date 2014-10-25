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
}

/// enable_if
template <bool B, typename T = void>
struct enable_if {};

template <typename T>
struct enable_if<true, T> {
    typedef T type;
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

template <typename T>
constexpr typename remove_reference<T>::type &&move(T &&t) {
    // static_cast is required here
    return static_cast<typename remove_reference<T>::type&&>(t);
}

}

#endif
