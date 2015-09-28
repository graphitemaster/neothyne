#include <string.h>

#include "grader.h"

#include "u_algorithm.h"

#include "m_const.h"

colorGrader::colorGrader() {
    reset();

    generateTexture();

    auto f1 = [](double x) {
        return 1.075 - 1.0/(x/16.0 + 1);
    };

    auto f2 = [](double x) {
        return 0.667 * (1.0 - u::square((x - 127.0) / 127.0));
    };

    // Compute color balance function for all 256 pixel values for shadows,
    // midtones and highlights.
    for (size_t i = 0; i < 256; i++) {
        const double low = f1(i);
        const double mid = f2(i);
        m_balanceAdd[kBalanceShadows][i] = low;
        m_balanceSub[kBalanceShadows][255 - i] = low;
        m_balanceAdd[kBalanceMidtones][i] = mid;
        m_balanceSub[kBalanceMidtones][i] = mid;
        m_balanceAdd[kBalanceHighlights][255 - i] = low;
        m_balanceSub[kBalanceHighlights][i] = low;
    }
}

void colorGrader::setBrightness(double brightness) {
    m_brightness = brightness;
    m_updated = true;
}

void colorGrader::setContrast(double contrast) {
    m_contrast = contrast;
    m_updated = true;
}

double colorGrader::brightness() const {
    return m_brightness;
}

double colorGrader::contrast() const {
    return m_contrast;
}

void colorGrader::generateTexture() {
    // Generate the 3D volume texture
    unsigned char *next = m_data;
    for (size_t y = 0; y < kHeight; y++) {
        for (size_t x = 0; x < kWidth; x++) {
            *next++ = 17 * (x % 16);
            *next++ = 17 * (x / 16);
            *next++ = 17 * y;
        }
    }
}

void colorGrader::generateColorBalanceTables() {
    const double *transfer[3][kBalanceMax];
    for (size_t i = kBalanceShadows; i < kBalanceMax; i++)
        for (size_t j = 0; j < 3; j++)
            transfer[j][i] = m_balance[j][i] > 0.0 ? m_balanceAdd[i] : m_balanceSub[i];
    for (size_t i = 0; i < 256; i++) {
        int color[3];
        for (size_t j = 0; j < 3; j++)
            color[j] = i;
        for (size_t j = 0; j < 3; j++)
            for (size_t k = kBalanceShadows; k < kBalanceMax; k++)
                color[j] = m::clamp(color[j] + m_balance[j][k] * transfer[j][k][color[j]], 0.0, 255.0);
        for (size_t j = 0; j < 3; j++)
            m_balanceLookup[j][i] = color[j];
    }
}

void colorGrader::generateHueSaturationTables() {
    int value;
    for (int hue = 0; hue < 6; hue++) {
        for (int i = 0; i < 256; i++) {
            // Hue
            value = (m_hue[0] + m_hue[hue + 1]) * 255.0 / 360.0;
            if ((i + value) < 0)
                m_hLookup[hue][i] = 255 + (i + value);
            else if ((i + value) > 255)
                m_hLookup[hue][i] = i + value - 255;
            else
                m_hLookup[hue][i] = i + value;

            // Saturation
            value = m::clamp((m_saturation[0] + m_saturation[hue + 1]) * 255.0 / 100.0, -255.0, 255.0);
            m_sLookup[hue][i] = m::clamp((i * (255 + value)) / 255, 0, 255);

            // Lightness
            value = m::clamp((m_lightness[0] + m_lightness[hue + 1]) * 127.0 / 100.0, -255.0, 255.0);
            m_lLookup[hue][i] = value < 0
                ? (unsigned char)((i * (255 + value)) / 255)
                : (unsigned char)(i + ((255 - i) * value) / 255);
        }
    }
}

// Changes arguments in correspondence to HSL.
// Ranges in the following:
//  H [0, 360]
//  S [0, 255]
//  L [0, 255]
void colorGrader::RGB2HSL(int &red, int &green, int &blue) {
    const int r = red;
    const int g = green;
    const int b = blue;

    int min;
    int max;
    if (r > g) {
        max = u::max(r, b);
        min = u::min(g, b);
    } else {
        max = u::max(g, b);
        min = u::min(r, b);
    }

    double h = 0.0;
    double s = 0.0;
    const double l = (max + min) / 2.0;
    if (max != min) {
        int delta = max - min;
        if (l < 128.0)
            s = 255.0 * double(delta) / double(max + min);
        else
            s = 255.0 * double(delta) / double(511 - max - min);

        if (r == max)
            h = (g - b) / double(delta);
        else if (g == max)
            h = 2.0 + (b - r) / double(delta);
        else
            h = 4.0 + (r - g) / double(delta);

        h = h * 42.5;

        if (h < 0)
            h += 255.0;
        else if (h > 255.0)
            h -= 255.0;
    }

    red = u::round(h);
    green = u::round(s);
    blue = u::round(l);
}

int colorGrader::HSLINT(double n1, double n2, double hue) {
    if (hue > 255.0)
        hue -= 255.0;
    else if (hue < 0.0)
        hue += 255.0;

    double value;
    if (hue < 42.5)
        value = n1 + (n2 - n1) * (hue / 42.5);
    else if (hue < 127.5)
        value = n2;
    else if (hue < 170.0)
        value = n1 + (n2 - n1) * ((170.0 - hue) / 42.5);
    else
        value = n1;

    return u::round(value * 255.0);
}

void colorGrader::HSL2RGB(int &hue, int &saturation, int &lightness) {
    const int h = hue;
    const int s = saturation;
    const int l = lightness;
    if (s == 0) { // Achromatic
        hue = l;
        saturation = l;
        lightness = l;
    } else { // Chromatic
        double m2;
        if (l < 128)
            m2 = (l * (255 + s)) / 65025.0;
        else
            m2 = (l + s - (l * s) / 255.0) / 255.0;
        double m1 = (l / 127.5) - m2;
        hue = HSLINT(m1, m2, h + 85);
        saturation = HSLINT(m1, m2, h);
        lightness = HSLINT(m1, m2, h - 85);
    }
}

// Calculates lightness value for RGB triplet with formula
// L = (max(R, G, B) + min(R, G, B)) / 2
int colorGrader::RGB2L(int red, int green, int blue) {
    int min = 0;
    int max = 0;
    if (red > green) {
        max = u::max(red, blue);
        min = u::min(green, blue);
    } else {
        max = u::max(green, blue);
        min = u::min(red, blue);
    }
    return u::round((max + min) / 2.0);
}

void colorGrader::applyColorBalance() {
    generateColorBalanceTables();
    unsigned char *src = m_data;
    unsigned char *dst = m_data;
    for (size_t h = 0; h < kHeight; h++) {
        unsigned char *s = src;
        unsigned char *d = dst;
        for (size_t w = 0; w < kWidth; w++) {
            int l[3];
            int n[3];
            for (size_t i = 0; i < 3; i++)
                l[i] = s[i];
            for (size_t i = 0; i < 3; i++)
                n[i] = m_balanceLookup[i][l[i]];
            if (m_preserveLuma) {
                RGB2HSL(n[0], n[1], n[2]);
                n[2] = RGB2L(l[0], l[1], l[2]);
                HSL2RGB(n[0], n[1], n[2]);
            }
            for (size_t i = 0; i < 3; i++)
                d[i] = n[i];
            s += 3;
            d += 3;
        }
        src += 3*kWidth;
        dst += 3*kWidth;
    }
}

void colorGrader::applyHueSaturation() {
    generateHueSaturationTables();

    // +42ish each step, capped to a byte, rounded down to nearest integer
    // i.e The ratio 256/6 times 0.5 1.5 2.5 ... 6.5 floored and then saturated
    // to [0, 255].
    //
    // how to generate the table in J:
    //  255 <. <. 256r6 * 0.5 + i.7     NB. With 255 and <. to saturate the result
    static constexpr int kHueThresholds[] = {
        21, 64, 106, 149, 192, 234, 255
    };

    unsigned char *src = m_data;
    unsigned char *dst = m_data;

    int phue = 0;
    int shue = 0;
    float pintensity = 0.0f;
    float sintensity = 0.0f;
    bool useSecondaryHue = false;
    float overlapHue = (m_hueOverlap / 100.0) * 21.0;

    for (size_t h = 0; h < kHeight; h++) {
        unsigned char *s = src;
        unsigned char *d = dst;
        for (size_t w = 0; w < kWidth; w++) {
            int r = s[0];
            int g = s[1];
            int b = s[2];
            RGB2HSL(r, g, b);

            phue = (r + (128 / 6)) / 6;

            for (int c = 0; c < 7; c++) {
                if (r < kHueThresholds[c] + overlapHue) {
                    int hueThreshold = kHueThresholds[c];
                    phue = c;
                    if (overlapHue > 1.0 && r > hueThreshold - overlapHue) {
                        shue = c + 1;
                        sintensity = (r - hueThreshold + overlapHue) / (2.0 * overlapHue);
                        pintensity = 1.0f - sintensity;
                        useSecondaryHue = true;
                    } else {
                        useSecondaryHue = false;
                    }
                    break;
                }
            }
            if (phue >= 6) {
                phue = 0;
                useSecondaryHue = false;
            }
            if (shue >= 6)
                shue = 0;
            if (useSecondaryHue) {
                int diff = m_hLookup[phue][r] - m_hLookup[shue][r];
                if (diff < -127 || diff >= 128) {
                    r = int(m_hLookup[phue][r] * pintensity +
                            (m_hLookup[shue][r] + 255) * sintensity) % 255;
                } else {
                    r = m_hLookup[phue][r] * pintensity +
                        m_hLookup[shue][r] * sintensity;
                }

                g = m_sLookup[phue][g] * pintensity + m_sLookup[shue][g] * sintensity;
                b = m_lLookup[phue][b] * pintensity + m_lLookup[shue][b] * sintensity;
            } else {
                r = m_hLookup[shue][r];
                g = m_sLookup[shue][g];
                b = m_lLookup[shue][b];
            }

            HSL2RGB(r, g, b);

            d[0] = r;
            d[1] = g;
            d[2] = b;

            s += 3;
            d += 3;
        }
        src += 3*kWidth;
        dst += 3*kWidth;
    }
}

void colorGrader::applyBrightnessContrast() {
    const float brightness = 255.0f * 0.392f * float(m_brightness);
    const float contrast = float(m_contrast);
    float gain = 1.0f;

    unsigned char *src = m_data;
    unsigned char *dst = m_data;

    //  if -1 <= contrast < 0, 0 <= gain < 1
    //  if contrast = 0; gain = 1 or no change
    //  if 0 > contrast < 1, 1 < gain < infinity
    if (contrast != 0.0f) {
        if (contrast > 0.0f)
            gain = 1.0f / (1.0f - contrast);
        else if (contrast < 0.0f)
            gain = 1.0f + contrast;
    }

    // 1/2(gain *max - max), where max = 2^8-1
    const float shift = (gain * 127.5f - 127.5f) - 0.5f;

    for (size_t h = 0; h < kHeight; h++) {
        unsigned char *s = src;
        unsigned char *d = dst;
        for (size_t w = 0; w < kWidth; w++) {
            for (size_t b = 0; b < 3; b++) {
                // Brightness & contrast correction
                const float tmp = m::clamp(gain*(brightness + s[b]) - shift, 0.0f, 255.0f);
                d[b] = (unsigned char)(tmp);
            }
            s += 3;
            d += 3;
        }
        src += 3*kWidth;
        dst += 3*kWidth;
    }
}

void colorGrader::grade() {
    applyColorBalance();
    applyHueSaturation();
    applyBrightnessContrast();
}

void colorGrader::update() {
    generateTexture();
    m_updated = false;
}

void colorGrader::reset() {
    memset(m_balance, 0, sizeof(m_balance));

    for (size_t i = kHuesAll; i < kHuesMax; i++) {
        m_hue[i] = 0.0;
        m_saturation[i] = 0.0;
        m_lightness[i] = 0.0;
    }

    m_preserveLuma = true;
    m_brightness = 0.0;
    m_contrast = 0.0;
    m_hueOverlap = 0.0;
    m_updated = true;
}

bool colorGrader::updated() const {
    return m_updated;
}

void colorGrader::setLuma(bool keep) {
    m_preserveLuma = keep;
    m_updated = true;
}

void colorGrader::setCR(double value, int what) {
    m_balance[0][what] = value;
    m_updated = true;
}

void colorGrader::setMG(double value, int what) {
    m_balance[1][what] = value;
    m_updated = true;
}

void colorGrader::setYB(double value, int what) {
    m_balance[2][what] = value;
    m_updated = true;
}

void colorGrader::setH(double hue, int what) {
    m_hue[what] = hue;
    m_updated = true;
}

void colorGrader::setS(double saturation, int what) {
    m_saturation[what] = saturation;
    m_updated = true;
}

void colorGrader::setL(double lightness, int what) {
    m_lightness[what] = lightness;
    m_updated = true;
}

void colorGrader::setHueOverlap(double value) {
    m_hueOverlap = value;
    m_updated = true;
}

bool colorGrader::luma() const {
    return m_preserveLuma;
}

double colorGrader::CR(int what) const {
    return m_balance[0][what];
}

double colorGrader::MG(int what) const {
    return m_balance[1][what];
}

double colorGrader::YB(int what) const {
    return m_balance[2][what];
}

double colorGrader::H(int what) const {
    return m_hue[what];
}

double colorGrader::S(int what) const {
    return m_saturation[what];
}

double colorGrader::hueOverlap() const {
    return m_hueOverlap;
}

double colorGrader::L(int what) const {
    return m_lightness[what];
}

const unsigned char *colorGrader::data() const {
    return m_data;
}
