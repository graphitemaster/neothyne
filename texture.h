#ifndef TEXTURE_HDR
#define TEXTURE_HDR
#include "u_string.h"
#include "u_optional.h"
#include "u_vector.h"

enum {
    kTexFlagNormal     = 1 << 0,
    kTexFlagGrey       = 1 << 1,
    kTexFlagDisk       = 1 << 2,
    kTexFlagPremul     = 1 << 3,
    kTexFlagNoCompress = 1 << 4,
    kTexFlagCompressed = 1 << 5
};

enum textureFormat {
    kTexFormatRGB,
    kTexFormatRGBA,
    kTexFormatBGR,
    kTexFormatBGRA,
    kTexFormatRG,
    kTexFormatLuminance,
    kTexFormatDXT1,
    kTexFormatDXT3,
    kTexFormatDXT5,
    kTexFormatBC4U, // Also ATI1
    kTexFormatBC4S,
    kTexFormatBC5U, // also ATI2
    kTexFormatBC5S
};

struct texture {
    texture()
        : m_width(0)
        , m_height(0)
        , m_bpp(0)
        , m_pitch(0)
        , m_mips(0)
        , m_flags(0)
        , m_format(kTexFormatLuminance)
    {
    }

    texture(const unsigned char *const data, size_t length, size_t width,
        size_t height, bool normal, textureFormat format);

    bool load(const u::string &file, float quality = 1.0f);
    bool from(const unsigned char *const data, size_t length, size_t width,
        size_t height, bool normal, textureFormat format);

    template <size_t S>
    static void halve(unsigned char *src, size_t sw, size_t sh, size_t stride,
        unsigned char *dst);

    template <size_t S>
    static void shift(unsigned char *src, size_t sw, size_t sh, size_t stride,
        unsigned char *dst, size_t dw, size_t dh);

    template <size_t S>
    static void scale(unsigned char *src, size_t sw, size_t sh, size_t stride,
        unsigned char *dst, size_t dw, size_t dh);

    static void scale(unsigned char *src, size_t sw, size_t sh, size_t bpp,
        size_t pitch, unsigned char *dst, size_t dw, size_t dh);
    static void reorient(unsigned char *src, size_t sw, size_t sh, size_t bpp,
        size_t stride, unsigned char *dst, bool flipx, bool flipy, bool swapxy);

    void resize(size_t width, size_t height);

    template <textureFormat F>
    void convert();

    size_t width() const;
    size_t height() const;
    textureFormat format() const;

    const unsigned char *data() const;

    size_t size() const;
    size_t bpp() const;
    size_t pitch() const;
    size_t mips() const;

    const u::string &hashString() const;

    int flags() const;

    void unload();

private:
    u::optional<u::string> find(const u::string &file);
    template <typename T>
    bool decode(const u::vector<unsigned char> &data, const char *fileName,
        float quality = 1.0f);

    u::string m_hashString;
    u::vector<unsigned char> m_data;
    size_t m_width;
    size_t m_height;
    size_t m_bpp;
    size_t m_pitch;
    size_t m_mips;
    int m_flags;
    textureFormat m_format;
};

#endif
