#include "engine.h"
#include "world.h"
#include "client.h"
#include "menu.h"
#include "gui.h"
#include "cvar.h"
#include "grader.h"

#include "u_set.h"
#include "u_misc.h"
#include "u_file.h"

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

#define PP_COUNT(X) (sizeof(X)/sizeof(*X))

static const char *kCreditsEngine[] = {
    "Dale 'graphitemaster' Weiler"
};

static const char *kCreditsDesign[] = {
    "Maxim 'acerspyro' Therrien"
};

static const char *kCreditsSpecialThanks[] = {
    "Lee 'eihrul' Salzman",
    "Wolfgang 'Blub\\w' Bullimer",
    "Forest 'LordHavoc' Hale"
};

#define RATIOS \
    static const u::initializer_list<const char *> kAspectRatios =
#include "res.h"
#define _(X, Y) { X, Y }
#define RESOLUTIONS \
    static const u::initializer_list<u::initializer_list<u::pair<int, int>>> kResolutionPairs =
#include "res.h"
#define _(X, Y) #X"x"#Y
#define RESOLUTIONS \
    static const u::initializer_list<u::initializer_list<const char *>> kResolutionStrings =
#include "res.h"

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

    auto &colorGrading = gWorld.getColorGrader();

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
                cmySliders(colorGrader::kBalanceShadows);
            if (gui::collapse("Midtones", "", D(midtones)))
                D(midtones) = !D(midtones);
            if (D(midtones))
                cmySliders(colorGrader::kBalanceMidtones);
            if (gui::collapse("Highlights", "", D(highlights)))
                D(highlights) = !D(highlights);
            if (D(highlights))
                cmySliders(colorGrader::kBalanceHighlights);
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
                hslSliders(colorGrader::kHuesRed);
            if (gui::collapse("Yellow", "", D(yellow)))
                D(yellow) = !D(yellow);
            if (D(yellow))
                hslSliders(colorGrader::kHuesYellow);
            if (gui::collapse("Green", "", D(green)))
                D(green) = !D(green);
            if (D(green))
                hslSliders(colorGrader::kHuesGreen);
            if (gui::collapse("Cyan", "", D(cyan)))
                D(cyan) = !D(cyan);
            if (D(cyan))
                hslSliders(colorGrader::kHuesCyan);
            if (gui::collapse("Blue", "", D(blue)))
                D(blue) = !D(blue);
            if (D(blue))
                hslSliders(colorGrader::kHuesBlue);
            if (gui::collapse("Magenta", "", D(magenta)))
                D(magenta) = !D(magenta);
            if (D(magenta))
                hslSliders(colorGrader::kHuesMagenta);
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

    auto &trilinear = varGet<int>("r_trilinear");
    auto &bilinear = varGet<int>("r_bilinear");
    auto &fog = varGet<int>("r_fog");
    auto &spec = varGet<int>("r_spec");
    auto &texcompcache = varGet<int>("r_tex_compress_cache");
    auto &texcompcachezlib = varGet<int>("r_tex_compress_cache_zlib");
    auto &hoq = varGet<int>("r_hoq");
    auto &maxhoq = varGet<int>("r_maxhoq");
    auto &mipmaps = varGet<int>("r_mipmaps");
    auto &fov = varGet<float>("cl_fov");
    auto &nearp = varGet<float>("cl_nearp");
    auto &farp = varGet<float>("cl_farp");

    // Calculate texture compression cache contents
    gui::areaBegin("Developer", x, y, w, h, D(scroll));
        gui::heading();
        gui::indent();
            if (gui::check("Texture compression cache", texcompcache))
                texcompcache.toggle();
            if (gui::check("Texture compression cache (on disk compression)", texcompcachezlib))
                texcompcachezlib.toggle();
            if (gui::button("Clear texture cache")) {
                const u::string cachePath = neoUserPath() + "cache";
                for (const auto &it : u::dir(cachePath)) {
                    const u::string cacheFile = cachePath + u::kPathSep + it;
                    if (u::remove(cacheFile))
                        u::print("[cache] => removed cache%c%.50s...\n", u::kPathSep, it);
                }
                u::print("[cache] => cleared\n");
            }
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
            gui::label("Occlusion queries");
            gui::indent();
                if (gui::check("Hardware occlusion queries", hoq.get()))
                    hoq.toggle();
                gui::slider("Maximum occlusion queries", maxhoq.get(), maxhoq.min(), maxhoq.max(), 1, hoq.get());
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
                auto &fullscreen = varGet<int>("vid_fullscreen");
                if (gui::check("Fullscreen", fullscreen))
                    fullscreen.toggle();
                gui::label("Vsync");
                auto &vsync = varGet<int>("vid_vsync");
                if (gui::check("Late swap tearing", vsync.get() == kSyncTear) && vsync.get() != kSyncTear)
                    vsync.set(kSyncTear);
                if (gui::check("Disabled", vsync.get() == kSyncNone) && vsync.get() != kSyncNone)
                    vsync.set(kSyncNone);
                if (gui::check("Enabled", vsync.get() == kSyncEnabled) && vsync.get() != kSyncEnabled)
                    vsync.set(kSyncEnabled);
                if (gui::check("Guess", vsync.get() == kSyncRefresh) && vsync.get() != kSyncRefresh)
                    vsync.set(kSyncRefresh);
                gui::label("Resolution");

                auto &width = varGet<int>("vid_width");
                auto &height = varGet<int>("vid_height");

                // Find matching resolution
                int findRatio = -1;
                int findResolution = -1;
                for (auto &it : kResolutionPairs) {
                    for (auto &jt : it) {
                        if (jt.first == width.get() && jt.second == height.get()) {
                            findRatio = &it - kResolutionPairs.begin();
                            findResolution = &jt - it.begin();
                            break;
                        }
                    }
                }

                if (findRatio != -1 && findResolution != -1) {
                    D(ratio) = findRatio;
                    D(resolution) = findResolution;
                }

                D(ratio) = gui::selector(nullptr, D(ratio), kAspectRatios);
                D(resolution) = gui::selector(nullptr, D(resolution), kResolutionStrings[D(ratio)]);

                auto resolution = kResolutionPairs[D(ratio)][D(resolution)];
                width.set(resolution.first);
                height.set(resolution.second);
            gui::dedent();

        }
        if (gui::collapse("Graphics", "", D(graphics)))
            D(graphics) = !D(graphics);
        if (D(graphics)) {
            auto &aniso = varGet<int>("r_aniso");
            auto &ssao = varGet<int>("r_ssao");
            auto &fxaa = varGet<int>("r_fxaa");
            auto &parallax = varGet<int>("r_parallax");
            auto &texcomp = varGet<int>("r_tex_compress");
            auto &texquality = varGet<float>("r_texquality");
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
        if (gui::collapse("Input", "", D(input)))
            D(input) = !D(input);
        if (D(input)) {
            gui::indent();
                auto &mouse_sens = varGet<float>("cl_mouse_sens");
                auto &mouse_invert = varGet<int>("cl_mouse_invert");
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
                    gui::slider("Yaw", sl.direction.x, 0.0f, 360.0f, 1.0f);
                    gui::slider("Pitch", sl.direction.y, 0.0f, 360.0f, 1.0f);
                gui::dedent();
                gui::separator();
                gui::slider("Radius", sl.radius, 1.0f, 1024.0f, 1.0f);
                gui::slider("Cutoff", sl.cutOff, 1.0f, 90.0f, 1.0f);
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
                    auto &ambient = varGet<float>("map_dlight_ambient");
                    auto &diffuse = varGet<float>("map_dlight_diffuse");
                    auto &color = varGet<int>("map_dlight_color");
                    auto &x = varGet<float>("map_dlight_directionx");
                    auto &y = varGet<float>("map_dlight_directiony");
                    auto &z = varGet<float>("map_dlight_directionz");
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
                    auto &equation = varGet<int>("map_fog_equation");
                    auto &density = varGet<float>("map_fog_density");
                    auto &color = varGet<int>("map_fog_color");
                    int R = (color.get() >> 16) & 0xFF;
                    int G = (color.get() >> 8) & 0xFF;
                    int B = color.get() & 0xFF;
                    gui::label("Equation");
                    static const int kEquations[] = { fog::kLinear, fog::kExp, fog::kExp2 };
                    D(fogSelect) = gui::selector(nullptr, D(fogSelect), { "Linear", "Exp", "Exp2" });
                    equation.set(kEquations[D(fogSelect)]);
                    if (equation.get() == fog::kLinear) {
                        auto &start = varGet<float>("map_fog_range_start");
                        auto &end = varGet<float>("map_fog_range_end");
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
                        pointLight pl;
                        pl.position = looking();
                        pl.ambient = 0.5f;
                        pl.diffuse = 0.5f;
                        pl.radius = 30;
                        pl.color = randomColor();
                        gSelected = gWorld.insert(pl);
                    } else if (gui::item("Spot light")) {
                        spotLight sl;
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
