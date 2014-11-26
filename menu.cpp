#include "engine.h"
#include "menu.h"
#include "gui.h"
#include "c_var.h"

int gMenuState = kMenuMain | kMenuConsole;
u::map<u::string, int*> gMenuReferences;
static u::map<u::string, int> gMenuData;

#define D(X) gMenuData[u::format("%s_%s", __func__, #X)]
#define R(X) *gMenuReferences[#X]

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
    const size_t h = neoHeight() / 5;
    const size_t x = neoWidth() / 2 - w / 2;
    const size_t y = neoHeight() / 2 - h / 2;

    gui::areaBegin("Main", x, y, w, h, D(scroll));
        gui::heading();
        if (gui::button("Play")) {
            R(playing) = true;
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
            R(running) = false;
        }
    gui::areaFinish();
}

static void menuOptions() {
    const size_t w = neoWidth() / 4;
    const size_t h = neoHeight() / 2;
    const size_t x = neoWidth() / 2 - w / 2;
    const size_t y = neoHeight() / 2 - h / 2;

    gui::areaBegin("Options", x, y, w, h, D(scroll));
        gui::heading();
        if (gui::collapse("Video", "", D(video)))
            D(video) = !D(video);
        if (D(video)) {
            gui::indent();
                auto &fullscreen = c::varGet<int>("vid_fullscreen");
                if (gui::check("Fullscreen", fullscreen))
                    fullscreen.toggle();
                gui::label("Vsync");
                auto &vsync = c::varGet<int>("vid_vsync");
                if (gui::check("Late swap tearing", vsync.get() == kSyncTear) && vsync.get() != kSyncTear)
                    vsync.set(kSyncTear);
                if (gui::check("Disabled", vsync.get() == kSyncNone) && vsync.get() != kSyncNone)
                    vsync.set(kSyncNone);
                if (gui::check("Enabled", vsync.get() == kSyncEnabled) && vsync.get() != kSyncEnabled)
                    vsync.set(kSyncEnabled);
                if (gui::check("Guess", vsync.get() == kSyncRefresh) && vsync.get() != kSyncRefresh)
                    vsync.set(kSyncRefresh);
                gui::label("Resolution");
                auto &width = c::varGet<int>("vid_width");
                auto &height = c::varGet<int>("vid_height");
                gui::slider("Width", width.get(), width.min(), width.max(), 1);
                gui::slider("Height", height.get(), height.min(), height.max(), 1);
            gui::dedent();

        }
        if (gui::collapse("Graphics", "", D(graphics)))
            D(graphics) = !D(graphics);
        if (D(graphics)) {
            auto &aniso = c::varGet<int>("r_aniso");
            auto &trilinear = c::varGet<int>("r_trilinear");
            auto &bilinear = c::varGet<int>("r_bilinear");
            auto &mipmaps = c::varGet<int>("r_mipmaps");
            auto &ssao = c::varGet<int>("r_ssao");
            auto &fxaa = c::varGet<int>("r_fxaa");
            auto &parallax = c::varGet<int>("r_parallax");
            auto &texcomp = c::varGet<int>("r_texcomp");
            auto &texcompcache = c::varGet<int>("r_texcompcache");
            auto &texquality = c::varGet<float>("r_texquality");
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
                auto &mouse_sens = c::varGet<float>("cl_mouse_sens");
                auto &mouse_invert = c::varGet<int>("cl_mouse_invert");
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
                auto &fov = c::varGet<float>("cl_fov");
                auto &nearp = c::varGet<float>("cl_nearp");
                auto &farp = c::varGet<float>("cl_farp");
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

static void menuConsole() {
    const size_t w = neoWidth() - (neoWidth() / 32);
    const size_t h = neoHeight() / 5;
    const size_t x = neoWidth() / 2 - w / 2;
    const size_t y = neoHeight() - h;

    gui::areaBegin("Console", x, y, w, h, D(scroll));
        gui::indent();
        for (size_t i = 0; i < 100; i++)
            gui::label("Never going to give you up!");
        gui::dedent();
    gui::areaFinish(25, true);
}

void menuRegister(const u::string &name, int &ref) {
    if (gMenuReferences.find(name) == gMenuReferences.end())
        gMenuReferences[name] = &ref;
}

void menuRegister(const u::string &name, bool &ref) {
    menuRegister(name, (int &)ref);
}

void menuReset() {
    gMenuData["menuCredits_engine"] = true;
    gMenuData["menuCredits_design"] = true;
    gMenuData["menuCredits_special"] = true;
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
}
