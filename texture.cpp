#include <SDL/SDL_image.h>

#include "texture.h"
#include "math.h"

template <size_t S>
void texture::halve(unsigned char *src, size_t sw, size_t sh, size_t stride, unsigned char *dst) {
    for (unsigned char *yend = src + sh * stride; src < yend;) {
        for (unsigned char *xend = src + sw * S, *xsrc = src; xsrc < xend; xsrc += 2 * S, dst += S) {
            for (size_t i = 0; i < S; i++)
                dst[i] = (size_t(xsrc[i]) + size_t(xsrc[i+S]) + size_t(xsrc[stride+i]) + size_t(xsrc[stride+i+S])) >> 2;
        }
        src += 2 * stride;
    }
}

template <size_t S>
void texture::shift(unsigned char *src, size_t sw, size_t sh, size_t stride, unsigned char *dst, size_t dw, size_t dh) {
    size_t wfrac = sw/dw, hfrac = sh/dh, wshift = 0, hshift = 0;
    while (dw << wshift < sw) wshift++;
    while (dh << hshift < sh) hshift++;
    size_t tshift = wshift + hshift;
    for (unsigned char *yend = src + sh * stride; src < yend; ) {
        for (unsigned char *xend = src + sw * S, *xsrc = src; xsrc < xend; xsrc += wfrac * S, dst += S) {
            size_t r[S] = {0};
            for (unsigned char *ycur = xsrc, *xend = ycur + wfrac * S,
                    *yend = src + hfrac * stride; ycur<yend; ycur += stride, xend += stride) {
                for (unsigned char *xcur = ycur; xcur < xend; xcur += S) {
                    for (size_t i = 0; i < S; i++)
                        r[i] += xcur[i];
                }
            }
            for (size_t i = 0; i < S; i++)
                dst[i] = (r[i]) >> tshift;
        }
        src += hfrac*stride;
    }
}

template <size_t S>
void texture::scale(unsigned char *src, size_t sw, size_t sh, size_t stride, unsigned char *dst, size_t dw, size_t dh) {
    size_t wfrac = (sw << 12) / dw;
    size_t hfrac = (sh << 12) / dh;
    size_t darea = dw * dh;
    size_t sarea = sw * sh;
    int over, under;
    for (over = 0; (darea >> over) > sarea; over++);
    for (under = 0; (darea << under) < sarea; under++);
    size_t cscale = m::clamp(under, over - 12, 12),
    ascale = m::clamp(12 + under - over, 0, 24),
    dscale = ascale + 12 - cscale,
    area = ((unsigned long long)darea << ascale) / sarea;
    dw *= wfrac;
    dh *= hfrac;
    for (size_t y = 0; y < dh; y += hfrac) {
        const size_t yn = y + hfrac - 1;
        const size_t yi = y >> 12;
        const size_t h = (yn >> 12) - yi;
        const size_t ylow = ((yn | (-int(h) >> 24)) & 0xFFFU) + 1 - (y & 0xFFFU);
        const size_t yhigh = (yn & 0xFFFU) + 1;
        const unsigned char *ysrc = src + yi * stride;
        for (size_t x = 0; x < dw; x += wfrac, dst += S) {
            const size_t xn = x + wfrac - 1;
            const size_t xi = x >> 12;
            const size_t w = (xn >> 12) - xi;
            const size_t xlow = ((w + 0xFFFU) & 0x1000U) - (x & 0xFFFU);
            const size_t xhigh = (xn & 0xFFFU) + 1;
            const unsigned char *xsrc = ysrc + xi * S;
            const unsigned char *xend = xsrc + w * S;
            size_t r[S] = {0};
            for (const unsigned char *xcur = xsrc + S; xcur < xend; xcur += S)
                for (size_t i = 0; i < S; i++)
                    r[i] += xcur[i];
            for (size_t i = 0; i < S; i++)
                r[i] = (ylow * (r[i] + ((xsrc[i] * xlow + xend[i] * xhigh) >> 12))) >> cscale;
            if (h) {
                xsrc += stride;
                xend += stride;
                for (size_t hcur = h; --hcur; xsrc += stride, xend += stride) {
                    size_t p[S] = {0};
                    for (const unsigned char *xcur = xsrc + S; xcur < xend; xcur += S)
                        for (size_t i = 0; i < S; i++)
                            p[i] += xcur[i];
                    for (size_t i = 0; i < S; i++)
                        r[i] += ((p[i] << 12) + xsrc[i] * xlow + xend[i] * xhigh) >> cscale;
                }
                size_t p[S] = {0};
                for (const unsigned char *xcur = xsrc + S; xcur < xend; xcur += S)
                    for (size_t i = 0; i < S; i++)
                        p[i] += xcur[i];
                for (size_t i = 0; i < S; i++)
                    r[i] += (yhigh * (p[i] + ((xsrc[i] * xlow + xend[i] * xhigh) >> 12))) >> cscale;
            }
            for (size_t i = 0; i < S; i++)
                dst[i] = (r[i] * area) >> dscale;
        }
    }
}

void texture::scale(unsigned char *src, size_t sw, size_t sh, size_t bpp, size_t pitch, unsigned char *dst, size_t dw, size_t dh) {
    if (sw == dw * 2 && sh == dh * 2)
        switch (bpp) {
            case 3: return halve<3>(src, sw, sh, pitch, dst);
            case 4: return halve<4>(src, sw, sh, pitch, dst);
        }
    else if(sw < dw || sh < dh || sw&(sw-1) || sh&(sh-1) || dw&(dw-1) || dh&(dh-1))
        switch (bpp) {
            case 3: return scale<3>(src, sw, sh, pitch, dst, dw, dh);
            case 4: return scale<4>(src, sw, sh, pitch, dst, dw, dh);
        }
    switch(bpp) {
        case 3: return shift<3>(src, sw, sh, pitch, dst, dw, dh);
        case 4: return shift<4>(src, sw, sh, pitch, dst, dw, dh);
    }
}

void texture::reorient(unsigned char *src, size_t sw, size_t sh, size_t bpp, size_t stride, unsigned char *dst, bool flipx, bool flipy, bool swapxy) {
    size_t stridex = swapxy ? bpp * sh : bpp;
    size_t stridey = swapxy ? bpp : bpp * sw;
    if (flipx)
        dst += (sw - 1) * stridex, stridex = -stridex;
    if (flipy)
        dst += (sh - 1) * stridey, stridey = -stridey;
    unsigned char *srcrow = src;
    for (size_t i = 0; i < sh; i++) {
        for (unsigned char *curdst = dst, *src = srcrow, *end = srcrow + sw * bpp; src < end; ) {
            for (size_t k = 0; k < bpp; k++)
                curdst[k] = *src++;
            curdst += stridex;
        }
        srcrow += stride;
        dst += stridey;
    }
}

bool texture::load(const u::string &file) {
    SDL_Surface *surface = IMG_Load(file.c_str());
    if (!surface)
        return false;

    m_width = surface->w;
    m_height = surface->h;
    m_bpp = 3; // surface->format->BytesPerPixel;// ? surface->format->BytesPerPixel : 3;
    m_pitch = surface->pitch;
    const size_t size = m_bpp * m_width * m_height;

    switch (m_bpp) {
        case 3:
            m_format = (surface->format->Rmask == 0x000000FF) ? TEX_RGB : TEX_BGR;
            break;
        case 4:
            m_format = (surface->format->Rmask == 0x000000FF) ? TEX_RGBA : TEX_BGRA;
            break;
    }

    // copy the SDL surface contents
    SDL_LockSurface(surface);
    m_data = new unsigned char[size];
    memcpy(m_data, surface->pixels, size);
    SDL_UnlockSurface(surface);

    SDL_FreeSurface(surface);
    return true;
}

texture::texture(void) :
    m_data(nullptr)
{

}

texture::~texture(void) {
    delete[] m_data;
}

void texture::resize(size_t width, size_t height) {
    unsigned char *data = new unsigned char [m_bpp * width * height];
    scale(m_data, m_width, m_height, m_bpp, m_pitch, data, width, height);
    delete[] m_data;
    m_data = data;
    m_width = width;
    m_height = height;
    m_pitch = m_width * m_bpp;
}

size_t texture::width(void) const {
    return m_width;
}

size_t texture::height(void) const {
    return m_height;
}

textureFormat texture::format(void) const {
    return m_format;
}

unsigned char *texture::data(void) const {
    return m_data;
}
