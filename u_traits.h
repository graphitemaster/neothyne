#ifndef U_TRAITS_HDR
#define U_TRAITS_HDR

namespace u {

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
    return static_cast<typename remove_reference<T>::type&&>(t);
}

}

#endif
