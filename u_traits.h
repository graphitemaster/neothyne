#ifndef U_TRAITS_HDR
#define U_TRAITS_HDR
#include <stddef.h>

#if defined(__has_feature)
#   define HAS_FEATURE(X) __has_feature(X)
#else
#   define HAS_FEATURE(X) 0
#endif

namespace u {

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

/// remove_extent
template <typename T>
struct remove_extent {
    typedef T type;
};
template <typename T>
struct remove_extent<T[]> {
    typedef T type;
};
template <typename T, size_t E>
struct remove_extent<T[E]> {
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
template <typename T, size_t N>
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
template <typename T, size_t N>
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

/// is_empty
#if HAS_FEATURE(is_empty)
template <typename T>
struct is_empty : integral_constant<bool, __is_empty(T)> {};
#else
namespace detail {
    template <typename T>
    struct is_empty1 : T {
        double l;
    };

    struct is_empty2 {
        double l;
    };

    template <typename T, bool = is_class<T>::value>
    struct is_empty : integral_constant<bool, sizeof(is_empty1<T>) == sizeof(is_empty2)> {};

    template <typename T>
    struct is_empty<T, false> : false_type {};
}
template <typename T>
struct is_empty : detail::is_empty<T> {};
#endif

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
struct is_enum : integral_constant<bool, __is_enum(T)> {};
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

/// add_pointer
template <typename T>
struct add_pointer {
    typedef typename remove_reference<T>::type *type;
};

/// decay
template <typename T>
struct decay {
private:
    typedef typename remove_reference<T>::type U;
public:
    typedef typename conditional<is_array<U>::value,
                                 typename remove_extent<U>::type*,
                                 typename conditional<is_function<U>::value,
                                                      typename add_pointer<U>::type,
                                                      typename remove_cv<U>::type>::type>::type type;
};

/// add_rvalue_reference
template <typename T>
struct add_rvalue_reference {
    typedef T &&type;
};
template <>
struct add_rvalue_reference<void> {
    typedef void type;
};
template <>
struct add_rvalue_reference<const void> {
    typedef const void type;
};
template <>
struct add_rvalue_reference<volatile void> {
    typedef volatile void type;
};
template <>
struct add_rvalue_reference<const volatile void> {
    typedef const volatile void type;
};

/// declval
template <typename T>
typename add_rvalue_reference<T>::type declval();

/// offset_of
namespace detail {
    template <typename T1, typename T2>
    struct offset_of {
        static T2 object;
        static constexpr size_t offset(T1 T2::*member) {
            return size_t(&(detail::offset_of<T1, T2>::object.*member)) -
                   size_t(&detail::offset_of<T1, T2>::object);
        }
    };
    template <typename T1, typename T2>
    T2 offset_of<T1, T2>::object;
}

template <typename T1, typename T2>
inline constexpr void *offset_of(T1 T2::*member) {
    return (void *)detail::offset_of<T1, T2>::offset(member);
}

/// Convenience likely/unlikely "traits".
template <typename T>
inline bool likely(const T &value) {
#if defined(__GNUC__)
    return __builtin_expect(!!value, 1);
#else
    return !!value == 1;
#endif
}

template <typename T>
inline bool unlikely(const T &value) {
#if defined(__GNUC__)
    return __builtin_expect(!!value, 0);
#else
    return !!value == 0;
#endif
}

}

#undef HAS_FEATURE

// Keep this outside of namespace u
constexpr size_t operator"" _z(unsigned long long n) {
    return n;
}

#endif
