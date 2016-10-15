#ifndef C_VARIABLE_HDR
#define C_VARIABLE_HDR

#include "u_string.h"

namespace c {

enum {
    kVarInt,
    kVarFloat,
    kVarString
};

enum {
    kPersist = 1 << 0,
    kReadOnly = 1 << 1
};

template <typename T>
struct Trait;

template <>
struct Trait<int> {
    enum { value = kVarInt };
};

template <>
struct Trait<float> {
    enum { value = kVarFloat };
};

template <>
struct Trait<u::string> {
    enum { value = kVarString };
};

struct Reference {
    Reference() = default;
    Reference(const char *name, const char *description, void *handle, int type);

private:
    friend struct Console;
    friend struct Config;

    const char *m_name;
    const char *m_description;
    void *m_handle;
    int m_type;
    Reference *m_next;
};

template <typename T>
struct Variable {
    Variable(int flags, const char *name, const char *desc, const T &min, const T &max);
    Variable(int flags, const char *name, const char *desc, const T &min, const T &max, const T &def);

    operator T&();
    T &get();
    const T &get() const;
    const T min() const;
    const T max() const;
    int set(const T &value);
    void setMin(const T &min);
    void setMax(const T &max);
    int flags() const;
    void toggle();

private:
    friend struct Config;
    Reference m_reference;
    T m_min;
    T m_max;
    T m_default;
    T m_current;
    int m_flags;
};

template <>
struct Variable<u::string> {
    Variable(int flags, const char *name, const char *desc);
    Variable(int flags, const char *name, const char *desc, const char *def);

    operator u::string&();
    u::string &get();
    const u::string &get() const;
    int set(const u::string &value);
    int flags() const;

private:
    friend struct Console;
    friend struct Config;
    Reference m_reference;
    const char *m_default;
    alignas(u::string) unsigned char m_currentData[sizeof(u::string)];
    u::string *m_current;
    int m_flags;
};

}

#define VAR(TYPE, NAME, ...) \
    static c::Variable<TYPE> NAME(c::kPersist, #NAME, __VA_ARGS__)

#define NVAR(TYPE, NAME, ...) \
    static c::Variable<TYPE> NAME(0, #NAME, __VA_ARGS__)

#endif
