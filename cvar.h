#ifndef CVAR_HDR
#define CVAR_HDR
#include "u_string.h"
#include "u_optional.h"

enum varType {
    kVarInt,
    kVarFloat,
    kVarString
};

enum {
    kVarPersist = 1 << 0,
    kVarReadOnly = 1 << 1
};

enum varStatus {
    kVarSuccess = 1,
    kVarRangeError,
    kVarTypeError,
    kVarNotFoundError,
    kVarReadOnlyError
};

template <typename T>
struct varTypeTraits;

template <>
struct varTypeTraits<int> {
    static constexpr varType type = kVarInt;
};

template <>
struct varTypeTraits<float> {
    static constexpr varType type = kVarFloat;
};

template <>
struct varTypeTraits<u::string> {
    static constexpr varType type = kVarString;
};

void varRegister(const char *name, const char *desc, void *what, varType type);

varStatus varChange(const u::string &name, const u::string &value, bool callback = false);

template <typename T>
inline void varDefine(const char *name, const char *desc, T *self) {
    varRegister(name, desc, (void *)self, varTypeTraits<typename T::type>::type);
}

template <typename T>
struct var {
    typedef T type;
    typedef void (*command)(T&);

    var(int flags, const char *name, const char *desc, const T &min, const T &max);
    var(int flags, const char *name, const char *desc, const T &min, const T &max, const T &def);
    var(int flags, const char *name, const char *desc, const T &min, const T &max, const T &def, command cb);

    operator T&();
    T &get();
    const T min() const;
    const T max() const;
    varStatus set(const T &value);
    void setMin(const T &min);
    void setMax(const T &max);
    void operator()();
    int flags() const;
    void toggle();

private:
    T m_min;
    T m_max;
    T m_default;
    T m_current;
    command m_callback;
    int m_flags;
};

template <>
struct var<u::string> {
    typedef u::string type;
    typedef void (*command)(u::string &value);

    var(int flags, const char *name, const char *desc);
    var(int flags, const char *name, const char *desc, const u::string &def);
    var(int flags, const char *name, const char *desc, const u::string &def, command cb);

    operator u::string&();
    u::string &get();
    varStatus set(const u::string &value);
    void operator()();
    int flags() const;

private:
    const u::string m_default;
    u::string m_current;
    command m_callback;
    int m_flags;
};

/// var<T>
template <typename T>
inline var<T>::var(int flags,
                   const char *name,
                   const char *desc,
                   const T &min,
                   const T &max)
    : m_min(min)
    , m_max(max)
    , m_callback(nullptr)
    , m_flags(flags)
{
    varDefine(name, desc, this);
}

template <typename T>
inline var<T>::var(int flags,
                   const char *name,
                   const char *desc,
                   const T &min,
                   const T &max,
                   const T &def)
    : m_min(min)
    , m_max(max)
    , m_default(def)
    , m_current(def)
    , m_callback(nullptr)
    , m_flags(flags)
{
    varDefine(name, desc, this);
}

template <typename T>
inline var<T>::var(int flags,
                   const char *name,
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
    , m_flags(flags)
{
    varDefine(name, desc, this);
}

template <typename T>
inline var<T>::operator T&() {
    return m_current;
}

template <typename T>
T &var<T>::get() {
    return m_current;
}

template <typename T>
inline const T var<T>::min() const {
    return m_min;
}

template <typename T>
inline const T var<T>::max() const {
    return m_max;
}

template <typename T>
inline varStatus var<T>::set(const T &value) {
    if (m_flags & kVarReadOnly)
        return kVarReadOnlyError;
    if (value < m_min)
        return kVarRangeError;
    if (value > m_max)
        return kVarRangeError;
    m_current = value;
    return kVarSuccess;
}

template <typename T>
inline void var<T>::setMin(const T &min) {
    m_min = min;
    if (m_current < m_min)
        m_current = m_min;
}

template <typename T>
inline void var<T>::setMax(const T &max) {
    m_max = max;
    if (m_current > m_max)
        m_current = m_max;
}

template <typename T>
inline void var<T>::operator()() {
    if (m_callback)
        m_callback(m_current);
}

template <typename T>
inline int var<T>::flags() const {
    return m_flags;
}

template <typename T>
inline void var<T>::toggle() {
    m_current = !m_current;
}

/// var<u::string>
inline var<u::string>::var(int flags,
                           const char *name,
                           const char *desc,
                           const u::string &def)
    : m_default(def)
    , m_current(def)
    , m_callback(nullptr)
    , m_flags(flags)
{
    varDefine(name, desc, this);
}

inline var<u::string>::var(int flags,
                           const char *name,
                           const char *desc)
    : m_callback(nullptr)
    , m_flags(flags)
{
    varDefine(name, desc, this);
}

inline var<u::string>::var(int flags,
                           const char *name,
                           const char *desc,
                           const u::string &def,
                           typename var<u::string>::command cb)
    : m_default(def)
    , m_current(def)
    , m_callback(cb)
    , m_flags(flags)
{
    varDefine(name, desc, this);
}

inline var<u::string>::operator u::string&() {
    return m_current;
}

inline u::string &var<u::string>::get() {
    return m_current;
}

inline varStatus var<u::string>::set(const u::string &value) {
    if (m_flags & kVarReadOnly)
        return kVarReadOnlyError;
    m_current = value;
    return kVarSuccess;
}

inline void var<u::string>::operator()() {
    if (m_callback)
        m_callback(m_current);
}

inline int var<u::string>::flags() const {
    return m_flags;
}

template <typename T>
var<T> &varGet(const char *name);

u::optional<u::string> varValue(const u::string &name);

#define VAR(TYPE, NAME, ...) \
    static var<TYPE> NAME(kVarPersist, #NAME, __VA_ARGS__)

#define NVAR(TYPE, NAME, ...) \
    static var<TYPE> NAME(0, #NAME, __VA_ARGS__);

bool writeConfig(const u::string &userPath);
bool readConfig(const u::string &userPath);

#endif
