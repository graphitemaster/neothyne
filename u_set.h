#ifndef U_SET_HDR
#define U_SET_HDR

#include "u_hash.h"
#include "u_buffer.h"
#include "u_pair.h"

namespace u {

template <typename K>
struct set {
    set();
    set(const set &other);
    set(set &&other);

    ~set();

    set &operator=(const set &other);
    set &operator=(set &&other);

    typedef hash_iterator<const hash_elem<K, void>> const_iterator;
    typedef const_iterator iterator;

    iterator begin() const;
    iterator end() const;

    void clear();
    bool empty() const;
    size_t size() const;

    const_iterator find(const K &key) const;
    iterator find(const K &key);

    pair<iterator, bool> insert(const K &key);
    pair<iterator, bool> insert(K &&key);
    void erase(iterator where);
    size_t erase(const K &key);

    void swap(set &other);

private:
    const_iterator lookup(const K &key, size_t h) const;
    iterator lookup(const K &key, size_t h);

    detail::hash_base<hash_elem<K, void>> m_base;
};

template <typename K>
set<K>::set()
    : m_base(9)
{
}

template <typename K>
set<K>::set(const set<K> &other)
    : m_base(other.m_base)
{
}

template <typename K>
set<K>::set(set<K> &&other)
    : m_base(u::move(other.m_base))
{
}

template <typename K>
set<K>::~set() {
    detail::hash_free(m_base);
}

template <typename K>
set<K> &set<K>::operator=(const set<K> &other) {
    set<K>(other).swap(*this);
    return *this;
}

template <typename K>
set<K> &set<K>::operator=(set<K> &&other) {
    using base = detail::hash_base<hash_elem<K, void>>;
    if (this == &other) assert(0);
    detail::hash_free(m_base);
    m_base.~base();
    new (&m_base) base(u::move(other));
    return *this;
}

template <typename K>
inline typename set<K>::iterator set<K>::begin() const {
    iterator it;
    it.node = *m_base.buckets.first;
    return it;
}

template <typename K>
inline typename set<K>::iterator set<K>::end() const {
    iterator it;
    it.node = nullptr;
    return it;
}

template <typename K>
inline bool set<K>::empty() const {
    return m_base.size == 0;
}

template <typename K>
inline size_t set<K>::size() const {
    return m_base.size;
}

template <typename K>
inline void set<K>::clear() {
    using base = detail::hash_base<hash_elem<K, void>>;
    m_base.~base();
    new (&m_base) base(9);
}

template <typename K>
typename set<K>::const_iterator set<K>::lookup(const K &key, size_t h) const {
    iterator result;
    result.node = detail::hash_find(m_base, key, h);
    return result;
}

template <typename K>
typename set<K>::iterator set<K>::lookup(const K &key, size_t h) {
    iterator result;
    result.node = detail::hash_find(m_base, key, h);
    return result;
}

template <typename K>
typename set<K>::const_iterator set<K>::find(const K &key) const {
    return lookup(key, hash(key));
}

template <typename K>
typename set<K>::iterator set<K>::find(const K &key) {
    return lookup(key, hash(key));
}

template <typename K>
inline pair<typename set<K>::iterator, bool> set<K>::insert(const K &key) {
    return insert(u::move(K(key)));
}

template <typename K>
inline pair<typename set<K>::iterator, bool> set<K>::insert(K &&key) {
    const size_t h = hash(key);
    auto result = make_pair(lookup(key, h), false);
    if (result.first.node != 0)
        return u::move(result);

    size_t nbuckets = (m_base.buckets.last - m_base.buckets.first);

    if ((m_base.size + 1) / nbuckets > 0.75f) {
        detail::hash_rehash(m_base, 8 * nbuckets);
        nbuckets = (m_base.buckets.last - m_base.buckets.first);
    }

    const size_t hh = h & (nbuckets - 2);
    typename set<K>::iterator &it = result.first;
    it.node = detail::hash_insert_new(m_base, hh);

    it.node->first.~hash_elem<K, void>();
    new ((void *)&it.node->first) hash_elem<K, void>(u::move(key));

    result.second = true;
    return u::move(result);
}

template <typename K>
inline void set<K>::erase(iterator where) {
    detail::hash_erase(m_base, where.node);
}

template <typename K>
inline size_t set<K>::erase(const K &key) {
    const_iterator it = find(key);
    if (!it.node) return 0;
    erase(it);
    return 1;
}

template <typename K>
void set<K>::swap(set &other) {
    detail::hash_swap(m_base, other.m_base);
}

}
#endif
