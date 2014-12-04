#include "engine.h"
#include "world.h"
#include "menu.h"
#include "gui.h"
#include "cvar.h"

extern bool gPlaying;
extern bool gRunning;
extern world::descriptor *gSelected;
extern world gWorld;

int gMenuState = kMenuMain | kMenuConsole; // Default state

u::stack<u::string, kMenuConsoleHistorySize> gMenuConsole; // The console text buffer

static u::map<u::string, int> gMenuData;


#define D(X) gMenuData[u::format("%s_%s", __func__, #X)]

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

static void menuMain() {
    const size_t w = neoWidth() / 8;
    const size_t h = neoHeight() / 4.5;
    const size_t x = neoWidth() / 2 - w / 2;
    const size_t y = neoHeight() / 2 - h / 2;

    gui::areaBegin("Main", x, y, w, h, D(scroll));
        gui::heading();
        if (gui::button("Play")) {
            gPlaying = true;
            gMenuState &= ~kMenuMain;
            neoRelativeMouse(true);
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
                gui::slider("Width", width.get(), width.min(), width.max(), 1);
                gui::slider("Height", height.get(), height.min(), height.max(), 1);
            gui::dedent();

        }
        if (gui::collapse("Graphics", "", D(graphics)))
            D(graphics) = !D(graphics);
        if (D(graphics)) {
            auto &aniso = varGet<int>("r_aniso");
            auto &trilinear = varGet<int>("r_trilinear");
            auto &bilinear = varGet<int>("r_bilinear");
            auto &mipmaps = varGet<int>("r_mipmaps");
            auto &ssao = varGet<int>("r_ssao");
            auto &fxaa = varGet<int>("r_fxaa");
            auto &parallax = varGet<int>("r_parallax");
            auto &texcomp = varGet<int>("r_texcomp");
            auto &texcompcache = varGet<int>("r_texcompcache");
            auto &texquality = varGet<float>("r_texquality");
            gui::indent();
                if (gui::collapse("Texture filtering", "", D(filtering)))
                    D(filtering) = !D(filtering);
                if (D(filtering)) {
                    gui::indent();
                        if (gui::check("Anisotropic", aniso.get()))
                            aniso.toggle();
                        if (gui::check("Trilinear", trilinear.get()))
                            trilinear.toggle();
                        if (gui::check("Bilinear", bilinear.get()))
                            bilinear.toggle();
                    gui::dedent();
                }
                if (gui::check("Mipmaps", mipmaps.get()))
                    mipmaps.toggle();
                if (gui::check("Ambient occlusion", ssao.get()))
                    ssao.toggle();
                if (gui::check("Anti-aliasing", fxaa.get()))
                    fxaa.toggle();
                if (gui::check("Parallax mapping", parallax.get()))
                    parallax.toggle();
                if (gui::check("Texture compression", texcomp.get()))
                    texcomp.toggle();
                if (gui::check("Texture compression cache", texcompcache))
                    texcompcache.toggle();
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
        if (gui::collapse("Game", "", D(game)))
            D(game) = !D(game);
        if (D(game)) {
            gui::indent();
                auto &fov = varGet<float>("cl_fov");
                auto &nearp = varGet<float>("cl_nearp");
                auto &farp = varGet<float>("cl_farp");
                gui::label("Distance");
                gui::indent();
                    gui::slider("Field of view", fov.get(), fov.min(), fov.max(), 0.01f);
                    gui::slider("Near", nearp.get(), nearp.min(), nearp.max(), 0.01f);
                    gui::slider("Far", farp.get(), farp.min(), farp.max(), 0.01f);
                gui::dedent();
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
    const size_t w = neoWidth() / 4;
    const size_t h = neoHeight() - 50;
    const size_t x = neoWidth() - w;
    const size_t y = neoHeight() - h - 50/2;

    gui::areaBegin("Edit", x, y, w, h, D(scroll));
        gui::heading();
        // If there is something selected, render the GUI for it
        if (gSelected) {
            if (gSelected->type == entity::kMapModel) {
                auto &mm = gWorld.getMapModel(gSelected->index);
                gui::value("Mapmodel");
                gui::label("Scale");
                gui::indent();
                    gui::slider("X", mm.scale.x, 0.0f, 10.0f, 0.1f);
                    gui::slider("Y", mm.scale.y, 0.0f, 10.0f, 0.1f);
                    gui::slider("Z", mm.scale.z, 0.0f, 10.0f, 0.1f);
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
                gui::value("Pointlight");
                gui::label("Color");
                gui::indent();
                    gui::slider("Red", R, 0, 0xFF, 1);
                    gui::slider("Green", G, 0, 0xFF, 1);
                    gui::slider("Blue", B, 0, 0xFF, 1);
                gui::dedent();
                gui::label("Term");
                gui::indent();
                    gui::slider("Ambient", pl.ambient, 0.0f, 1.0f, 0.1f);
                    gui::slider("Diffuse", pl.diffuse, 0.0f, 1.0f, 0.1f);
                gui::dedent();
                gui::separator();
                gui::slider("Radius", pl.radius, 1.0f, 1024.0f, 1.0f);
                pl.color = { R / 255.0f, G / 255.0f, B / 255.0f };
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
                        gui::slider("Green", G, 0, 0xFF, 1);
                        gui::slider("Blue", B, 0, 0xFF, 1);
                    gui::dedent();
                    gui::label("Direction");
                    gui::indent();
                        gui::slider("X", x.get(), x.min(), x.max(), 0.001f);
                        gui::slider("Y", y.get(), y.min(), y.max(), 0.001f);
                        gui::slider("Z", z.get(), z.min(), z.max(), 0.001f);
                    gui::dedent();
                    // Set the color again
                    color.set((R << 16) | (G << 8) | B);
                gui::dedent();
            }
        }
    gui::areaFinish();
}

static void menuConsole() {
    const size_t w = neoWidth();
    const size_t h = neoHeight() / 5;
    const size_t x = neoWidth() / 2 - w / 2;
    const size_t y = neoHeight() - h;

    gui::areaBegin("", x, y, w, h, D(scroll), false);
    for (auto &it : gMenuConsole)
        gui::label(it);
    gui::areaFinish(30, true);
}

void menuReset() {
    gMenuData["menuCredits_engine"] = true;
    gMenuData["menuCredits_design"] = true;
    gMenuData["menuCredits_special"] = true;

    gMenuData["menuEdit_dlight"] = true;
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
}
