#ifndef C_VAR_HDR
#define C_VAR_HDR

namespace c {

enum varType {
    VAR_INT,
    VAR_FLOAT
};

template <typename T>
struct varTypeTraits;
template <>
struct varTypeTraits<int> { static constexpr varType type = VAR_INT; };
template <>
struct varTypeTraits<float> { static constexpr varType type = VAR_FLOAT; };

void varRegister(const char *name, const char *desc, void *what, varType type);

template <typename T>
struct var {
    var(const char *name, const char *desc, const T &min, const T &max,
        const T &def) :
        m_min(min),
        m_max(max),
        m_default(def),
        m_current(def),
        m_callback(nullptr)
    {
        void *self = reinterpret_cast<void *>(this);
        varRegister(name, desc, self, varTypeTraits<T>::type);
    }

    var(const char *name, const char *desc, const T &min, const T &max,
        const T &def, void (*callback)(T value)) :
        var(name, desc, min, max, def)
    {
        m_callback = callback;
    }

    operator T&(void) {
        return m_current;
    }

    const T get(void) const {
        return m_current;
    }

    void set(const T &value) {
        if (value < m_min) return;
        if (value > m_max) return;
        m_current = value;
    }

    void operator()(T value) {
        if (m_callback)
            m_callback(value);
    }

private:
    const T m_min;
    const T m_max;
    const T m_default;
    T m_current;
    void (*m_callback)(T value);
};

bool writeConfig(void);
bool readConfig(void);

}
#endif
