#ifndef UTIL_HDR
#define UTIL_HDR
#include <map>       // std::map
#include <set>       // std::set
#include <list>      // std::list
#include <vector>    // std::vector
#include <memory>    // std::unique_ptr
#include <algorithm> // std::sort
#include <string>    // std::string

#include <stdio.h>   // fopen
#include <stdlib.h>  // abs
#include <stdarg.h>  // va_start, va_end, va_list
#include <string.h>  // strcpy, strlen
#include <errno.h>

// mkdir / EEXISTS
#include <sys/stat.h>
#include <sys/types.h>

namespace u {
    template<typename T1, typename T2>
    using map = std::map<T1, T2>;

    template <typename T>
    using list = std::list<T>;

    template <typename T>
    using set = std::set<T>;

    template <typename T, typename D = std::default_delete<T>>
    using unique_ptr = std::unique_ptr<T, D>;

    template <typename T>
    using vector = std::vector<T>;

    using string = std::string;

    template <typename I, typename T>
    I find(I first, I last, const T &value) {
        while (first != last) {
            if (*first == value)
                return first;
            ++first;
        }
        return last;
    }

    // An implementation of std::move
    template <typename T>
    constexpr typename std::remove_reference<T>::type &&move(T &&t) {
        return static_cast<typename std::remove_reference<T>::type&&>(t);
    }

    template <typename T>
    T min(const T &lhs, const T &rhs) {
        return lhs < rhs ? lhs : rhs;
    }

    template <typename T>
    T max(const T &lhs, const T &rhs) {
        return lhs > rhs ? lhs : rhs;
    }

    template <typename T>
    void swap(T &lhs, T &rhs) {
        // use std here because their swap is more efficent than what we could
        // implement (memcpy for trivially copy assignable for instance)
        std::swap(lhs, rhs);
    }

    // A small implementation of boost.optional
    struct optional_none { };

    typedef int optional_none::*none_t;
    none_t const none = static_cast<none_t>(0);

    template <typename T>
    struct optional {
        optional(void) :
            m_init(false) { }

        // specialization for `none'
        optional(none_t) :
            m_init(false) { }

        optional(const T &value) :
            m_init(true)
        {
            construct(value);
        }

        optional(const optional<T> &opt) :
            m_init(opt.m_init)
        {
            if (m_init)
                construct(opt.get());
        }

        ~optional(void) {
            destruct();
        }

        optional &operator=(const optional<T> &opt) {
            destruct();
            if ((m_init = opt.m_init))
                construct(opt.get());
            return *this;
        }

        operator bool(void) const {
            return m_init;
        }

        T &operator *(void) {
            return get();
        }

        const T &operator*(void) const {
            return get();
        }

    private:
        void *storage(void) {
            return m_data;
        }

        const void *storage(void) const {
            return m_data;
        }

        T &get(void) {
            return *static_cast<T*>(storage());
        }

        const T &get(void) const {
            return *static_cast<const T*>(storage());
        }

        void destruct(void) {
            if (m_init)
                get().~T();
            m_init = false;
        }

        void construct(const T &data) {
            new (storage()) T(data);
        }

        bool m_init;
        alignas(alignof(T)) unsigned char m_data[sizeof(T)];
    };


    template <typename randomAccessIterator>
    inline void sort(randomAccessIterator first, randomAccessIterator last) {
        std::sort(first, last);
    }

    // check if file exists
    inline bool exists(const string &file) {
        return unique_ptr<FILE, int (*)(FILE *)>(fopen(file.c_str(), "r"), &fclose).get();
    }

    inline FILE *fopen(const string& infile, const char *type) {
        string file = infile;
        // Deal with constructing the directories if the file contains a path
        for (size_t i = 0; file[i]; i++) {
            if (i > 0 && (file[i] == '\\' || file[i] == '/')) {
                char slash = file[i];
                file[i] = '\0';
                if (mkdir(file.c_str(), 0777) != 0 && errno != EEXIST)
                    return nullptr;
                file[i] = slash;
            }
        }
        return ::fopen(file.c_str(), type);
    }

    inline int sscanf(const string &thing, const char *fmt, ...) {
        va_list va;
        va_start(va, fmt);
        int value = vsscanf(thing.c_str(), fmt, va);
        va_end(va);
        return value;
    }

    inline u::string format(const string fmt, ...) {
        int n = int(fmt.size()) * 2;
        int f = 0;
        string str;
        unique_ptr<char[]> formatted;
        va_list ap;
        for (;;) {
            formatted.reset(new char[n]);
            strcpy(&formatted[0], fmt.c_str());
            va_start(ap, fmt);
            f = vsnprintf(&formatted[0], n, fmt.c_str(), ap);
            va_end(ap);
            if (f < 0 || f >= n)
                n += abs(f - n + 1);
            else
                break;
        }
        return string(formatted.get());
    }

    inline optional<string> getline(FILE *fp) {
        string s;
        for (;;) {
            char buf[256];
            if (!fgets(buf, sizeof(buf), fp)) {
                if (feof(fp)) {
                    if (s.empty())
                        return none;
                    else
                        return u::move(s);
                }
                abort();
            }
            size_t n = strlen(buf);
            if (n && buf[n - 1] == '\n')
                --n;
            s.append(buf, n);
            if (n < sizeof(buf) - 1)
                return u::move(s);
        }
    }

    // read a file
    inline optional<vector<unsigned char>> read(const string &file, const char *mode) {
        unique_ptr<FILE, int(*)(FILE*)> fp(u::fopen(file, mode), &fclose);
        if (!fp)
            return none;

        vector<unsigned char> data;
        fseek(fp.get(), 0, SEEK_END);
        data.resize(ftell(fp.get()));
        fseek(fp.get(), 0, SEEK_SET);

        if (fread(&data[0], data.size(), 1, fp.get()) != 1)
            return none;
        return data;
    }

    // write a file
    inline bool write(const u::vector<unsigned char> &data, const string &file, const char *mode = "wb") {
        unique_ptr<FILE, int(*)(FILE*)> fp(u::fopen(file, mode), &fclose);
        if (!fp)
            return false;
        fwrite(&data[0], data.size(), 1, fp.get());
        return true;
    }

    // sha512
    struct sha512 {
        sha512(void);
        sha512(const unsigned char *buf, size_t length);

        void init(void);
        void process(const unsigned char *src, size_t length);
        void done(void);

        u::string hex(void);

    private:
        void compress(const unsigned char *buf);

        static const uint64_t K[80];

        static void inline store64(uint64_t x, unsigned char *y) {
            for (int i = 0; i != 8; ++i)
                y[i] = (x >> ((7-i) * 8)) & 255;
        }

        static inline uint64_t load64(const unsigned char *y) {
            uint64_t res = 0;
            for (int i = 0; i != 8; ++i)
                res |= uint64_t(y[i]) << ((7-i) * 8);
            return res;
        }

        static inline uint64_t ch(uint64_t x, uint64_t y, uint64_t z) {
            return z ^ (x & (y ^ z));
        }

        static inline uint64_t maj(uint64_t x, uint64_t y, uint64_t z) {
            return ((x | y) & z) | (x & y);
        }

        static inline uint64_t rot(uint64_t x, uint64_t n) {
            return (x >> (n & 63)) | (x << (64 - (n & 63)));
        }

        static inline uint64_t sh(uint64_t x, uint64_t n) {
            return x >> n;
        }

        static inline uint64_t sigma0(uint64_t x) {
            return rot(x, 28) ^ rot(x, 34) ^ rot(x, 39);
        }

        static inline uint64_t sigma1(uint64_t x) {
            return rot(x, 14) ^ rot(x, 18) ^ rot(x, 41);
        }

        static inline uint64_t gamma0(uint64_t x) {
            return rot(x, 1) ^ rot(x, 8) ^ sh(x, 7);
        }

        static inline uint64_t gamma1(uint64_t x) {
            return rot(x, 19) ^ rot(x, 61) ^ sh(x, 6);
        }

        uint64_t m_length;
        uint64_t m_state[8];
        uint32_t m_currentLength;
        unsigned char m_buffer[128];
        unsigned char m_out[512 / 8];
    };

    // zlib decompression
    struct zlib {
        bool decompress(u::vector<unsigned char> &out, const u::vector<unsigned char> &in);
    private:
        static size_t readBitFromStream(size_t &bitp, const unsigned char *bits);
        static size_t readBitsFromStream(size_t &bitp, const unsigned char *bits, size_t nbits);

        struct huffmanTree {
            bool make(const u::vector<size_t> &bitlen, size_t maxbitlen);
            // Decodes a symbol from the tree
            bool decode(bool& decoded, size_t &r, size_t &treepos, size_t bit) const;
            // 2D representation of a huffman tree: The one dimension is "0" or "1", the other contains all nodes and leaves of the tree.

        private:
            u::vector<size_t> m_tree2D;
        };

        struct inflator {
            inflator() :
                m_error(false) { }

            void inflate(u::vector<unsigned char> &out, const u::vector<unsigned char> &in, size_t inpos = 0);

            // get the tree of a deflated block with fixed tree
            void generateFixedTrees(huffmanTree& tree, huffmanTree& treeD);

            // decode a single symbol from given list of bits with given code tree. return value is the symbol
            size_t huffmanDecodeSymbol(const unsigned char *in, size_t &bp, const huffmanTree &codetree, size_t inlength);

            // get the tree of a deflated block with dynamic tree, the tree itself is also Huffman compressed with a known tree
            void getTreeInflateDynamic(huffmanTree &tree, huffmanTree &treeD, const unsigned char *in, size_t &bp, size_t inlength);

            void inflateHuffmanBlock(u::vector<unsigned char> &out, const unsigned char *in, size_t &bp, size_t &pos, size_t inlength, size_t btype);
            void inflateNoCompression(u::vector<unsigned char> &out, const unsigned char *in, size_t &bp, size_t &pos, size_t inlength);

        private:
            friend struct zlib;

            bool m_error;

            // the code tree for Huffman codes, dist codes, and code length codes
            huffmanTree m_codeTree;
            huffmanTree m_codeTreeDistance;
            huffmanTree m_codeLengthCodeTree;
        };
    };
}

#endif
