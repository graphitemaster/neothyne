#ifndef U_HASH_HDR
#define U_HASH_HDR
#include <stddef.h>
#include <string.h>
#include <assert.h>

#include "u_new.h"
#include "u_buffer.h"

namespace u {

// Simple hash function via SDBM
namespace detail {
    static inline size_t sdbm(const void *data, size_t length) {
        size_t hash = 0;
        unsigned char *as = (unsigned char *)data;
        for (unsigned char *it = as, *end = as + length; it != end; ++it)
            hash = *it + (hash << 6) + (hash << 16) - hash;
        return hash;
    }
}

template <typename T>
inline size_t hash(const T &value) {
    const size_t rep = size_t(value);
    return detail::sdbm((const void *)&rep, sizeof(rep));
}

template <typename K, typename V>
struct hash_elem {
    hash_elem();
    hash_elem(const K &key, const V &value);

    const K first;
    V second;

private:
    // Prevent compiler synthesized assignment operator for non-pod types from
    // generating warnings.
    hash_elem &operator=(const hash_elem &);
};

template<typename K, typename V>
hash_elem<K, V>::hash_elem()
    : first(), second()
{
}

template <typename K, typename V>
hash_elem<K, V>::hash_elem(const K &key, const V &value)
    : first(key)
    , second(value)
{
}

template <typename K>
struct hash_elem<K, void> {
    hash_elem();
    hash_elem(const K& key);

    const K first;
};

template<typename K>
hash_elem<K, void>::hash_elem()
    : first()
{
}

template <typename K>
hash_elem<K, void>::hash_elem(const K& key)
    : first(key)
{
}

namespace detail {
    template<typename N>
    struct hash_node {
        hash_node<N> *next;
        hash_node<N> *prev;
        N first;
    };

    template<typename N>
    struct hash_chunk {
        hash_node<N> nodes[64];
        hash_chunk<N> *next;
    };

    template<typename N>
    struct hash_base {
        hash_base(size_t n = 0);
        hash_base(const hash_base &);
        hash_base(hash_base &&);

        hash_base &operator=(hash_base &&);

        buffer<hash_node<N> *> buckets;

        size_t size;

        hash_chunk<N> *chunks;
        hash_node<N> *unused;
    };

    template<typename N>
    hash_base<N>::hash_base(size_t n)
        : buckets()
        , size(0)
        , chunks(nullptr)
        , unused(nullptr)
    {
        buckets.resize(n, nullptr);
    }

    template<typename N>
    inline hash_node<N> *hash_insert_new(hash_base<N> &h, size_t hh);

    template<typename N>
    hash_base<N>::hash_base(const hash_base &other)
        : buckets()
        , size(other.size)
        , chunks(nullptr)
        , unused(nullptr)
    {
        size_t nbuckets = other.buckets.last - other.buckets.first;
        buckets.resize(nbuckets, nullptr);

        hash_node<N> **och = other.buckets.first;
        for (hash_node<N> *oc = *och; oc; oc = oc->next) {
            size_t hh = hash(oc->first.first) & (nbuckets - 2);
            hash_node<N> *nc = hash_insert_new(*this, hh);
            nc->first.~N();
            new (&nc->first) N(oc->first);
        }
    }

    template<typename N>
    hash_base<N>::hash_base(hash_base &&other)
        : buckets(u::move(other.buckets))
        , size(other.size)
        , chunks(other.chunks)
        , unused(other.unused)
    {
    }

    template<typename N>
    inline void hash_init(hash_base<N> &h, size_t n) {
        h.size = 0;
        h.chunks = nullptr;
        h.unused = nullptr;
        h.buckets.resize(n, nullptr);
    }

    template<typename N>
    inline hash_node<N> *hash_insert(hash_node<N> **buckets, hash_node<N> *c, size_t hh) {
        hash_node<N> *it = buckets[hh + 1];
        c->next = it;
        if (it) {
            c->prev = it->prev;
            it->prev = c;
            if (c->prev) c->prev->next = c;
        } else {
            size_t nb = hh;
            while (nb && !buckets[nb])
                --nb;
            hash_node<N> *prev = buckets[nb];
            while (prev && prev->next)
                prev = prev->next;

            c->prev = prev;
            if (prev)
                prev->next = c;
        }
        for (; it == buckets[hh]; --hh) {
            buckets[hh] = c;
            if (!hh) break;
        }
        return c;
    }

    template<typename N>
    inline hash_node<N> *hash_insert_new(hash_base<N> &h, size_t hh) {
        if (!h.unused) {
            hash_chunk<N> *chunk = neoMalloc(sizeof(hash_chunk<N>));
            new (chunk) hash_chunk<N>();
            chunk->next = h.chunks;
            h.chunks = chunk;
            for (size_t i = 0; i < (64 - 1); ++i) {
                chunk->nodes[i].next = &chunk->nodes[i + 1];
                chunk->nodes[i].prev = nullptr;
            }
            chunk->nodes[64 - 1].next = nullptr;
            chunk->nodes[64 - 1].prev = nullptr;
            h.unused = chunk->nodes;
        }
        ++h.size;
        hash_node<N> *c = h.unused;
        h.unused = h.unused->next;
        return hash_insert(h.buckets.first, c, hh);
    }

    template<typename N>
    inline void hash_swap(hash_base<N> &a, hash_base<N> &b) {
        a.buckets.swap(b.buckets);
        size_t size = a.size;
        hash_chunk<N> *chunks = a.chunks;
        hash_node<N> *unused = a.unused;
        a.size = b.size;
        a.chunks = b.chunks;
        a.unused = b.unused;
        b.size = size;
        b.chunks = chunks;
        b.unused = unused;
    }

    template<typename N>
    inline void hash_delete_chunks(hash_base<N> &h) {
        hash_chunk<N> *chunks = h.chunks;
        h.chunks = nullptr;
        for (hash_chunk<N> *nc; chunks; chunks = nc) {
            nc = chunks->next;
            chunks->~hash_chunk<N>();
            neoFree(chunks);
        }
    }

    template<typename N>
    inline void hash_free(hash_base<N> &h) {
        hash_delete_chunks(h);
    }

    template<typename N, typename K>
    inline hash_node<N> *hash_find(const hash_base<N> &h, const K &key) {
        size_t hh = hash(key) & (h.buckets.last - h.buckets.first - 2);
        for (hash_node<N> *c = h.buckets.first[hh], *end = h.buckets.first[hh + 1]; c != end; c = c->next) {
            if (c->first.first == key)
                return c;
        }
        return nullptr;
    }

    template<typename N>
    inline void hash_rehash(hash_base<N> &h, size_t n) {
        buffer<hash_node<N> *> &och = h.buckets;
        buffer<hash_node<N> *>  nch;
        nch.resize(n, nullptr);

        hash_node<N> *p = *och.first;
        while (p) {
            hash_node<N> *pp = p->next;
            size_t hh = hash(p->first.first) & (n - 2);
            p->prev = p->next = nullptr;
            hash_insert(nch.first, p, hh);
            p = pp;
        }

        h.buckets = u::move(nch);
    }

    template<typename N>
    inline void hash_erase(hash_base<N> &h, hash_node<N> *node) {
        size_t nbuckets = h.buckets.last - h.buckets.first;
        size_t hh = hash(node->first.first) & (nbuckets - 2);

        hash_node<N> *next = node->next;
        for (; h.buckets.first[hh] == node; --hh) {
            h.buckets.first[hh] = next;
            if (!hh) break;
        }

        if (node->prev) node->prev->next = next;
        if (next) next->prev = node->prev;

        node->next = h.unused;
        node->prev = nullptr;
        h.unused = node;
    }
}

template <typename N>
struct hash_iterator {
    N *operator->() const;
    N &operator*() const;
    detail::hash_node<N> *node;
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

    detail::hash_node<N> *node;
};

template <typename K>
struct hash_iterator<const hash_elem<K, void>> {
    const K *operator->() const;
    const K &operator*() const;

    detail::hash_node<hash_elem<K, void>> *node;
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
static inline void operator++(hash_iterator<N> &lhs) {
    lhs.node = lhs.node->next;
}

template <typename N>
inline N *hash_iterator<N>::operator->() const {
    return &node->first;
}

template <typename N>
inline N &hash_iterator<N>::operator*() const {
    return node->first;
}

template <typename N>
inline const N *hash_iterator<const N>::operator->() const {
    return &node->first;
}

template <typename N>
inline const N &hash_iterator<const N>::operator*() const {
    return node->first;
}

template <typename K>
inline const K *hash_iterator<const hash_elem<K, void>>::operator->() const {
    return &node->first.first;
}

template <typename K>
inline const K &hash_iterator<const hash_elem<K, void>>::operator*() const {
    return node->first.first;
}

}

#endif
