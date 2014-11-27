#include <stdlib.h>
#include "u_string.h"
#include "u_hash.h"

namespace u {

string::string()
    : m_first(nullptr)
    , m_last(nullptr)
    , m_capacity(nullptr)
{
}

string::string(const string& other)
    : m_first(nullptr)
    , m_last(nullptr)
    , m_capacity(nullptr)
{
    reserve(other.size());
    append(other.m_first, other.m_last);
}

string::string(const char* sz)
    : m_first(nullptr)
    , m_last(nullptr)
    , m_capacity(nullptr)
{
    const size_t len = strlen(sz);
    reserve(len);
    append(sz, sz + len);
}

string::string(const char *sz, size_t len)
    : m_first(nullptr)
    , m_last(nullptr)
    , m_capacity(nullptr)
{
    reserve(len);
    append(sz, sz + len);
}

string::~string() {
    free(m_first);
}

string& string::operator=(const string& other) {
    string(other).swap(*this);
    return *this;
}

const char* string::c_str() const {
    return m_first;
}

size_t string::size() const {
    return m_last - m_first;
}

bool string::empty() const {
    return m_last - m_first == 0;
}

char *string::copy() const {
    const size_t length = size() + 1;
    return (char *)memcpy(malloc(length), m_first, length);
}

void string::reserve(size_t capacity) {
    if (m_first + capacity + 1 <= m_capacity)
        return;

    const size_t size = m_last - m_first;

    char *newfirst = (char *)malloc(capacity + 1);
    for (char *it = m_first, *newit = newfirst, *end = m_last; it != end; ++it, ++newit)
        *newit = *it;
    free(m_first);

    m_first = newfirst;
    m_last = newfirst + size;
    m_capacity = m_first + capacity;
}

void string::resize(size_t size) {
    reserve(size);
    for (char *it = m_last, *end = m_first + size + 1; it < end; ++it)
        *it = '\0';
    m_last += size;
}

string &string::append(const char *first, const char *last) {
    const size_t newsize = m_last - m_first + last - first + 1;
    if (m_first + newsize > m_capacity)
        reserve((newsize * 3) / 2);
    for (; first != last; ++m_last, ++first)
        *m_last = *first;
    *m_last = '\0';
    return *this;
}

string &string::append(const char *str, size_t len) {
    return append(str, str + len);
}

char string::pop_back() {
    if (m_last == m_first)
        return *m_first;
    m_last--;
    char last = *m_last;
    *m_last = '\0';
    return last;
}

char string::pop_front() {
    char front = *m_first;
    erase(0, 1);
    return front;
}

string::iterator string::begin() {
    return m_first;
}

string::iterator string::end() {
    return m_last;
}

string::const_iterator string::begin() const {
    return m_first;
}

string::const_iterator string::end() const {
    return m_last;
}

char &string::operator[](size_t index) {
    return m_first[index];
}

const char &string::operator[](size_t index) const {
    return m_first[index];
}

size_t string::find(char ch) const {
    char *search = strchr(m_first, ch);
    return search ? search - m_first : npos;
}

void string::erase(size_t beg, size_t end) {
    char *mbeg = m_first + beg;
    char *mend = m_first + end;
    const size_t len = m_last - mend;
    memmove(mbeg, mend, len);
    m_last = &mbeg[len];
    m_last[0] = '\0';
}

void string::swap(string& other) {
    char *tfirst = m_first;
    char *tlast = m_last;
    char *tcapacity = m_capacity;
    m_first = other.m_first;
    m_last = other.m_last;
    m_capacity = other.m_capacity;
    other.m_first = tfirst;
    other.m_last = tlast;
    other.m_capacity = tcapacity;
}

void string::reset() {
    m_last = m_first;
    *m_first = '\0';
    m_capacity = m_first;
}

size_t hash(const string &str) {
    return detail::sdbm(str.c_str(), str.size());
}

}
