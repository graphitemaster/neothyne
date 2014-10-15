#ifndef U_OPTIONAL_HDR
#define U_OPTIONAL_HDR

namespace u {

// A small implementation of boost.optional
struct optional_none { };

typedef int optional_none::*none_t;
none_t const none = static_cast<none_t>(0);

template <typename T>
struct optional {
    optional();
    optional(none_t);
    optional(const T &value);
    optional(const optional<T> &opt);

    ~optional();

    optional &operator=(const optional<T> &opt);

    operator bool() const;
    T &operator *();
    const T &operator*() const;

private:
    void *storage();
    const void *storage() const;
    T &get();
    const T &get() const;
    void destruct();
    void construct(const T &data);

    bool m_init;
    alignas(alignof(T)) unsigned char m_data[sizeof(T)];
};

template <typename T>
optional<T>::optional()
    : m_init(false)
{
}

template <typename T>
optional<T>::optional(none_t)
    : m_init(false)
{
}

template <typename T>
optional<T>::optional(const T &value)
    : m_init(true)
{
    construct(value);
}

template <typename T>
optional<T>::optional(const optional<T> &opt)
    : m_init(opt.m_init)
{
    if (m_init)
        construct(opt.get());
}

template <typename T>
optional<T>::~optional() {
    destruct();
}

template <typename T>
optional<T> &optional<T>::operator=(const optional<T> &opt) {
    destruct();
    if ((m_init = opt.m_init))
        construct(opt.get());
    return *this;
}

template <typename T>
optional<T>::operator bool() const {
    return m_init;
}

template <typename T>
T &optional<T>::operator *() {
    return get();
}

template <typename T>
const T &optional<T>::operator*() const {
    return get();
}

template <typename T>
void *optional<T>::storage() {
    return m_data;
}

template <typename T>
const void *optional<T>::storage() const {
    return m_data;
}

template <typename T>
T &optional<T>::get() {
    return *static_cast<T*>(storage());
}

template <typename T>
const T &optional<T>::get() const {
    return *static_cast<const T*>(storage());
}

template <typename T>
void optional<T>::destruct() {
    if (m_init)
        get().~T();
    m_init = false;
}

template <typename T>
void optional<T>::construct(const T &data) {
    new (storage()) T(data);
}

}

#endif
