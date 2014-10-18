#ifndef C_VAR_HDR
#define C_VAR_HDR
#include "u_string.h"

namespace c {

enum varType {
    VAR_INT,
    VAR_FLOAT,
    VAR_STRING
};

template <typename T>
struct varTypeTraits;

template <>
struct varTypeTraits<int> {
    static constexpr varType type = VAR_INT;
};

template <>
struct varTypeTraits<float> {
    static constexpr varType type = VAR_FLOAT;
};

template <>
struct varTypeTraits<u::string> {
    static constexpr varType type = VAR_STRING;
};

void varRegister(const char *name, const char *desc, void *what, varType type);

template <typename T>
struct var {
    typedef void (*command)(T&);

    var(const char *name, const char *desc, const T &min, const T &max, const T &def);
    var(const char *name, const char *desc, const T &min, const T &max, const T &def, command cb);

    operator T&();
    const T get() const;
    void set(const T &value);
    void operator()();

private:
    const T m_min;
    const T m_max;
    const T m_default;
    T m_current;
    command m_callback;
};

template <>
struct var<u::string> {
    typedef void (*command)(u::string &value);

    var(const char *name, const char *desc, const u::string &def);
    var(const char *name, const char *desc, const u::string &def, command cb);

    operator u::string&();
    const u::string get() const;
    void set(const u::string &value);
    void operator()();

private:
    const u::string m_default;
    u::string m_current;
    command m_callback;
};

/// var<T>
template <typename T>
inline var<T>::var(const char *name,
                   const char *desc,
                   const T &min,
                   const T &max,
                   const T &def)
    : m_min(min)
    , m_max(max)
    , m_default(def)
    , m_current(def)
    , m_callback(nullptr)
{
    void *self = reinterpret_cast<void *>(this);
    varRegister(name, desc, self, varTypeTraits<T>::type);
}

template <typename T>
inline var<T>::var(const char *name,
                   const char *desc,
                   const T &min,
                   const T &max,
                   const T &def,
                   typename var<T>::command cb)
    : m_min(min)
    , m_max(max)
    , m_default(def)
    , m_current(def)
    , m_callback(cb)
{
    void *self = reinterpret_cast<void *>(this);
    varRegister(name, desc, self, varTypeTraits<T>::type);
}

template <typename T>
inline var<T>::operator T&() {
    return m_current;
}

template <typename T>
inline const T var<T>::get() const {
    return m_current;
}

template <typename T>
inline void var<T>::set(const T &value) {
    if (value < m_min) return;
    if (value > m_max) return;
    m_current = value;
}

template <typename T>
inline void var<T>::operator()() {
    if (m_callback)
        m_callback(m_current);
}

/// var<u::string>
inline var<u::string>::var(const char *name,
                           const char *desc,
                           const u::string &def)
    : m_default(def)
    , m_current(def)
    , m_callback(nullptr)
{
    void *self = reinterpret_cast<void *>(this);
    varRegister(name, desc, self, varTypeTraits<u::string>::type);
}

inline var<u::string>::var(const char *name,
                           const char *desc,
                           const u::string &def,
                           typename var<u::string>::command cb)
    : m_default(def)
    , m_current(def)
    , m_callback(cb)
{
    void *self = reinterpret_cast<void *>(this);
    varRegister(name, desc, self, varTypeTraits<u::string>::type);
}

inline var<u::string>::operator u::string&() {
    return m_current;
}

inline const u::string var<u::string>::get() const {
    return m_current;
}

inline void var<u::string>::set(const u::string &value) {
    m_current = value;
}

inline void var<u::string>::operator()() {
    if (m_callback)
        m_callback(m_current);
}

bool writeConfig();
bool readConfig();

}
#endif
