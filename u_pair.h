#ifndef U_PAIR_HDR
#define U_PAIR_HDR

namespace u {

template <typename K, typename V>
struct pair {
    pair();
    pair(const K &key, const V &value);
    K first;
    V second;
};

template <typename K, typename V>
pair<K, V>::pair()
{
}

template <typename K, typename V>
pair<K, V>::pair(const K& key, const V& value)
    : first(key)
    , second(value)
{
}

template <typename K, typename V>
static inline pair<K, V> make_pair(const K &key, const V &value) {
    return pair<K, V>(key, value);
}

}

#endif
