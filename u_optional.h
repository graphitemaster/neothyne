#ifndef U_OPTIONAL_HDR
#define U_OPTIONAL_HDR
namespace u {
    // A small implementation of boost.optional
    struct optional_none { };

    typedef int optional_none::*none_t;
    none_t const none = static_cast<none_t>(0);

    template <typename T>
    struct optional {
        optional(void) :
            m_init(false)
        {
        }

        // specialization for `none'
        optional(none_t) :
            m_init(false)
        {
        }

        optional(const T &value) :
            m_init(true)
        {
            construct(value);
        }

        optional(const optional<T> &opt) :
            m_init(opt.m_init)
        {
            if (m_init)
                construct(opt.get());
        }

        ~optional(void) {
            destruct();
        }

        optional &operator=(const optional<T> &opt) {
            destruct();
            if ((m_init = opt.m_init))
                construct(opt.get());
            return *this;
        }

        operator bool(void) const {
            return m_init;
        }

        T &operator *(void) {
            return get();
        }

        const T &operator*(void) const {
            return get();
        }

    private:
        void *storage(void) {
            return m_data;
        }

        const void *storage(void) const {
            return m_data;
        }

        T &get(void) {
            return *static_cast<T*>(storage());
        }

        const T &get(void) const {
            return *static_cast<const T*>(storage());
        }

        void destruct(void) {
            if (m_init)
                get().~T();
            m_init = false;
        }

        void construct(const T &data) {
            new (storage()) T(data);
        }

        bool m_init;
        alignas(alignof(T)) unsigned char m_data[sizeof(T)];
    };
}
#endif
