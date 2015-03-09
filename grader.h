#ifndef GRADER_HDR
#define GRADER_HDR
#include <stddef.h>

struct colorGrader {
    colorGrader();
    void setBrightness(double brightness);
    void setContrast(double contrast);
    double brightness() const;
    double contrast() const;

    enum {
        kBalanceShadows,
        kBalanceMidtones,
        kBalanceHighlights,
        kBalanceMax
    };

    enum {
        kHuesAll,
        kHuesRed,
        kHuesYellow,
        kHuesGreen,
        kHuesCyan,
        kHuesBlue,
        kHuesMagenta,
        kHuesMax
    };

    void setLuma(bool keep);
    void setCR(double value, int what);
    void setMG(double value, int what);
    void setYB(double value, int what);
    void setH(double hue, int what);
    void setS(double saturation, int what);
    void setL(double lightness, int what);
    void setHueOverlap(double value);

    bool luma() const;
    double CR(int what) const;
    double MG(int what) const;
    double YB(int what) const;
    double H(int what) const;
    double S(int what) const;
    double L(int what) const;
    double hueOverlap() const;
    const unsigned char *data() const;

    bool updated() const;

    void reset();
    void update();

    void grade();

protected:
    void applyBrightnessContrast();
    void applyColorBalance();
    void applyHueSaturation();

    void generateColorBalanceTables();
    void generateHueSaturationTables();

    void generateTexture();

    // Color space conversion functions
    static void RGB2HSL(int &red, int &green, int &blue);
    static int HSLINT(double n1, double n2, double hue);
    static void HSL2RGB(int &hue, int &saturation, int &lightness);
    static int RGB2L(int red, int green, int blue);

private:
    // Brightness & contrast
    double m_brightness;
    double m_contrast;

    // Hue & saturation
    double m_hue[kHuesMax];
    double m_lightness[kHuesMax];
    double m_saturation[kHuesMax];
    double m_hueOverlap;
    int m_hLookup[6][256];
    int m_sLookup[6][256];
    int m_lLookup[6][256];

    // color balance
    bool m_preserveLuma;
    double m_balance[3][kBalanceMax];
    double m_balanceAdd[kBalanceMax][256];
    double m_balanceSub[kBalanceMax][256];
    unsigned char m_balanceLookup[3][256];

    // 3D LUT
    static constexpr size_t kWidth = 256u; //16*16
    static constexpr size_t kHeight = 16u;
    unsigned char m_data[kWidth * kHeight * 3];

    bool m_updated;
};

#endif
