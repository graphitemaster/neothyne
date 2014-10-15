#ifndef U_HASH_HDR
#define U_HASH_HDR
#include "u_misc.h"

namespace u {

// Hash node represents a key value pair that can be used to implement associative
// containers like map, or set
template <typename K, typename V>
struct hash_node {
    hash_node(const K &key, const V &value);

    const K first;
    V second;
    hash_node *next;
    hash_node *prev;
};

template <typename K, typename V>
hash_node<K, V>::hash_node(const K &key, const V &value)
    : first(key)
    , second(value)
{
}

template <typename K>
struct hash_node<K, void> {
    hash_node(const K& key);

    const K first;
    hash_node* next;
    hash_node* prev;
};

template <typename K>
hash_node<K, void>::hash_node(const K& key)
    : first(key)
{
}

template <typename K, typename V>
static void hash_node_insert(hash_node<K, V>* node, size_t hash,
                             hash_node<K, V>** buckets, size_t nbuckets)
{
    size_t bucket = hash & (nbuckets - 1);
    hash_node<K, V>* it = buckets[bucket + 1];
    node->next = it;
    if (it) {
        node->prev = it->prev;
        it->prev = node;

        if (node->prev)
            node->prev->next = node;
    } else {
        size_t newbucket = bucket;
        while (newbucket && !buckets[newbucket])
            --newbucket;

        hash_node<K, V>* prev = buckets[newbucket];
        while (prev && prev->next)
            prev = prev->next;

        node->prev = prev;
        if (prev)
            prev->next = node;
    }
    for (; it == buckets[bucket]; --bucket) {
        buckets[bucket] = node;
        if (!bucket)
            break;
    }
}

template <typename K, typename V>
static inline void hash_node_erase(const hash_node<K, V>* where, size_t hash,
                                   hash_node<K, V>** buckets, size_t nbuckets)
{
    size_t bucket = hash & (nbuckets - 1);

    hash_node<K, V>* next = where->next;
    for (; buckets[bucket] == where; --bucket) {
        buckets[bucket] = next;
        if (!bucket)
            break;
    }

    if (where->prev)
        where->prev->next = where->next;
    if (next)
        next->prev = where->prev;
}

template <typename N>
struct hash_iterator {
    N *operator->() const;
    N &operator*() const;
    N *node;
};

template <typename N>
struct hash_iterator<const N> {

    hash_iterator() {}
    hash_iterator(hash_iterator<N> other)
        : node(other.node)
    {
    }

    const N *operator->() const;
    const N &operator*() const;
    const N *node;
};

template <typename K>
struct hash_iterator<const hash_node<K, void> > {
    const K *operator->() const;
    const K &operator*() const;
    hash_node<K, void>* node;
};

template <typename LN, typename RN>
static inline bool operator==(const hash_iterator<LN>& lhs, const hash_iterator<RN>& rhs) {
    return lhs.node == rhs.node;
}

template <typename LN, typename RN>
static inline bool operator!=(const hash_iterator<LN>& lhs, const hash_iterator<RN>& rhs) {
    return lhs.node != rhs.node;
}

template <typename N>
static inline void operator++(hash_iterator<N>& lhs) {
    lhs.node = lhs.node->next;
}

template <typename N>
inline N *hash_iterator<N>::operator->() const {
    return node;
}

template <typename N>
inline N &hash_iterator<N>::operator*() const {
    return *node;
}

template <typename N>
inline const N *hash_iterator<const N>::operator->() const {
    return node;
}

template <typename N>
inline const N &hash_iterator<const N>::operator*() const {
    return *node;
}

template <typename K>
inline const K *hash_iterator<const hash_node<K, void> >::operator->() const {
    return &node->first;
}

template <typename K>
inline const K &hash_iterator<const hash_node<K, void> >::operator*() const {
    return node->first;
}

template <typename N, typename K>
static inline N hash_find(const K &key, N *buckets, size_t nbuckets) {
    const size_t bucket = hash(key) & (nbuckets - 2);
    for (N it = buckets[bucket], end = buckets[bucket+1]; it != end; it = it->next)
        if (it->first == key)
            return it;
    return 0;
}

}

#endif
