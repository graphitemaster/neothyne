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
struct varTypeTraits<int> { static constexpr varType type = VAR_INT; };
template <>
struct varTypeTraits<float> { static constexpr varType type = VAR_FLOAT; };
template <>
struct varTypeTraits<u::string> { static constexpr varType type = VAR_STRING; };

void varRegister(const char *name, const char *desc, void *what, varType type);

template <typename T>
struct var {
    typedef void (*command)(T&);

    var(const char *name, const char *desc, const T &min, const T &max, const T &def)
        : m_min(min)
        , m_max(max)
        , m_default(def)
        , m_current(def)
        , m_callback(nullptr)
    {
        void *self = reinterpret_cast<void *>(this);
        varRegister(name, desc, self, varTypeTraits<T>::type);
    }

    var(const char *name, const char *desc, const T &min, const T &max, const T &def, command cb)
        : m_min(min)
        , m_max(max)
        , m_default(def)
        , m_current(def)
        , m_callback(cb)
    {
        void *self = reinterpret_cast<void *>(this);
        varRegister(name, desc, self, varTypeTraits<T>::type);
    }

    operator T&() {
        return m_current;
    }

    const T get() const {
        return m_current;
    }

    void set(const T &value) {
        if (value < m_min) return;
        if (value > m_max) return;
        m_current = value;
    }

    void operator()() {
        if (m_callback)
            m_callback(m_current);
    }

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

    var(const char *name, const char *desc, const u::string &def)
        : m_default(def)
        , m_current(def)
        , m_callback(nullptr)
    {
        void *self = reinterpret_cast<void *>(this);
        varRegister(name, desc, self, varTypeTraits<u::string>::type);
    }

    var(const char *name, const char *desc, const u::string &def, command cb)
        : m_default(def)
        , m_current(def)
        , m_callback(cb)
    {
        void *self = reinterpret_cast<void *>(this);
        varRegister(name, desc, self, varTypeTraits<u::string>::type);
    }

    operator u::string&() {
        return m_current;
    }

    const u::string get() const {
        return m_current;
    }

    void set(const u::string &value) {
        m_current = value;
    }

    void operator()() {
        if (m_callback)
            m_callback(m_current);
    }

private:
    const u::string m_default;
    u::string m_current;
    command m_callback;
};

bool writeConfig();
bool readConfig();

}
#endif
