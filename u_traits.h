#ifndef U_TRAITS_HDR
#define U_TRAITS_HDR

#ifdef __has_feature
#   define HAS_FEATURE(X) __has_feature(X)
#else
#   define HAS_FEATURE(X) 0
#endif

namespace u {

namespace detail {
    /// To avoid including stddef.h we evaluate the type of sizeof
    /// which the standard says is size_t
    typedef decltype(sizeof(0)) size_type;
};

/// nullptr_t
typedef decltype(nullptr) nullptr_t;

/// conditional
template <bool B, typename T, typename F>
struct conditional {
    typedef T type;
};
template <typename T, typename F>
struct conditional<false, T, F> {
    typedef F type;
};

/// enable_if
template <bool, typename T = void>
struct enable_if {
};
template <typename T>
struct enable_if<true, T> {
    typedef T type;
};

/// integral_constant
template <typename T, T v>
struct integral_constant {
    static constexpr const T value = v;
};
template <typename T, T v>
constexpr const T integral_constant<T, v>::value;

/// true_type
typedef integral_constant<bool, true> true_type;

/// false_type
typedef integral_constant<bool, false> false_type;

/// remove_const
template <typename T>
struct remove_const {
    typedef T type;
};
template <typename T>
struct remove_const<const T> {
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

/// remove_cv
template <typename T>
struct remove_cv {
    typedef typename remove_volatile<typename remove_const<T>::type>::type type;
};

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

/// remove_all_extents
template <typename T>
struct remove_all_extents {
    typedef T type;
};
template <typename T>
struct remove_all_extents<T[]> {
    typedef typename remove_all_extents<T>::type type;
};
template <typename T, detail::size_type N>
struct remove_all_extents<T[N]> {
    typedef typename remove_all_extents<T>::type type;
};

/// add_lvalue_reference
template <typename T>
struct add_lvalue_reference {
    typedef T &type;
};
template <typename T>
struct add_lvalue_reference<T&> {
    typedef T &type;
};
template <>
struct add_lvalue_reference<void> {
    typedef void type;
};
template <>
struct add_lvalue_reference<const void> {
    typedef const void type;
};
template <>
struct add_lvalue_reference<volatile void> {
    typedef volatile void type;
};
template <>
struct add_lvalue_reference<const volatile void> {
    typedef const volatile void type;
};

/// is_integral
namespace detail {
    template <typename T>
    struct is_integral : false_type {};
    template <>
    struct is_integral<bool> : true_type {};
    template <>
    struct is_integral<char> : true_type {};
    template <>
    struct is_integral<signed char> : true_type {};
    template <>
    struct is_integral<unsigned char> : true_type {};
    template <>
    struct is_integral<short> : true_type {};
    template <>
    struct is_integral<unsigned short> : true_type {};
    template <>
    struct is_integral<int> : true_type {};
    template <>
    struct is_integral<unsigned int> : true_type {};
    template <>
    struct is_integral<long> : true_type {};
    template <>
    struct is_integral<unsigned long> : true_type {};
    template <>
    struct is_integral<long long> : true_type {};
    template <>
    struct is_integral<unsigned long long> : true_type {};
}
template <typename T>
struct is_integral : detail::is_integral<typename remove_cv<T>::type> {};

/// is_floating_point
namespace detail {
    template <typename T>
    struct is_floating_point : false_type {};
    template <>
    struct is_floating_point<float> : true_type {};
    template <>
    struct is_floating_point<double> : true_type {};
    template <>
    struct is_floating_point<long double> : true_type {};
}
template <typename T>
struct is_floating_point : detail::is_floating_point<typename remove_cv<T>::type> {};

/// is_array
template <typename T>
struct is_array : false_type {};
template <typename T>
struct is_array<T[]> : true_type {};
template <typename T, detail::size_type N>
struct is_array<T[N]> : true_type {};

/// is_pointer
namespace detail {
    template <typename T>
    struct is_pointer : false_type {};
    template <typename T>
    struct is_pointer<T*> : true_type {};
}
template <typename T>
struct is_pointer : detail::is_pointer<typename remove_cv<T>::type> {};

/// is_lvalue_reference
template <typename T>
struct is_lvalue_reference : false_type {};
template <typename T>
struct is_lvalue_reference<T&> : true_type {};

/// is_rvalue_reference
template <typename T>
struct is_rvalue_reference : false_type {};
template <typename T>
struct is_rvalue_reference<T&&> : true_type {};

/// is_reference
template <typename T>
struct is_reference : integral_constant<bool, is_lvalue_reference<T>::value ||
                                              is_rvalue_reference<T>::value>
{
};

/// is_const
template <typename T>
struct is_const : false_type {};
template <typename T>
struct is_const<const T> : true_type {};

/// is_volatile
template <typename T>
struct is_volatile : false_type {};
template <typename T>
struct is_volatile<volatile T> : true_type {};

/// is_void
namespace detail {
    template <typename T>
    struct is_void : false_type {};
    template <>
    struct is_void<void> : true_type {};
}
template <typename T>
struct is_void : detail::is_void<typename remove_cv<T>::type> {};

/// is_null_pointer
namespace detail {
    template <typename T>
    struct is_null_pointer : false_type {};
    template <>
    struct is_null_pointer<nullptr_t> : true_type {};
}
template <typename T>
struct is_null_pointer : detail::is_null_pointer<typename remove_cv<T>::type> {};

/// is_union
#if HAS_FEATURE(is_union)
template <typename T>
struct is_union : integral_constant<bool, __is_union(T)> { };
#else
namespace detail {
    template <typename T>
    struct is_union : false_type {};
}
template <typename T>
struct is_union : detail::is_union<typename remove_cv<T>::type> {};
#endif

/// is_class
namespace detail {
    template <typename T>
    char class_test(int T::*);
    template <typename T>
    int class_test(...);
}
template <typename T>
struct is_class : integral_constant<bool, sizeof(detail::class_test<T>(0)) == 1 && !is_union<T>::value> {};

/// is_same
template <typename T, typename U>
struct is_same : false_type {};
template <typename T>
struct is_same<T, T> : true_type {};

/// is_function
namespace detail {
    template <typename T>
    char function_test(T*);
    template <typename T>
    int function_test(...);
    template <typename T>
    T &function_source();
    template <typename T, bool = is_class<T>::value ||
                                 is_union<T>::value ||
                                 is_void<T>::value ||
                                 is_reference<T>::value ||
                                 is_null_pointer<T>::value>
    struct is_function : integral_constant<bool, sizeof(function_test<T>(function_source<T>())) == 1> {};
    template <typename T>
    struct is_function<T, true> : false_type {};
}
template <typename T>
struct is_function : detail::is_function<T> {};

/// is_member_pointer
namespace detail {
    template <typename T>
    struct is_member_pointer : false_type {};
    template <typename T, typename U>
    struct is_member_pointer<T U::*> : true_type {};
}
template <typename T>
struct is_member_pointer : detail::is_member_pointer<typename remove_cv<T>::type> {};

/// is_enum
#if HAS_FEATURE(is_enum)
template <typename T>
struct is_enum : integral_constant<bool, __is_union(T)> {};
#else
template <typename T>
struct is_enum : integral_constant<bool, !is_void<T>::value &&
                                         !is_integral<T>::value &&
                                         !is_floating_point<T>::value &&
                                         !is_array<T>::value &&
                                         !is_pointer<T>::value &&
                                         !is_reference<T>::value &&
                                         !is_member_pointer<T>::value &&
                                         !is_union<T>::value &&
                                         !is_class<T>::value &&
                                         !is_function<T>::value> {};
#endif

/// is_arithmetic
template <typename T>
struct is_arithmetic : integral_constant<bool, is_integral<T>::value ||
                                               is_floating_point<T>::value> {};

/// is_scalar
template <typename T>
struct is_scalar : integral_constant<bool, is_arithmetic<T>::value ||
                                           is_member_pointer<T>::value ||
                                           is_pointer<T>::value ||
                                           is_null_pointer<T>::value ||
                                           is_enum<T>::value> {};
/// is_trivially_constructible
#if HAS_FEATURE(is_trivially_constructible)
template <typename T, typename... A>
struct is_trivially_constructible : integral_constant<bool, __is_trivially_constructible(T, A...)> {};
#else
#   if HAS_FEATURE(has_trivial_constructor)
#   define IS_TRIVIAL_CONSTRUCTIBLE(T) __has_trivial_constructor(T)
#else
#   define IS_TRIVIAL_CONSTRUCTIBLE(T) is_scalar<T>::value
#endif
template <typename T, typename... A>
struct is_trivially_constructible : false_type {};
template <typename T>
struct is_trivially_constructible<T> : integral_constant<bool, IS_TRIVIAL_CONSTRUCTIBLE(T)> {};
template <typename T>
struct is_trivially_constructible<T, T&&> : integral_constant<bool, IS_TRIVIAL_CONSTRUCTIBLE(T)> {};
template <typename T>
struct is_trivially_constructible<T, const T&> : integral_constant<bool, IS_TRIVIAL_CONSTRUCTIBLE(T)> {};
template <typename T>
struct is_trivially_constructible<T, T&> : integral_constant<bool, IS_TRIVIAL_CONSTRUCTIBLE(T)> {};
#undef IS_TRIVIAL_CONSTRUCTIBLE
#endif

/// is_trivially_default_constructible
template <typename T>
struct is_trivially_default_constructible : is_trivially_constructible<T> {};

/// is_trivially_copy_constructible
template <typename T>
struct is_trivially_copy_constructible
    : is_trivially_constructible<T, typename add_lvalue_reference<const T>::type> {};

/// is_trivially_assignable
#if HAS_FEATURE(is_trivially_assignable)
template <typename T, typename A>
struct is_trivially_assignable : integral_constant<bool, __is_trivially_assignable(T, A)> {};
#else
template <typename T, typename A>
struct is_trivially_assignable : false_type {};
template <typename T>
struct is_trivially_assignable<T&, T> : integral_constant<bool, is_scalar<T>::value> {};
template <typename T>
struct is_trivially_assignable<T&, T&> : integral_constant<bool, is_scalar<T>::value> {};
template <typename T>
struct is_trivially_assignable<T&, const T&> : integral_constant<bool, is_scalar<T>::value> {};
template <typename T>
struct is_trivially_assignable<T&, T&&> : integral_constant<bool, is_scalar<T>::value> {};
#endif

/// add_const
namespace detail {
    template <typename T, bool = is_reference<T>::value ||
                                 is_function<T>::value ||
                                 is_const<T>::value>
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

/// is_trivially_copy_assignable
template <typename T>
struct is_trivially_copy_assignable
    : is_trivially_assignable<typename add_lvalue_reference<T>::type,
                              typename add_lvalue_reference<typename add_const<T>::type>::type> {};

/// is_trivially_destructible
namespace detail {
    template <typename T>
    struct is_trivially_destructible : integral_constant<bool, is_scalar<T>::value ||
                                                               is_reference<T>::value> {};
}
template <typename T>
struct is_trivially_destructible : detail::is_trivially_destructible<typename remove_all_extents<T>::type> {};

/// is_pod
template <typename T>
struct is_pod : integral_constant<bool, is_trivially_default_constructible<T>::value &&
                                        is_trivially_copy_constructible<T>::value &&
                                        is_trivially_copy_assignable<T>::value &&
                                        is_trivially_destructible<T>::value> {};

/// move
template <typename T>
inline constexpr typename remove_reference<T>::type &&move(T &&t) noexcept {
    typedef typename remove_reference<T>::type U;
    return static_cast<U&&>(t);
}

/// forward
template <typename T>
inline constexpr T &&forward(typename remove_reference<T>::type &t) noexcept {
    return static_cast<T&&>(t);
}
template <typename T>
inline constexpr T &&forward(typename remove_reference<T>::type &&t) noexcept {
    return static_cast<T&&>(t);
}

namespace detail {
    template <typename H, typename T>
    struct types {
        typedef H head;
        typedef T tail;
    };

    // nat: not a type
    struct nat {
        nat() = delete;
        nat(const nat&) = delete;
        nat &operator=(const nat&) = delete;
        ~nat() = delete;
    };

    typedef types<signed char,
            types<signed short,
            types<signed int,
            types<signed long,
            types<signed long long, nat>>>>> signed_types;

    typedef types<unsigned char,
            types<unsigned short,
            types<unsigned int,
            types<unsigned long,
            types<unsigned long long, nat>>>>> unsigned_types;

    // Given a type list recursively instantiate until we find the given type
    template <typename L, size_type S, bool = S <= sizeof(typename L::head)>
    struct find_first_type;
    template <typename H, typename T, size_type S>
    struct find_first_type<types<H, T>, S, true> {
        typedef H type;
    };
    template <typename H, typename T, size_type S>
    struct find_first_type<types<H, T>, S, false> {
        typedef typename find_first_type<T, S>::type type;
    };

    // For make_signed, make_unsigned we need to also apply the right cv
    template <typename T, typename U, bool = is_const<typename remove_reference<T>::type>::value,
                                      bool = is_volatile<typename remove_reference<T>::type>::value>
    struct apply_cv {
        typedef U type;
    };
    template <typename T, typename U>
    struct apply_cv<T, U, true, false> { // is_const
        typedef const U type;
    };
    template <typename T, typename U>
    struct apply_cv<T, U, false, true> { // is_volatile
        typedef volatile U type;
    };
    template <typename T, typename U>
    struct apply_cv<T, U, true, true> { // is_const && is_volatile
        typedef const volatile U type;
    };
    template <typename T, typename U>
    struct apply_cv<T&, U, true, false> { // T& is_const
        typedef const U& type;
    };
    template <typename T, typename U>
    struct apply_cv<T&, U, false, true> { // T& is_volatile
        typedef volatile U& type;
    };
    template <typename T, typename U>
    struct apply_cv<T&, U, true, true> { // T& is_const && T& is_volatile
        typedef const volatile U& type;
    };

    template <typename T, bool = is_integral<T>::value || is_enum<T>::value>
    struct make_signed {};
    template <typename T, bool = is_integral<T>::value || is_enum<T>::value>
    struct make_unsigned {};
    template <typename T>
    struct make_signed<T, true> {
        typedef typename find_first_type<signed_types, sizeof(T)>::type type;
    };
    template <typename T>
    struct make_unsigned<T, true> {
        typedef typename find_first_type<unsigned_types, sizeof(T)>::type type;
    };

    template <>
    struct make_signed<bool, true> {};
    template <>
    struct make_signed<signed short, true> {
        typedef short type;
    };
    template <>
    struct make_signed<unsigned short, true> {
        typedef short type;
    };
    template <>
    struct make_signed<signed int, true> {
        typedef int type;
    };
    template <>
    struct make_signed<unsigned int, true> {
        typedef int type;
    };
    template <>
    struct make_signed<signed long, true> {
        typedef long type;
    };
    template <>
    struct make_signed<unsigned long, true> {
        typedef long type;
    };
    template <>
    struct make_signed<signed long long, true> {
        typedef long long type;
    };
    template <>
    struct make_signed<unsigned long long, true> {
        typedef long long type;
    };

    template <>
    struct make_unsigned<bool, true> {};
    template <>
    struct make_unsigned<signed short, true> {
        typedef unsigned short type;
    };
    template <>
    struct make_unsigned<unsigned short, true> {
        typedef unsigned short type;
    };
    template <>
    struct make_unsigned<signed int, true> {
        typedef unsigned int type;
    };
    template <>
    struct make_unsigned<unsigned int, true> {
        typedef unsigned int type;
    };
    template <>
    struct make_unsigned<signed long,true> {
        typedef unsigned long type;
    };
    template <>
    struct make_unsigned<unsigned long,true> {
        typedef unsigned long type;
    };
    template <>
    struct make_unsigned<signed long long, true> {
        typedef unsigned long long type;
    };
    template <>
    struct make_unsigned<unsigned long long, true> {
        typedef unsigned long long type;
    };
}

/// make_signed
template <typename T>
struct make_signed {
    typedef typename detail::apply_cv<T, typename detail::make_signed<
        typename remove_cv<T>::type>::type>::type type;
};

/// make_unsigned
template <typename T>
struct make_unsigned {
    typedef typename detail::apply_cv<T, typename detail::make_unsigned<
        typename remove_cv<T>::type>::type>::type type;
};

namespace detail {
    template <typename T, bool = is_integral<T>::value>
    struct is_signed_switch : integral_constant<bool, T(-1) < T(0)> {};
    template <typename T>
    struct is_signed_switch<T, false> : true_type {}; // floating point

    template <typename T, bool = is_arithmetic<T>::value>
    struct is_signed : is_signed_switch<T> {};
    template <typename T>
    struct is_signed<T, false> : false_type {};
}

/// is_signed
template <typename T>
struct is_signed : detail::is_signed<T> {};

}

#undef HAS_FEATURE
#endif
