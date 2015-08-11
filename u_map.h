#ifndef U_MAP_HDR
#define U_MAP_HDR

#include "u_hash.h"
#include "u_pair.h"

namespace u {

template <typename K, typename V>
class map {
public:
    map();
    map(const map &other);
    map(map &&other);

    ~map();

    map &operator=(const map &other);
    map &operator=(map &&other);

    typedef pair<K, V> value_type;

    typedef hash_iterator<const hash_elem<K, V>> const_iterator;
    typedef hash_iterator<hash_elem<K, V>> iterator;

    iterator begin();
    iterator end();

    const_iterator begin() const;
    const_iterator end() const;

    void clear();
    bool empty() const;
    size_t size() const;

    const_iterator find(const K &key) const;
    iterator find(const K &key);

    pair<iterator, bool> insert(const pair<K, V> &p);
    pair<iterator, bool> insert(pair<K, V> &&p);

    void erase(const_iterator where);

    V &operator[](const K &key);
    V &operator[](K &&key);

    void swap(map &other);

private:
    const_iterator lookup(const K &key, size_t h) const;
    iterator lookup(const K &key, size_t h);

    detail::hash_base<hash_elem<K, V>> m_base;
};

template <typename K, typename V>
map<K, V>::map()
    : m_base(9)
{
}

template <typename K, typename V>
map<K, V>::map(const map<K, V> &other)
    : m_base(other.m_base)
{
}

template <typename K, typename V>
map<K, V>::map(map<K, V> &&other)
    : m_base(u::move(other.m_base))
{
}

template <typename K, typename V>
map<K, V>::~map() {
    detail::hash_free(m_base);
}

template <typename K, typename V>
map<K, V> &map<K, V>::operator=(const map<K, V> &other) {
    map<K, V>(other).swap(*this);
    return *this;
}

template <typename K, typename V>
map<K, V> &map<K, V>::operator=(map<K, V> &&other) {
    using base = detail::hash_base<hash_elem<K, V>>;
    assert(this != &other);
    detail::hash_free(m_base);
    m_base.~base();
    new (&m_base) base(u::move(other));
    return *this;
}


template <typename K, typename V>
inline typename map<K, V>::iterator map<K, V>::begin() {
    iterator it;
    it.node = *m_base.buckets.first;
    return it;
}

template <typename K, typename V>
inline typename map<K, V>::iterator map<K, V>::end() {
    iterator it;
    it.node = nullptr;
    return it;
}

template <typename K, typename V>
inline typename map<K, V>::const_iterator map<K, V>::begin() const {
    const_iterator cit;
    cit.node = *m_base.buckets.first;
    return cit;
}

template <typename K, typename V>
inline typename map<K, V>::const_iterator map<K, V>::end() const {
    const_iterator cit;
    cit.node = nullptr;
    return cit;
}

template <typename K, typename V>
inline bool map<K, V>::empty() const {
    return m_base.size == 0;
}

template <typename K, typename V>
inline size_t map<K, V>::size() const {
    return m_base.size;
}

template <typename K, typename V>
inline void map<K, V>::clear() {
    using base = detail::hash_base<hash_elem<K, V>>;
    m_base.~base();
    new (&m_base) base(9);
}

template <typename K, typename V>
inline typename map<K, V>::iterator map<K, V>::lookup(const K &key, size_t h) {
    iterator result;
    result.node = detail::hash_find(m_base, key, h);
    return result;
}

template <typename K, typename V>
inline typename map<K, V>::const_iterator map<K, V>::lookup(const K &key, size_t h) const {
    const_iterator result;
    result.node = detail::hash_find(m_base, key, h);
    return result;
}

template <typename K, typename V>
inline typename map<K, V>::iterator map<K, V>::find(const K &key) {
    return lookup(key, hash(key));
}

template <typename K, typename V>
inline typename map<K, V>::const_iterator map<K, V>::find(const K &key) const {
    return lookup(key, hash(key));
}

template <typename K, typename V>
inline pair<typename map<K, V>::iterator, bool> map<K, V>::insert(const pair<K, V> &p) {
    return insert(u::move(pair<K, V>(p)));
}

template <typename K, typename V>
inline pair<typename map<K, V>::iterator, bool> map<K, V>::insert(pair<K, V> &&p) {
    const size_t h = hash(p.first);
    auto result = make_pair(lookup(p.first, h), false);
    if (result.first.node != 0)
        return u::move(result);

    size_t nbuckets = (m_base.buckets.last - m_base.buckets.first);

    if ((m_base.size + 1) / nbuckets > 0.75f) {
        detail::hash_rehash(m_base, 8 * nbuckets);
        nbuckets = (m_base.buckets.last - m_base.buckets.first);
    }

    const size_t hh = h & (nbuckets - 2);
    typename map<K, V>::iterator &it = result.first;
    it.node = detail::hash_insert_new(m_base, hh);

    it.node->first.~hash_elem<K, V>();
    new (&it.node->first) hash_elem<K, V>(u::move(p.first), u::move(p.second));

    result.second = true;
    return u::move(result);
}

template <typename K, typename V>
inline void map<K, V>::erase(const_iterator where) {
    detail::hash_erase(m_base, where.node);
}

template <typename K, typename V>
inline V &map<K, V>::operator[](const K &key) {
    return insert(u::move(make_pair(key, u::move(V())))).first->second;
}

template <typename K, typename V>
inline V &map<K, V>::operator[](K &&key) {
    return insert(u::move(make_pair(u::forward<K>(key), u::move(V())))).first->second;
}

template <typename K, typename V>
inline void map<K, V>::swap(map &other) {
    detail::hash_swap(m_base, other.m_base);
}

}

#endif
