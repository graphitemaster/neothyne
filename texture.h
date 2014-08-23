#ifndef TEXTURE_HDR
#define TEXTURE_HDR
#include "util.h"

enum textureFormat {
    TEX_RGB,
    TEX_RGBA,
    TEX_BGR,
    TEX_BGRA
};

struct texture {
    texture();
    ~texture();

    bool load(const u::string &file);

    template <size_t S>
    static void halve(unsigned char *src, size_t sw, size_t sh, size_t stride, unsigned char *dst);

    template <size_t S>
    static void shift(unsigned char *src, size_t sw, size_t sh, size_t stride, unsigned char *dst, size_t dw, size_t dh);

    template <size_t S>
    static void scale(unsigned char *src, size_t sw, size_t sh, size_t stride, unsigned char *dst, size_t dw, size_t dh);

    static void scale(unsigned char *src, size_t sw, size_t sh, size_t bpp, size_t pitch, unsigned char *dst, size_t dw, size_t dh);
    static void reorient(unsigned char *src, size_t sw, size_t sh, size_t bpp, size_t stride, unsigned char *dst, bool flipx, bool flipy, bool swapxy);

    void resize(size_t width, size_t height);

    size_t width(void) const;
    size_t height(void) const;
    textureFormat format(void) const;

    unsigned char *data(void) const;

private:
    unsigned char *m_data;
    size_t m_width;
    size_t m_height;
    size_t m_bpp;
    size_t m_pitch;
    textureFormat m_format;
};

#endif
