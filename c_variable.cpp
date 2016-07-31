#include "c_console.h"

namespace c {

Reference::Reference(const char *name, const char *description, void *handle, int type)
    : m_name(name)
    , m_description(description)
    , m_handle(handle)
    , m_type(type)
    , m_next(Console::m_references)
{
    Console::m_references = this;
}

template <typename T>
Variable<T>::Variable(int flags,
                      const char *name,
                      const char *description,
                      const T &min,
                      const T &max)
    : m_reference({ name, description, (void *)this, Trait<T>::value })
    , m_min(min)
    , m_max(max)
    , m_flags(flags)
{
}

template <typename T>
Variable<T>::Variable(int flags,
                      const char *name,
                      const char *description,
                      const T &min,
                      const T &max,
                      const T &def)
    : m_reference({ name, description, (void *)this, Trait<T>::value })
    , m_min(min)
    , m_max(max)
    , m_default(def)
    , m_current(def)
    , m_flags(flags)
{
}

template <typename T>
Variable<T>::operator T&() {
    return m_current;
}

template <typename T>
T &Variable<T>::get() {
    return m_current;
}

template <typename T>
const T &Variable<T>::get() const {
    return m_current;
}

template <typename T>
const T Variable<T>::min() const {
    return m_min;
}

template <typename T>
const T Variable<T>::max() const {
    return m_max;
}

template <typename T>
int Variable<T>::set(const T &value) {
    if (m_flags & kReadOnly)
        return Console::kVarReadOnlyError;
    if (value < m_min)
        return Console::kVarRangeError;
    if (value > m_max)
        return Console::kVarRangeError;
    m_current = value;
    return Console::kVarSuccess;
}

template <typename T>
void Variable<T>::setMin(const T &min) {
    m_min = min;
    if (m_current < m_min) m_current = m_min;
}

template <typename T>
void Variable<T>::setMax(const T &max) {
    m_max = max;
    if (m_current > m_max) m_current = m_max;
}

template <typename T>
int Variable<T>::flags() const {
    return m_flags;
}

template <typename T>
void Variable<T>::toggle() {
    m_current = !m_current;
}

Variable<u::string>::Variable(int flags,
                              const char *name,
                              const char *description,
                              const char *def)
    : m_reference({ name, description, (void *)this, kVarString })
    , m_default(def)
    , m_flags(flags)
{
}

Variable<u::string>::Variable(int flags,
                              const char *name,
                              const char *description)
    : m_reference({ name, description, (void *)this, kVarString })
    , m_flags(flags)
{
}

u::string *Variable<u::string>::current() const {
    union {
        u::string *s;
        void *p;
    };
    *(void **)&p = (void *)m_current;
    return s;
}

Variable<u::string>::operator u::string&() {
    return *current();
}

u::string &Variable<u::string>::get() {
    return *current();
}

const u::string &Variable<u::string>::get() const {
    return *current();
}

int Variable<u::string>::set(const u::string &value) {
    if (m_flags & kReadOnly)
        return Console::kVarReadOnlyError;
    *current() = value;
    return Console::kVarSuccess;
}

int Variable<u::string>::flags() const {
    return m_flags;
}

template struct Variable<int>;
template struct Variable<float>;
template struct Variable<u::string>;

}
