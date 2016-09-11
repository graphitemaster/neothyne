#include "engine.h"
#include "world.h"
#include "client.h"
#include "menu.h"
#include "grader.h"
#include "gui.h"

#include "c_variable.h"
#include "c_console.h"

#include "u_set.h"
#include "u_misc.h"
#include "u_file.h"
#include "u_log.h"

extern bool gPlaying;
extern bool gRunning;
extern world::descriptor *gSelected;
extern world gWorld;
extern client gClient;

int gMenuState = kMenuMain | kMenuConsole; // Default state

u::stack<u::string, kMenuConsoleHistorySize> gMenuConsole; // The console text buffer

static u::map<u::string, int> gMenuData;
static u::map<u::string, u::string> gMenuStrings;
static u::vector<u::string> gMenuPaths;

#define D(X) gMenuData[u::format("%s_%s", __func__, #X)]
#define STR(X) gMenuStrings[u::format("%s_%s", __func__, #X)]

#define FMT(N, ...) u::format("%"#N"s..", __VA_ARGS__).c_str()

#define PP_COUNT(X) (sizeof (X) / sizeof *(X))

static const char *kCreditsEngine[] = {
    "Dale 'graphitemaster' Weiler"
};

static const char *kCreditsDesign[] = {
    "Maxim 'acerspyro' Therrien"
};

static const char *kCreditsSpecialThanks[] = {
    "Lee 'eihrul' Salzman",
    "Wolfgang 'Blub\\w' Bumiller",
    "Forest 'LordHavoc' Hale"
};

static u::vector<const char*> kAspectRatios = {
    "3:2", "4:3", "5:3", "5:4", "16:9", "16:10", "17:9"
};

struct Resolution {
    int width;
    int height;
};

static u::vector<u::vector<const char*>> kResolutionStrings = {
{
    "720x480",
    "1152x768",
    "1280x854",
    "1440x960",
    "2880x1920"
},{
    "320x240",
    "640x480",
    "800x600",
    "1024x768",
    "1152x864",
    "1280x960",
    "1400x1050",
    "1600x1200",
    "2048x1536",
    "3200x2400",
    "4000x3000",
    "6400x4800"
}, {
    "800x480",
    "1280x768"
}, {
    "1280x1024",
    "2560x2048",
    "5120x4096"
}, {
    "852x480",
    "1280x720",
    "1365x768",
    "1600x900",
    "1920x1080"
}, {
    "320x200",
    "640x400",
    "1280x800",
    "1440x900",
    "1680x1050",
    "1920x1200",
    "2560x1600",
    "3840x2400",
    "7680x4800"
}, {
    "2048,1080"
}};

static u::vector<u::vector<Resolution>> kResolutions = {
{
    { 720, 480 },
    { 1152, 768 },
    { 1280, 854 },
    { 1440, 960 },
    { 2880, 1920 }
},{
    { 320, 240 },
    { 640, 480 },
    { 800, 600 },
    { 1024, 768 },
    { 1152, 864 },
    { 1280, 960 },
    { 1400, 1050 },
    { 1600, 1200 },
    { 2048, 1536 },
    { 3200, 2400 },
    { 4000, 3000 },
    { 6400, 4800 }
}, {
    { 800, 480 },
    { 1280, 768 }
}, {
    { 1280, 1024 },
    { 2560, 2048 },
    { 5120, 4096 }
}, {
    { 852, 480 },
    { 1280, 720 },
    { 1365, 768 },
    { 1600, 900 },
    { 1920, 1080 }
}, {
    { 320, 200 },
    { 640, 400 },
    { 1280, 800 },
    { 1440, 900 },
    { 1680, 1050 },
    { 1920, 1200 },
    { 2560, 1600 },
    { 3840, 2400 },
    { 7680, 4800 }
}, {
    { 2048,1080 }
}};


static m::vec3 looking() {
    world::trace::hit h;
    world::trace::query q;
    q.start = gClient.getPosition();
    q.radius = 0.01f;
    m::vec3 direction;
    gClient.getDirection(&direction, nullptr, nullptr);
    q.direction = direction.normalized();
    gWorld.trace(q, &h, 1024.0f, false);
    return h.position;
}

static m::vec3 randomColor() {
    return { u::randf(), u::randf(), u::randf() };
}

static void menuMain() {
    const size_t w = neoWidth() / 10;
    const size_t h = neoHeight() / 5;
    const size_t x = neoWidth() / 2 - w / 2;
    const size_t y = neoHeight() / 2 - h / 2;

    gui::areaBegin("Main", x, y, w, h, D(scroll));
        gui::heading();
        if (gui::button("Play")) {
            gPlaying = true;
            gMenuState &= ~kMenuMain;
            neoRelativeMouse(true);
        }
        if (gui::button("Create")) {
            //gPlaying = true;
            gMenuState ^= kMenuCreate;
            gMenuState &= ~kMenuMain;
        }
        if (gui::button("Options")) {
            gMenuState ^= kMenuOptions;
            gMenuState &= ~kMenuMain;
        }
        if (gui::button("Credits")) {
            gMenuState ^= kMenuCredits;
            gMenuState &= ~kMenuMain;
        }
        if (gui::button("Exit")) {
            gRunning = false;
        }
    gui::areaFinish();
}

static void menuColorGrading() {
    const size_t w = neoWidth() / 3;
    const size_t h = neoHeight() - (neoHeight() / 4);
    const size_t x = neoWidth() / 2 - w / 2;
    const size_t y = neoHeight() / 2 - h / 2;

    auto *colorGrad = gWorld.getColorGrader();
    if (!colorGrad) return;
    auto &colorGrading = *colorGrad;

    auto cmySliders = [&](int what) {
        float cr = colorGrading.CR(what);
        float mg = colorGrading.MG(what);
        float yb = colorGrading.YB(what);
        if (gui::slider("Cyan - Red", cr, 0.0f, 255.0f, 0.001f))
            colorGrading.setCR(cr, what);
        if (gui::slider("Magenta - Green", mg, 0.0f, 255.0f, 0.001f))
            colorGrading.setMG(mg, what);
        if (gui::slider("Yellow - Blue", yb, 0.0f, 255.0f, 0.001f))
            colorGrading.setYB(yb, what);
    };

    auto hslSliders = [&](int what) {
        float h = colorGrading.H(what);
        float s = colorGrading.S(what);
        float l = colorGrading.L(what);
        if (gui::slider("Hue", h, 0.0f, 255.0f, 0.001f))
            colorGrading.setH(h, what);
        if (gui::slider("Saturation", s, -255.0f, 255.0f, 0.001f))
            colorGrading.setS(s, what);
        if (gui::slider("Lightness", l, 0.0f, 255.0f, 0.001f))
            colorGrading.setL(l, what);
    };

    gui::areaBegin("Color grading", x, y, w, h, D(scroll));
        gui::heading();
        gui::label("Tone balance");
        gui::indent();
            if (gui::check("Preserve luminosity", colorGrading.luma()))
                colorGrading.setLuma(!colorGrading.luma());
            if (gui::collapse("Shadows", "", D(shadows)))
                D(shadows) = !D(shadows);
            if (D(shadows))
                cmySliders(ColorGrader::kBalanceShadows);
            if (gui::collapse("Midtones", "", D(midtones)))
                D(midtones) = !D(midtones);
            if (D(midtones))
                cmySliders(ColorGrader::kBalanceMidtones);
            if (gui::collapse("Highlights", "", D(highlights)))
                D(highlights) = !D(highlights);
            if (D(highlights))
                cmySliders(ColorGrader::kBalanceHighlights);
        gui::dedent();
        gui::label("Hue and Saturation");
        gui::indent();
            float hueOverlap = colorGrading.hueOverlap();
            if (gui::slider("Overlap", hueOverlap, 0.0f, 255.0f, 0.001f))
                colorGrading.setHueOverlap(hueOverlap);
            // NEXT
            if (gui::collapse("Red", "", D(red)))
                D(red) = !D(red);
            if (D(red))
                hslSliders(ColorGrader::kHuesRed);
            if (gui::collapse("Yellow", "", D(yellow)))
                D(yellow) = !D(yellow);
            if (D(yellow))
                hslSliders(ColorGrader::kHuesYellow);
            if (gui::collapse("Green", "", D(green)))
                D(green) = !D(green);
            if (D(green))
                hslSliders(ColorGrader::kHuesGreen);
            if (gui::collapse("Cyan", "", D(cyan)))
                D(cyan) = !D(cyan);
            if (D(cyan))
                hslSliders(ColorGrader::kHuesCyan);
            if (gui::collapse("Blue", "", D(blue)))
                D(blue) = !D(blue);
            if (D(blue))
                hslSliders(ColorGrader::kHuesBlue);
            if (gui::collapse("Magenta", "", D(magenta)))
                D(magenta) = !D(magenta);
            if (D(magenta))
                hslSliders(ColorGrader::kHuesMagenta);
        gui::dedent();
        gui::label("Brightness and contrast");
        gui::indent();
            float brightness = colorGrading.brightness();
            float contrast = colorGrading.contrast();
            if (gui::slider("Brightness", brightness, -1.0f, 1.0f, 0.0015f))
                colorGrading.setBrightness(brightness);
            if (gui::slider("Contrast", contrast, -1.0f, 1.0f, 0.0015f))
                colorGrading.setContrast(contrast);
        gui::dedent();
        gui::heading();
        if (gui::button("Reset"))
            colorGrading.reset();
    gui::areaFinish();
}

static void menuDeveloper() {
    const size_t w = neoWidth() / 3;
    const size_t h = neoHeight() / 2;
    const size_t x = neoWidth() / 2 - w / 2;
    const size_t y = neoHeight() / 2 - h / 2;

    auto &trilinear = c::Console::value<int>("r_trilinear");
    auto &bilinear = c::Console::value<int>("r_bilinear");
    auto &fog = c::Console::value<int>("r_fog");
    auto &spec = c::Console::value<int>("r_spec");
    auto &texcompcache = c::Console::value<int>("r_tex_compress_cache");
    auto &mipmaps = c::Console::value<int>("r_mipmaps");
    auto &fov = c::Console::value<float>("cl_fov");
    auto &nearp = c::Console::value<float>("cl_nearp");
    auto &farp = c::Console::value<float>("cl_farp");

    // Calculate texture compression cache contents
    gui::areaBegin("Developer", x, y, w, h, D(scroll));
        gui::heading();
        gui::indent();
            if (gui::check("Texture compression cache", texcompcache))
                texcompcache.toggle();
            if (gui::button("Clear texture cache")) {
                const u::string cachePath = neoUserPath() + "cache";
                for (const auto &it : u::dir(cachePath)) {
                    const u::string cacheFile = cachePath + u::kPathSep + it;
                    if (u::remove(cacheFile))
                        u::Log::out("[cache] => removed cache%c%s\n", u::kPathSep, it);
                }
                u::Log::out("[cache] => cleared\n");
            }
            if (gui::button("Reset configuration"))
                neoDeleteConfig();
            gui::label("Texture filtering");
            gui::indent();
                if (gui::check("Mipmaps", mipmaps.get()))
                    mipmaps.toggle();
                if (gui::check("Trilinear", trilinear.get(), mipmaps.get()))
                    trilinear.toggle();
                if (gui::check("Bilinear", bilinear.get()))
                    bilinear.toggle();
            gui::dedent();
            gui::label("World shading");
            gui::indent();
                if (gui::check("Fog", fog.get()))
                    fog.toggle();
                if (gui::check("Specularity", spec.get()))
                    spec.toggle();
            gui::dedent();
            gui::label("Clipping planes");
            gui::indent();
                gui::slider("Field of view", fov.get(), fov.min(), fov.max(), 0.01f);
                gui::slider("Near", nearp.get(), nearp.min(), nearp.max(), 0.01f);
                gui::slider("Far", farp.get(), farp.min(), farp.max(), 0.01f);
            gui::dedent();
        gui::dedent();
    gui::areaFinish();
}

static void menuOptions() {
    const size_t w = neoWidth() / 3;
    const size_t h = neoHeight() / 2;
    const size_t x = neoWidth() / 2 - w / 2;
    const size_t y = neoHeight() / 2 - h / 2;

    gui::areaBegin("Options", x, y, w, h, D(scroll));
        gui::heading();
        if (gui::collapse("Video", "", D(video)))
            D(video) = !D(video);
        if (D(video)) {
            gui::indent();
                auto &fullscreen = c::Console::value<int>("vid_fullscreen");
                if (gui::check("Fullscreen", fullscreen))
                    fullscreen.toggle();
                gui::label("Vsync");
                auto &vsync = c::Console::value<int>("vid_vsync");
                if (gui::check("Late swap tearing", vsync.get() == kSyncTear) && vsync.get() != kSyncTear)
                    vsync.set(kSyncTear);
                if (gui::check("Disabled", vsync.get() == kSyncNone) && vsync.get() != kSyncNone)
                    vsync.set(kSyncNone);
                if (gui::check("Enabled", vsync.get() == kSyncEnabled) && vsync.get() != kSyncEnabled)
                    vsync.set(kSyncEnabled);
                if (gui::check("Guess", vsync.get() == kSyncRefresh) && vsync.get() != kSyncRefresh)
                    vsync.set(kSyncRefresh);
                gui::label("Resolution");

                auto &width = c::Console::value<int>("vid_width");
                auto &height = c::Console::value<int>("vid_height");

                // Find matching resolution
                int findRatio = -1;
                int findResolution = -1;
                for (auto &it : kResolutions) {
                    for (auto &jt : it) {
                        if (jt.width == width.get() && jt.height == height.get()) {
                            findRatio = &it - kResolutions.begin();
                            findResolution = &jt - it.begin();
                            break;
                        }
                    }
                }

                if (findRatio != -1 && findResolution != -1) {
                    D(ratio) = findRatio;
                    D(resolution) = findResolution;

                    D(ratio) = gui::selector(nullptr, D(ratio), kAspectRatios);
                    D(resolution) = gui::selector(nullptr, D(resolution), kResolutionStrings[D(ratio)]);
                }

                auto resolution = kResolutions[D(ratio)][D(resolution)];
                width.set(resolution.width);
                height.set(resolution.height);
            gui::dedent();

        }
        if (gui::collapse("Graphics", "", D(graphics)))
            D(graphics) = !D(graphics);
        if (D(graphics)) {
            auto &aniso = c::Console::value<int>("r_aniso");
            auto &ssao = c::Console::value<int>("r_ssao");
            auto &fxaa = c::Console::value<int>("r_fxaa");
            auto &parallax = c::Console::value<int>("r_parallax");
            auto &texcomp = c::Console::value<int>("r_tex_compress");
            auto &texquality = c::Console::value<float>("r_tex_quality");
            gui::indent();
                gui::slider<int>("Anisotropic", aniso.get(), aniso.min(), aniso.max(), 1);
                if (gui::check("Ambient occlusion", ssao.get()))
                    ssao.toggle();
                if (gui::check("Anti-aliasing", fxaa.get()))
                    fxaa.toggle();
                if (gui::check("Parallax mapping", parallax.get()))
                    parallax.toggle();
                if (gui::check("Texture compression", texcomp.get()))
                    texcomp.toggle();
                gui::slider("Texture quality", texquality.get(), texquality.min(), texquality.max(), 0.01f);
            gui::dedent();
        }
        if (gui::collapse("Audio", "", D(audio)))
            D(audio) = !D(audio);
        if (D(audio)) {
            extern a::Audio *gAudio;

            auto &driver = c::Console::value<u::string>("snd_driver");
            auto &device = c::Console::value<u::string>("snd_device");

            const auto& audioDrivers = gAudio->drivers();
            // Copy the drivers for the selector line
            u::vector<u::string> drivers;
            for (const auto &it : audioDrivers)
                drivers.push_back(it.name);
            // Get the driver and device index for the current driver selected
            u::vector<u::string> devices;
            for (const auto &it : audioDrivers) {
                if (it.name != driver.get())
                    continue;
                D(driver) = &it - audioDrivers.begin();
                devices = it.devices;
                for (const auto &jt : devices) {
                    if (jt != device.get())
                        continue;
                    D(device) = &jt - devices.begin();
                    break;
                }
                break;
            }
            // Draw the options
            gui::indent();
                if (drivers.size()) {
                    gui::label("Driver");
                    D(driver) = gui::selector(nullptr, D(driver), drivers);
                }
                if (devices.size()) {
                    gui::label("Device");
                    D(device) = gui::selector(nullptr, D(device), devices);
                }
                driver.set(drivers[D(driver)]);
                device.set(devices[D(device)]);
            gui::dedent();
        }
        if (gui::collapse("Input", "", D(input)))
            D(input) = !D(input);
        if (D(input)) {
            gui::indent();
                auto &mouse_sens = c::Console::value<float>("cl_mouse_sens");
                auto &mouse_invert = c::Console::value<int>("cl_mouse_invert");
                gui::label("Mouse");
                if (gui::check("Invert", mouse_invert.get()))
                    mouse_invert.toggle();
                gui::slider("Sensitivity", mouse_sens.get(), mouse_sens.min(), mouse_sens.max(), 0.01f);
            gui::dedent();
        }
    gui::areaFinish();
}

static void menuCredits() {
    const size_t w = neoWidth() / 4;
    const size_t h = neoHeight() / 3;
    const size_t x = neoWidth() / 2 - w / 2;
    const size_t y = neoHeight() / 2 - h / 2;

    gui::areaBegin("Credits", x, y, w, h, D(scroll));
        gui::heading();
        if (gui::collapse("Engine", "", D(engine)))
            D(engine) = !D(engine);
        if (D(engine)) {
            gui::indent();
            for (auto &it : kCreditsEngine)
                gui::label(it);
            gui::dedent();
        }
        if (gui::collapse("Design", "", D(design)))
            D(design) = !D(design);
        if (D(design)) {
            gui::indent();
            for (auto &it : kCreditsDesign)
                gui::label(it);
            gui::dedent();
        }
        if (gui::collapse("Special Thanks", "", D(special)))
            D(special) = !D(special);
        if (D(special)) {
            gui::indent();
            for (auto &it : kCreditsSpecialThanks)
                gui::label(it);
            gui::dedent();
        }
    gui::areaFinish();
}

static void menuEdit() {
    // Menu against the right hand side
    size_t w = neoWidth() / 4;
    size_t h = neoHeight() - 50;
    size_t x = neoWidth() - w;
    size_t y = neoHeight() - h - 50/2;

    gui::areaBegin("Edit", x, y, w, h, D(scroll));
        gui::heading();
        // If there is something selected, render the GUI for it
        if (gSelected) {
            if (gSelected->type == entity::kMapModel) {
                auto &mm = gWorld.getMapModel(gSelected->index);
                gui::value("Model");
                gui::label("Scale");
                gui::indent();
                    gui::slider("X", mm.scale.x, 0.0f, 10.0f, 0.1f);
                    gui::slider("Y", D(lockScale) ? mm.scale.x : mm.scale.y, 0.0f, 10.0f, 0.1f);
                    gui::slider("Z", D(lockScale) ? mm.scale.x : mm.scale.z, 0.0f, 10.0f, 0.1f);
                    gui::separator();
                    if (gui::check("Lock", D(lockScale)))
                        D(lockScale) = !D(lockScale);
                    if (D(lockScale)) {
                        mm.scale.y = mm.scale.x;
                        mm.scale.z = mm.scale.x;
                    }
                gui::dedent();
                gui::label("Rotate");
                gui::indent();
                    gui::slider("X", mm.rotate.x, 0.0f, 360.0f, 0.1f);
                    gui::slider("Y", mm.rotate.y, 0.0f, 360.0f, 0.1f);
                    gui::slider("Z", mm.rotate.z, 0.0f, 360.0f, 0.1f);
                gui::dedent();
                gui::separator();
                if (gui::button("Delete")) {
                    gWorld.erase(gSelected->where);
                    gSelected = nullptr;
                }
            } else if (gSelected->type == entity::kPointLight) {
                auto &pl = gWorld.getPointLight(gSelected->index);
                int R = pl.color.x * 255.0f;
                int G = pl.color.y * 255.0f;
                int B = pl.color.z * 255.0f;
                gui::value("Point light");
                gui::label("Color");
                gui::indent();
                    gui::slider("Red", R, 0, 0xFF, 1);
                    gui::slider("Green", D(plightLock) ? R : G, 0, 0xFF, 1);
                    gui::slider("Blue", D(plightLock) ? R : B, 0, 0xFF, 1);
                    gui::separator();
                    if (gui::check("Lock", D(plightLock)))
                        D(plightLock) = !D(plightLock);
                gui::dedent();
                gui::label("Term");
                gui::indent();
                    gui::slider("Ambient", pl.ambient, 0.0f, 1.0f, 0.1f);
                    gui::slider("Diffuse", pl.diffuse, 0.0f, 1.0f, 0.1f);
                gui::dedent();
                gui::separator();
                gui::slider("Radius", pl.radius, 1.0f, 1024.0f, 1.0f);
                pl.color = {
                    R / 255.0f,
                    (D(plightLock) ? R : G) / 255.0f,
                    (D(plightLock) ? R : B) / 255.0f
                };
                if (gui::check("Shadows", pl.castShadows))
                    pl.castShadows = !pl.castShadows;
                gui::separator();
                if (gui::button("Delete")) {
                    gWorld.erase(gSelected->where);
                    gSelected = nullptr;
                }
            } else if (gSelected->type == entity::kSpotLight) {
                auto &sl = gWorld.getSpotLight(gSelected->index);
                int R = sl.color.x * 255.0f;
                int G = sl.color.y * 255.0f;
                int B = sl.color.z * 255.0f;
                gui::value("Spot light");
                gui::label("Color");
                gui::indent();
                    gui::slider("Red", R, 0, 0xFF, 1);
                    gui::slider("Green", D(slightLock) ? R : G, 0, 0xFF, 1);
                    gui::slider("Blue", D(slightLock) ? R : B, 0, 0xFF, 1);
                    gui::separator();
                    if (gui::check("Lock", D(slightLock)))
                        D(slightLock) = !D(slightLock);
                gui::dedent();
                gui::label("Term");
                gui::indent();
                    gui::slider("Ambient", sl.ambient, 0.0f, 1.0f, 0.1f);
                    gui::slider("Diffuse", sl.diffuse, 0.0f, 1.0f, 0.1f);
                gui::dedent();
                gui::label("Direction");
                gui::indent();
                    gui::slider("X", sl.direction.x, 0.0f, 360.0f, 1.0f);
                    gui::slider("Y", sl.direction.y, 0.0f, 360.0f, 1.0f);
                    gui::slider("Z", sl.direction.z, 0.0f, 360.0f, 1.0f);
                gui::dedent();
                gui::separator();
                gui::slider("Radius", sl.radius, 1.0f, 1024.0f, 1.0f);
                gui::slider("Cutoff", sl.cutOff, 1.0f, 90.0f, 1.0f);
                if (gui::check("Shadows", sl.castShadows))
                    sl.castShadows = !sl.castShadows;
                sl.color = {
                    R / 255.0f,
                    (D(slightLock) ? R : G) / 255.0f,
                    (D(slightLock) ? R : B) / 255.0f
                };
                gui::separator();
                if (gui::button("Delete")) {
                    gWorld.erase(gSelected->where);
                    gSelected = nullptr;
                }
            }
        } else {
            if (gui::collapse("Ambient light", "", D(dlight)))
                D(dlight) = !D(dlight);
            if (D(dlight)) {
                gui::indent();
                    auto &ambient = c::Console::value<float>("map_dlight_ambient");
                    auto &diffuse = c::Console::value<float>("map_dlight_diffuse");
                    auto &color = c::Console::value<int>("map_dlight_color");
                    auto &x = c::Console::value<float>("map_dlight_directionx");
                    auto &y = c::Console::value<float>("map_dlight_directiony");
                    auto &z = c::Console::value<float>("map_dlight_directionz");
                    int R = (color.get() >> 16) & 0xFF;
                    int G = (color.get() >> 8) & 0xFF;
                    int B = color.get() & 0xFF;
                    gui::slider("Ambient", ambient.get(), ambient.min(), ambient.max(), 0.01f);
                    gui::slider("Diffuse", diffuse.get(), diffuse.min(), diffuse.max(), 0.01f);
                    gui::label("Color");
                    gui::indent();
                        gui::slider("Red", R, 0, 0xFF, 1);
                        gui::slider("Green", D(dlightLock) ? R : G, 0, 0xFF, 1);
                        gui::slider("Blue", D(dlightLock) ? R : B, 0, 0xFF, 1);
                        gui::separator();
                        if (gui::check("Lock", D(dlightLock)))
                            D(dlightLock) = !D(dlightLock);
                    gui::dedent();
                    gui::label("Direction");
                    gui::indent();
                        gui::slider("X", x.get(), x.min(), x.max(), 0.001f);
                        gui::slider("Y", y.get(), y.min(), y.max(), 0.001f);
                        gui::slider("Z", z.get(), z.min(), z.max(), 0.001f);
                    gui::dedent();
                    // Set the color again
                    color.set((R << 16) | ((D(dlightLock) ? R : G) << 8) | (D(dlightLock) ? R : B));
                gui::dedent();
            }
            if (gui::collapse("Fog", "", D(fog)))
                D(fog) = !D(fog);
            if (D(fog)) {
                gui::indent();
                    auto &equation = c::Console::value<int>("map_fog_equation");
                    auto &density = c::Console::value<float>("map_fog_density");
                    auto &color = c::Console::value<int>("map_fog_color");
                    int R = (color.get() >> 16) & 0xFF;
                    int G = (color.get() >> 8) & 0xFF;
                    int B = color.get() & 0xFF;
                    gui::label("Equation");
                    static const int kEquations[] = { r::fog::kLinear, r::fog::kExp, r::fog::kExp2 };
                    static const u::vector<const char *> kFogs = { "Linear", "Exp", "Exp2" };
                    D(fogSelect) = gui::selector(nullptr, D(fogSelect), kFogs);
                    equation.set(kEquations[D(fogSelect)]);
                    if (equation.get() == r::fog::kLinear) {
                        auto &start = c::Console::value<float>("map_fog_range_start");
                        auto &end = c::Console::value<float>("map_fog_range_end");
                        gui::label("Range");
                        gui::indent();
                        gui::slider("Start", start.get(), start.min(), start.max(), 0.001f);
                        gui::slider("End", end.get(), end.min(), end.max(), 0.001f);
                        gui::dedent();
                    }
                    gui::slider("Density", density.get(), density.min(), density.max(), 0.001f);
                    gui::label("Color");
                    gui::indent();
                        gui::slider("Red", R, 0, 0xFF, 1);
                        gui::slider("Green", D(fogLightLock) ? R : G, 0, 0xFF, 1);
                        gui::slider("Blue", D(fogLightLock) ? R : B, 0, 0xFF, 1);
                        gui::separator();
                        if (gui::check("Lock", D(fogLightLock)))
                            D(fogLightLock) = !D(fogLightLock);
                    gui::dedent();
                gui::dedent();
                color.set((R << 16) | ((D(fogLightLock) ? R : G) << 8) | (D(fogLightLock) ? R : B));
            }
            if (gui::collapse("New", "", D(newent)))
                D(newent) = !D(newent);
            if (D(newent)) {
                gui::indent();
                    if (gui::item("Model"))
                        D(model) = true;
                    else if (gui::item("Point light")) {
                        r::pointLight pl;
                        pl.position = looking();
                        pl.ambient = 0.5f;
                        pl.diffuse = 0.5f;
                        pl.radius = 30;
                        pl.color = randomColor();
                        gSelected = gWorld.insert(pl);
                    } else if (gui::item("Spot light")) {
                        r::spotLight sl;
                        sl.position = looking();
                        sl.ambient = 0.5f;
                        sl.diffuse = 0.5f;
                        gClient.getDirection(&sl.direction, nullptr, nullptr);
                        sl.radius = 30;
                        sl.color = randomColor();
                        gSelected = gWorld.insert(sl);
                    } else if (gui::item("Playerstart")) {
                        playerStart ps;
                        ps.position = looking();
                        gClient.getDirection(&ps.direction, nullptr, nullptr);
                        gSelected = gWorld.insert(ps);
                    } else if (gui::item("Jumppad")) {
                        jumppad jp;
                        jp.position = looking();
                        gClient.getDirection(nullptr, &jp.direction, nullptr);
                        gSelected = gWorld.insert(jp);
                    } else if (gui::item("Teleport")) {
                        teleport tp;
                        tp.position = looking();
                        gClient.getDirection(&tp.direction, nullptr, nullptr);
                        gSelected = gWorld.insert(tp);
                    }
                gui::dedent();
            }
        }
    gui::areaFinish();

    // Centered menu for new entities
    w = neoWidth() / 4;
    h = neoHeight() / 3;
    x = neoWidth() / 2 - w / 2;
    y = neoHeight() / 2 - h / 2;
    if (D(model)) {
        // Find all the model by name
        u::set<u::string> models;
        for (auto &it : gWorld.getMapModels())
            models.insert(it->name);
        if (models.size()) {
            gui::areaBegin("Mapmodels", x, y, w, h, D(modelScroll));
            for (auto &it : models) {
                if (gui::item(it.c_str())) {
                    mapModel m;
                    m.name = it;
                    m.position = looking();
                    gSelected = gWorld.insert(m);
                    D(model) = false;
                }
            }
            gui::areaFinish();
        }
    }
}

static void menuConsole() {
    const size_t w = neoWidth();
    const size_t h = neoHeight() / 5;
    const size_t x = neoWidth() / 2 - w / 2;
    const size_t y = neoHeight() - h;

    gui::areaBegin("", x, y, w, h, D(scroll), false);
    for (auto &it : gMenuConsole)
        gui::label(it.c_str());
    gui::areaFinish(30, true);
}

static void menuCreate() {
    const size_t w = neoWidth() / 4;
    const size_t h = neoHeight() / 3;
    const size_t x = neoWidth() / 2 - w / 2;
    const size_t y = neoHeight() / 2 - h / 2;

    if (D(browse)) {
        if (gMenuPaths.empty())
            STR(directory) = neoUserPath();
        gui::areaBegin(STR(directory).c_str(), x, y, w, h, D(browseScroll));
            // When it isn't the user path we need a way to go back
            if (STR(directory) != neoUserPath() && gui::item("..")) {
                auto &&get = u::move(gMenuPaths.back());
                gMenuPaths.pop_back();
                // Prevent against the situation where the user is being evil
                if (u::exists(get, u::kDirectory))
                    STR(directory) = get;
                else
                    STR(directory) = neoUserPath();
            }
            for (const auto &what : u::dir(STR(directory))) {
                if (u::dir::isFile(u::format("%s%c%s", STR(directory), u::kPathSep, what))) {
                    if (gui::item(what))
                        D(browse) = false;
                } else {
                    if (gui::item(u::format("%s%c", what, u::kPathSep).c_str())) {
                        // Clicked a directory
                        gMenuPaths.push_back(STR(directory));
                        STR(directory) = u::format("%s%s%c", STR(directory), what, u::kPathSep);
                    }
                }
            }
        gui::areaFinish();
    } else {
        gui::areaBegin("New map", x, y, w, h, D(createScroll));
            if (STR(mesh).empty()) {
                if (gui::button("Load model"))
                    D(browse) = true;
            } else {
                gui::label(FMT(20, STR(mesh)));
            }
            if (STR(skybox).empty()) {
                if (gui::button("Load skybox", !STR(mesh).empty()))
                    D(browse) = true;
            } else {
                gui::label(FMT(20, STR(skybox)));
            }
        gui::areaFinish();
    }
}

void menuReset() {
    gMenuData["menuCredits_engine"] = true;
    gMenuData["menuCredits_design"] = true;
    gMenuData["menuCredits_special"] = true;

    gMenuData["menuEdit_dlight"] = true;
    gMenuData["menuEdit_fog"] = true;
    gMenuData["menuEdit_newent"] = false;
    gMenuData["menuEdit_light"] = true;

    gMenuData["menuCreate_browse"] = false;

    gMenuStrings["menuCreate_mesh"] = "";
    gMenuStrings["menuCreate_skybox"] = "";

    gMenuStrings["menuCreate_directory"] = neoUserPath();
    gMenuPaths.destroy();
}

void menuUpdate() {
    if (gMenuState & kMenuMain)
        menuMain();
    if (gMenuState & kMenuOptions)
        menuOptions();
    if (gMenuState & kMenuCredits)
        menuCredits();
    if (gMenuState & kMenuConsole)
        menuConsole();
    if (gMenuState & kMenuEdit)
        menuEdit();
    if (gMenuState & kMenuCreate)
        menuCreate();
    if (gMenuState & kMenuColorGrading)
        menuColorGrading();
    if (gMenuState & kMenuDeveloper)
        menuDeveloper();
}
