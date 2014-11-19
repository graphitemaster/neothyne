#ifndef U_STRING_HDR
#define U_STRING_HDR
#include <string.h>

namespace u {

struct string {
    typedef char *iterator;
    typedef const char *const_iterator;

    static constexpr size_t npos = size_t(-1);

    // constructors
    string();
    string(const string& other);
    string(const char* sz);
    string(const char* sz, size_t len);
    template <typename I>
    string(I first, I last);
    ~string();

    // iterators
    iterator begin();
    iterator end();
    const_iterator begin() const;
    const_iterator end() const;

    string& operator=(const string& other);

    const char* c_str() const;
    size_t size() const;
    bool empty() const;

    char *copy() const;

    void reserve(size_t size);
    void resize(size_t size);

    string &append(const char *first, const char* last);
    string &append(const char *str, size_t len);

    char pop_back();
    char pop_front();

    char &operator[](size_t index);
    const char &operator[](size_t index) const;
    size_t find(char ch) const;

    void erase(size_t beg, size_t end);

    void swap(string& other);

private:
    char *m_first;
    char *m_last;
    char *m_capacity;
};

template <typename I>
inline string::string(I first, I last)
    : m_first(nullptr)
    , m_last(nullptr)
    , m_capacity(nullptr)
{
    const size_t len = last - first;
    reserve(len);
    append((const char *)first, len);
}

inline bool operator==(const string& lhs, const string& rhs) {
    if (lhs.size() != rhs.size())
        return false;
    return !strcmp(lhs.c_str(), rhs.c_str());
}

inline string operator+(const string &lhs, const char *rhs) {
    return string(lhs).append(rhs, strlen(rhs));
}

inline string operator+(const char *lhs, const string &rhs) {
    return string(lhs).append(rhs.c_str(), rhs.size());
}

inline string operator+(const string &lhs, const string &rhs) {
    return string(lhs).append(rhs.c_str(), rhs.size());
}

inline void operator+=(string &lhs, const char *rhs) {
    lhs.append(rhs, strlen(rhs));
}

inline void operator+=(string &lhs, const string &rhs) {
    operator+=(lhs, rhs.c_str());
}

inline void operator+=(string &lhs, char rhs) {
    lhs.append(&rhs, 1);
}

size_t hash(const string &str);

}

#endif
