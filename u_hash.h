#ifndef U_HASH_HDR
#define U_HASH_HDR
#include <stddef.h>
#include <string.h>

#include "u_new.h"
#include "u_buffer.h"

// A cache friendly chunk-allocated hash table for creating key, key-value
// and other associative containers.
//
// Optimizations:
//  - Chunked allocation strategy to avoid memory fragmentation and improve
//    cache locality (linked list nodes in a chunk are contiguous in memory.)
//
//  - Hash nodes appropriately sized for L1 cache size to reduce false-sharing
//    and cache-line bouncing when used in a multi-threaded context. Currently
//    not the case but improves performance for instruction-level-parallelism
//    as multiple pipelines don't touch the same cache-line.
//
//  - Empty base class optimization reduces node sizes when only a key is
//    chosen, thus an empty value type is not included. This is useful as
//    it means map<K, void> nodes only consume the space of the key and
//    set<K> can be implemented in terms of map<K, void>.
//
//  - Small iterator model (it's just a pointer). Compiles down to efficient
//    pointer arithmetic within chunk-boundaries (with cache-friendly
//    characteristics due to nodes being contiguous in memory.) Outside
//    chunk-boundaries involve a pointer reload into the next chunk, each
//    chunk-boundary becomes immediately hot during iteration (each chunk
//    is padded to cache-line alignment.) Results are blazing fast iteration
//    comparable to that of iterating a vector. This makes it reasonable
//    for storing entities with a name rather than an index.
//
//  - Memory management handled entirely by u::buffer, a type-aware
//    "buffer of objects" allocator that optimizes operations based on
//    type. Takes advantage of raw memory operations like `memcpy' when
//    type satisfies constraints.
//
// Note: specializes search functionality on `const char *' for key to
//       utilize string comparison and different hashing mechanism;
//       this decision largely has to do with that being the common case
//       of using `const char *' for key. If you intended to compare pointer
//       values (and hash their representation) instead, consider using a
//       different pointer type or a proxy type.

namespace u {

namespace detail {
    template <size_t E>
    struct fnvConst;

    template <>
    struct fnvConst<8> {
        // 64-bit
        static constexpr size_t kPrime = size_t(1099511628211ull);
        static constexpr size_t kOffsetBasis = size_t(14695981039346656037ull);
    };

    template <>
    struct fnvConst<4> {
        // 32-bit
        static constexpr size_t kPrime = 16777619u;
        static constexpr size_t kOffsetBasis = 2166136261u;
    };

    static inline size_t fnv1a(const void *data, size_t length) {
        using constants = fnvConst<sizeof(size_t)>;
        static constexpr size_t kPrime = constants::kPrime;
        static constexpr size_t kOffsetBasis = constants::kOffsetBasis;
        size_t hash = kOffsetBasis;
        unsigned char *as = (unsigned char *)data;
        for (unsigned char *it = as, *end = as + length; it != end; ++it) {
            hash ^= *it;
            hash *= kPrime;
        }
        return hash;
    }

    template <typename T>
    struct hasher {
        static size_t hash(const T &value) {
            const size_t rep = size_t(value);
            return detail::fnv1a((const void *)&rep, sizeof rep);
        }
    };
    // Specialization for string literals
    template <>
    struct hasher<const char *> {
        static size_t hash(const char *value) {
            const size_t length = strlen(value);
            return detail::fnv1a((const void *)value, length);
        }
    };
}

template <typename T>
inline size_t hash(const T &value) {
    return detail::hasher<T>::hash(value);
}

inline size_t hash(const unsigned char *data, size_t size) {
    return detail::fnv1a((const void *)data, size);
}

template <typename K, typename V>
struct hash_elem {
    hash_elem();
    hash_elem(const K &key, const V &value);
    hash_elem(K &&key, V &&value);

    const K first;
    V second;

private:
    // Prevent compiler synthesized assignment operator for non-pod types from
    // generating warnings.
    hash_elem &operator=(const hash_elem &);
};

template<typename K, typename V>
hash_elem<K, V>::hash_elem()
    : first()
    , second()
{
}

template <typename K, typename V>
hash_elem<K, V>::hash_elem(const K &key, const V &value)
    : first(key)
    , second(value)
{
}

template <typename K, typename V>
hash_elem<K, V>::hash_elem(K &&key, V &&value)
    : first(u::forward<K>(key))
    , second(u::forward<V>(value))
{
}

template <typename K>
struct hash_elem<K, void> {
    hash_elem();
    hash_elem(const K& key);
    hash_elem(K &&key);

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

template <typename K>
hash_elem<K, void>::hash_elem(K &&key)
    : first(u::forward<K>(key))
{
}

namespace detail {
    template <typename N>
    struct hash_node {
        hash_node<N> *next;
        hash_node<N> *prev;
        N first;
    };

    // The following compile time logic ensures that a `hash_chunk' is always a
    // multiple of `kCacheLineSize'. It first trys to adjust the chunk count
    // between `kLowerBound' and `kUpperBound' items to see if that will make
    // a `hash_chunk' a multiple of `kCacheLineSize'. If it fails to do this,
    // `kUpperBound' is chosen and additional padding is added to a `hash_chunk'
    // to make it a multiple of `kCacheLineSize'.
    static constexpr size_t kCacheLineSize = 64;
    static constexpr size_t kLowerBound = 16;
    static constexpr size_t kUpperBound = 64;

    template <typename N, size_t E>
    struct hash_node_align {
        // Recursively retry until we're a multiple of `kCacheLineSize'
        static constexpr size_t kValue =
            ((sizeof(hash_node<N>[E]) + sizeof(void *)) % kCacheLineSize == 0)
                ? E : hash_node_align<N, E+1>::kValue;
    };

    template <typename N>
    struct hash_node_align<N, kUpperBound> {
        // Upper bound reached, do not keep retrying
        static constexpr size_t kValue = kUpperBound;
    };

    template <size_t E, bool A>
    struct hash_chunk_padding;

    template <size_t E>
    struct hash_chunk_padding<E, true> {
        // E = size, A = aligned. Here we have no padding since A == true, that
        // is to say that E is already a multiple of `kCacheLineSize'
    };

    template <size_t E>
    struct hash_chunk_padding<E, false> {
        // E = size, A = aligned, Here we need padding since A == false, that
        // is to say that E is not a multiple of `kCacheLineSize' so we'll construct
        // an array of bytes that will be inherited for `hash_chunk' so that it
        // becomes a multiple of `kCacheLineSize'
        unsigned char padding[kCacheLineSize - (E % kCacheLineSize)];
    };

    template <size_t E>
    struct hash_padding
        : hash_chunk_padding<E, E % kCacheLineSize == 0> {
    };

    template <typename N, size_t V = hash_node_align<N, kLowerBound>::kValue, bool P = V == kUpperBound>
    struct hash_chunk;

    template <typename N, size_t V>
    struct hash_chunk<N, V, false> {
        // When P == false, it implies that the upper bound on calculating `kNodes'
        // was not reached, which implies that the count will make the size of
        // this `hash_chunk' a multiple of `kCacheLineSize'
        static constexpr size_t kNodes = V;
        hash_node<N> nodes[kNodes];
        hash_chunk<N> *next;
    };

    template <typename N, size_t V>
    struct hash_chunk<N, V, true>
        : hash_padding<sizeof(hash_node<N>[V]) + sizeof(hash_chunk<N>*)>
    {
        // When P == true, it can mean one of two things, 1. We reached alignment
        // exactly at `kUpperBound' or 2. We did not reach alignment at `kUpperBound'.
        // In this case we inherit from `hash_padding' with our current size and
        // that appropriately checks if it's a multiple of `kCacheLineSize'. If it
        // is, then it does not add padding, otherwise it adds padding until this
        // `hash_chunk' is a multiple of `kCacheLineSize'.
        static constexpr size_t kNodes = V;
        hash_node<N> nodes[kNodes];
        hash_chunk<N> *next;
    };

    template <typename N>
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

    template <typename N>
    hash_base<N>::hash_base(size_t n)
        : buckets()
        , size(0)
        , chunks(nullptr)
        , unused(nullptr)
    {
        buckets.resize(n, nullptr);
    }

    template <typename N>
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
            const size_t hh = hash(oc->first.first) & (nbuckets - 2);
            hash_node<N> *nc = hash_insert_new(*this, hh);
            nc->first.~N();
            new (&nc->first) N(oc->first);
        }
    }

    template <typename N>
    hash_base<N>::hash_base(hash_base &&other)
        : buckets(u::move(other.buckets))
        , size(other.size)
        , chunks(other.chunks)
        , unused(other.unused)
    {
    }

    template <typename N>
    inline void hash_init(hash_base<N> &h, size_t n) {
        h.size = 0;
        h.chunks = nullptr;
        h.unused = nullptr;
        h.buckets.resize(n);
    }

    template <typename N>
    inline hash_node<N> *hash_insert(hash_node<N> **buckets, hash_node<N> *c, size_t hh) {
        hash_node<N> *it = buckets[hh + 1];
        c->next = it;
        if (it) {
            c->prev = it->prev;
            auto &prev = c->prev;
            if (prev)
                prev->next = c;
            it->prev = c;
        } else {
            size_t nb = hh;
            while (nb && !buckets[nb])
                --nb;
            hash_node<N> *prev = buckets[nb];
            if (prev) {
                while (prev->next)
                    prev = prev->next;
                c->prev = prev;
                prev->next = c;
            } else {
                c->prev = nullptr;
            }
        }
        for (; it == buckets[hh]; --hh) {
            buckets[hh] = c;
            if (hh)
                continue;
            break;
        }
        return c;
    }

    template <typename N>
    inline hash_node<N> *hash_insert_new(hash_base<N> &h, size_t hh) {
        if (!h.unused) {
            hash_chunk<N> *chunk = neoMalloc(sizeof *chunk);
            new (chunk) hash_chunk<N>();
            chunk->next = h.chunks;
            h.chunks = chunk;
            for (size_t i = 0; i < hash_chunk<N>::kNodes - 1; ++i) {
                chunk->nodes[i].next = &chunk->nodes[i + 1];
                chunk->nodes[i].prev = nullptr;
            }
            chunk->nodes[hash_chunk<N>::kNodes - 1].next = nullptr;
            chunk->nodes[hash_chunk<N>::kNodes - 1].prev = nullptr;
            h.unused = chunk->nodes;
        }
        ++h.size;
        hash_node<N> *c = h.unused;
        h.unused = h.unused->next;
        return hash_insert(h.buckets.first, c, hh);
    }

    template <typename N>
    inline void hash_swap(hash_base<N> &a, hash_base<N> &b) {
        a.buckets.swap(b.buckets);
        u::swap(a.size, b.size);
        u::swap(a.chunks, b.chunks);
        u::swap(a.unused, b.unused);
    }

    template <typename N>
    inline void hash_delete_chunks(hash_base<N> &h) {
        hash_chunk<N> *chunks = h.chunks;
        h.chunks = nullptr;
        for (hash_chunk<N> *nc; chunks; chunks = nc) {
            nc = chunks->next;
            chunks->~hash_chunk<N>();
            neoFree(chunks);
        }
    }

    template <typename N>
    inline void hash_free(hash_base<N> &h) {
        hash_delete_chunks(h);
    }

    template <typename N, typename K>
    struct hash_find_specialized {
        static hash_node<N> *find(const hash_base<N> &h, const K &key, size_t value) {
            const size_t hh = value & (h.buckets.last - h.buckets.first - 2);
            for (hash_node<N> *c = h.buckets.first[hh], *end = h.buckets.first[hh + 1]; c != end; c = c->next)
                if (c->first.first == key)
                    return c;
            return nullptr;
        }
    };

    template <typename N>
    struct hash_find_specialized<N, const char *> {
        static hash_node<N> *find(const hash_base<N> &h, const char *key, size_t value) {
            const size_t hh = value & (h.buckets.last - h.buckets.first - 2);
            for (hash_node<N> *c = h.buckets.first[hh], *end = h.buckets.first[hh + 1]; c != end; c = c->next)
                if (!strcmp(c->first.first, key))
                    return c;
            return nullptr;
        }
    };

    template <typename N, typename K>
    inline hash_node<N> *hash_find(const hash_base<N> &h, const K &key, size_t value) {
        return hash_find_specialized<N, K>::find(h, key, value);
    }

    template <typename N>
    inline void hash_rehash(hash_base<N> &h, size_t n) {
        buffer<hash_node<N> *> &och = h.buckets;
        buffer<hash_node<N> *> nch;
        nch.resize(n);

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

    template <typename N>
    inline void hash_erase(hash_base<N> &h, hash_node<N> *node) {
        const size_t nbuckets = h.buckets.last - h.buckets.first;
        size_t hh = hash(node->first.first) & (nbuckets - 2);

        hash_node<N> *next = node->next;
        for (; h.buckets.first[hh] == node; --hh) {
            h.buckets.first[hh] = next;
            if (hh)
                continue;
            break;
        }

        if (node->prev)
            node->prev->next = next;
        if (next)
            next->prev = node->prev;

        node->next = h.unused;
        node->prev = nullptr;
        h.unused = node;

        node->first.~N();
        new (&node->first) N();
    }
}

template <typename N>
struct hash_iterator {
    hash_iterator();
    hash_iterator(detail::hash_node<N> *node);
    N *operator->() const;
    N &operator*() const;
    detail::hash_node<N> *node;
};

template <typename N>
struct hash_iterator<const N> {
    hash_iterator();
    hash_iterator(detail::hash_node<N> *node);
    hash_iterator(hash_iterator<N> other);
    const N *operator->() const;
    const N &operator*() const;

    detail::hash_node<N> *node;
};


template <typename N>
hash_iterator<N>::hash_iterator()
    : node(nullptr)
{
}


template <typename N>
hash_iterator<N>::hash_iterator(detail::hash_node<N> *node)
    : node(node)
{
}

template <typename N>
hash_iterator<const N>::hash_iterator()
    : node(nullptr)
{
}

template <typename N>
hash_iterator<const N>::hash_iterator(hash_iterator<N> other)
    : node(other.node)
{
}

template <typename N>
hash_iterator<const N>::hash_iterator(detail::hash_node<N> *node)
    : node(node)
{
}

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

// 128-bit version djbx33a
struct djbx33a {
    djbx33a(const unsigned char *data, size_t size);
    const char *hex() const;
private:
    char m_result[33];
};

inline const char *djbx33a::hex() const {
    return m_result;
}

}

#endif
