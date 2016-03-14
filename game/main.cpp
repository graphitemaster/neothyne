#include "engine.h"
#include "gui.h"
#include "world.h"
#include "client.h"
#include "menu.h"
#include "cvar.h"
#include "edit.h"

#include "r_pipeline.h"
#include "r_gui.h"

#include "u_file.h"
#include "u_misc.h"

// Game globals
bool gRunning = true;
bool gPlaying = false;
world::descriptor *gSelected = nullptr; // Selected world entity
client gClient;
world gWorld;
r::pipeline gPipeline;
m::perspective gPerspective;

VAR(float, cl_fov, "field of view", 45.0f, 270.0f, 90.0f);
VAR(float, cl_nearp, "near plane", 0.0f, 10.0f, 0.1f);
VAR(float, cl_farp, "far plane", 128.0f, 4096.0f, 2048.0f);

static void setBinds() {
    neoBindSet("MouseDnL", []() {
        if (varGet<int>("cl_edit").get() && (gMenuState == 0 || gMenuState == kMenuConsole))
            edit::select();
    });

    neoBindSet("EscapeDn", []() {
        if (gPlaying && (gMenuState & kMenuMain)) {
            if (gMenuState & kMenuConsole) {
                gMenuState = kMenuConsole;
            } else {
                gMenuState &= ~kMenuMain;
            }
        } else if (!(gMenuState & kMenuEdit) && !(gMenuState & kMenuColorGrading)) {
            // If the console is opened leave it open
            if (!(gMenuState & kMenuMain)) {
                neoRelativeMouse(false);
                neoCenterMouse();
            }
            gMenuState = (gMenuState & kMenuConsole)
                ? kMenuMain | kMenuConsole
                : kMenuMain;
        } else {
            gMenuState &= ~kMenuEdit;
            gMenuState &= ~kMenuColorGrading;
            gMenuState &= ~kMenuDeveloper;
        }
        neoRelativeMouse(gPlaying && (gMenuState & ~kMenuConsole) == 0);
        menuReset();
    });

    neoBindSet("F8Dn", []() {
        neoScreenShot();
    });

    neoBindSet("F9Dn", []() {
        neoRelativeMouse(false);
        neoCenterMouse();
        gMenuState ^= kMenuDeveloper;
    });

    neoBindSet("F10Dn", []() {
        if (varGet<int>("cl_edit").get())
            gMenuState ^= kMenuColorGrading;
        neoRelativeMouse(!((gMenuState & kMenuEdit) || (gMenuState & kMenuColorGrading)));
    });

    neoBindSet("F11Dn", []() {
        gMenuState ^= kMenuConsole;
    });

    neoBindSet("F12Dn", []() {
        if (varGet<int>("cl_edit").get())
            gMenuState ^= kMenuEdit;
        neoRelativeMouse(!(gMenuState & kMenuEdit));
    });

    neoBindSet("EDn", []() {
        if (gPlaying) {
            varGet<int>("cl_edit").toggle();
            gMenuState &= ~kMenuEdit;
            neoRelativeMouse(!(gMenuState & kMenuEdit));
        }
        edit::deselect();
    });

    neoBindSet("DeleteDn", []() {
        edit::remove();
    });
}

NVAR(float, u_x, "", -180.0f, 360.0f, 0.0f);
NVAR(float, u_y, "", -180.0f, 360.0f, 0.0f);
NVAR(float, u_z, "", -180.0f, 360.0f, 0.0f);

int neoMain(frameTimer &timer, int, char **, bool &shutdown) {
    // Setup rendering pipeline
    gPerspective.fov = cl_fov;
    gPerspective.nearp = cl_nearp;
    gPerspective.farp = cl_farp;
    gPerspective.width = neoWidth();
    gPerspective.height = neoHeight();

    gPipeline.setWorld(m::vec3::origin);

    // Clear the screen as soon as possible
    gl::ClearColor(40/255.0f, 30/255.0f, 50/255.0f, 0.1f);
    gl::Clear(GL_COLOR_BUFFER_BIT);
    neoSwap();

    setBinds();

    r::gui gGui;
    if (!gGui.load("fonts/droidsans"))
        neoFatal("failed to load font");
    if (!gGui.upload())
        neoFatal("failed to initialize GUI rendering method\n");

    // Setup window and menu
    menuReset();
    neoSetWindowTitle("Neothyne");
    neoCenterMouse();

#if 1
    // Setup some lights
    const m::vec3 places[] = {
        m::vec3(153.04, 105.02, 197.67),
        m::vec3(-64.14, 105.02, 328.36),
        m::vec3(-279.83, 105.02, 204.61),
        m::vec3(-458.72, 101.02, 189.58),
        m::vec3(-664.53, 75.02, -1.75),
        m::vec3(-580.69, 68.02, -184.89),
        m::vec3(-104.43, 84.02, -292.99),
        m::vec3(-23.59, 84.02, -292.40),
        m::vec3(333.00, 101.02, 194.46),
        m::vec3(167.13, 101.02, 0.32),
        m::vec3(-63.36, 37.20, 2.30),
        m::vec3(459.97, 68.02, -181.60),
        m::vec3(536.75, 75.01, 2.80),
        m::vec3(-4.61, 117.02, -91.74),
        m::vec3(-2.33, 117.02, 86.34),
        m::vec3(-122.92, 117.02, 84.58),
        m::vec3(-123.44, 117.02, -86.57),
        m::vec3(-300.24, 101.02, -0.15),
        m::vec3(-448.34, 101.02, -156.27),
        m::vec3(-452.94, 101.02, 23.58),
        m::vec3(-206.59, 101.02, -209.52),
        m::vec3(62.59, 101.02, -207.53)
    };

    pointLight light;
    light.diffuse = 1.0f;
    light.ambient = 0.10f;
    light.radius = 30.0f;
    for (size_t i = 0; i < sizeof places / sizeof *places; i++) {
        light.color = { u::randf(), u::randf(), u::randf() };
        light.position = places[i];
        //light.position.y -= 5.0f;
        gWorld.insert(light);
    }
    light.position = { 0, 110, 0 };
    gWorld.insert(light);

    // World only has one directional light
    directionalLight &dlight = gWorld.getDirectionalLight();
    dlight.color = { 0.2, 0.2, 0.2 };
    dlight.ambient = 0.10f;
    dlight.diffuse = 0.50f;
    dlight.direction = { -1.0f, 0.0f, 0.0f };

    // and some map models
    mapModel m;
    m.name = "models/iqmtest";
    m.position = { 40, 95, 0 };
    m.rotate = { 0, -90, 0 };
    gWorld.insert(m);

    m.name = "models/cube";
    m.position = { 85, 112, 35 };
    gWorld.insert(m);

    m.name = "models/ball";
    m.position = { 100, 110, 90 };
    m.scale = { 10, 10, 10 };

    gWorld.insert(m);

    if (!gWorld.load("garden.kdgz"))
        neoFatal("failed to load world");
#endif

    while (gRunning && !shutdown) {
        gClient.update(gWorld, timer.delta());

        gPerspective.fov = cl_fov;
        gPerspective.nearp = cl_nearp;
        gPerspective.farp = cl_farp;
        gPerspective.width = neoWidth();
        gPerspective.height = neoHeight();

        gPipeline.setPerspective(gPerspective);
        gPipeline.setRotation(gClient.getRotation());
        gPipeline.setPosition(gClient.getPosition());
        gPipeline.setTime(timer.ticks());
        gPipeline.setDelta(timer.delta());

        auto mouse = neoMouseState();

        // Update dragging/moving entity
        if (mouse.button & mouseState::kMouseButtonLeft && gSelected && !(gMenuState & kMenuEdit))
            edit::move();

        if (gPlaying && gWorld.isLoaded()) {
            gWorld.upload(gPerspective);
            gl::ClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            gWorld.render(gPipeline);
            gGui.render(gPipeline);
            gui::begin(mouse);
        } else {
            gl::ClearColor(40/255.0f, 30/255.0f, 50/255.0f, 0.1f);
            gl::Clear(GL_COLOR_BUFFER_BIT);
            gGui.render(gPipeline);
            gui::begin(mouse);
        }
        neoSwap();

        if (!gPlaying) {
            const size_t w = neoWidth();
            const size_t h = (neoHeight() / 2);
            const size_t x = neoWidth() / 2 - w / 2; // Center on X
            const size_t y = neoHeight() - h;

            r::pipeline plane;
            r::pipeline logo;

            m::perspective standard = gPerspective;
            standard.width = w;
            standard.height = h;
            standard.fov = 20.0f;

            plane.setPerspective(standard);
            plane.setTime(timer.ticks());
            plane.setDelta(timer.delta());

            //p.setPosition({0, 0, -300});
            plane.setScale({120, 70, 120});

            // Last 1/4th bottom screen
            const m::vec3 rot(160, 0, 0);
            m::quat rx(m::toRadian(rot.x), m::vec3::xAxis);
            m::mat4 rotate;
            rx.getMatrix(&rotate);
            plane.setRotate(rotate);
            plane.setPosition({0, 0, -10});
            plane.setWorld({0, -7.3f, 60.0f});


            gui::drawModel(x, 0, w, neoHeight(), "models/plane", plane, 0, 10);

            logo.setPerspective(standard);
            logo.setTime(timer.ticks());
            logo.setDelta(timer.delta());
            logo.setWorld({0, 0, 0});
            logo.setPosition({0, 0, -130});
            logo.setScale({1, 1, 1});

            gui::drawModel(x, y, w, h, "models/logo", logo);
        }

        if (!gPlaying) {
            // Render the model (for testing only)
            r::pipeline p;
            m::perspective pp = gPerspective;

            const m::vec3 rot(0.0f, -(m::fmod(gPipeline.time() / 10.0f, 361.0f)), 0.0f);
            m::quat ry(m::toRadian(rot.y), m::vec3::yAxis);
            m::mat4 rotate;
            ry.getMatrix(&rotate);
            p.setRotate(rotate);

            int w = neoWidth() / 12;
            int h = neoHeight() / 12;
            pp.width = w;
            pp.height = h;

            p.setPerspective(pp);
            p.setWorld({0, 0, 0});
            p.setPosition({0, 0, -1.5f});
            p.setScale({1, 1, 1});

            gui::drawModel(128 / neoWidth(),
                           neoHeight() / 128 + 16, // 16 to keep above command line
                           w,
                           h, "models/icon", p);
        }

        // Must come first as we want the menu to go over the cross hair if it's
        // launched after playing
        if (gPlaying && !(gMenuState & ~kMenuConsole)) {
            gui::drawLine(neoWidth() / 2, neoHeight() / 2 - 10, neoWidth() / 2, neoHeight() / 2 - 4, 2, 0xFFFFFFE1);
            gui::drawLine(neoWidth() / 2, neoHeight() / 2 + 4, neoWidth() / 2, neoHeight() / 2 + 10, 2, 0xFFFFFFE1);
            gui::drawLine(neoWidth() / 2 + 10, neoHeight() / 2, neoWidth() / 2 + 4, neoHeight() / 2, 2, 0xFFFFFFE1);
            gui::drawLine(neoWidth() / 2 - 10, neoHeight() / 2, neoWidth() / 2 - 4, neoHeight() / 2, 2, 0xFFFFFFE1);
        }

        menuUpdate();

        // Render FPS/MSPF
        gui::drawText(neoWidth() - 10, 10, gui::kAlignRight,
            u::format("%d fps : %.2f mspf\n", timer.fps(), timer.mspf()).c_str(),
            gui::RGBA(255, 255, 255, 255));

        if (varGet<int>("cl_edit").get() && !(gMenuState & kMenuEdit)) {
            gui::drawText(neoWidth() / 2, neoHeight() - 20, gui::kAlignCenter, "F12 to toggle edit menu",
                gui::RGBA(0, 0, 0, 255));
            gui::drawText(neoWidth() / 2, neoHeight() - 40, gui::kAlignCenter, "F10 to toggle color grading menu",
                gui::RGBA(0, 0, 0, 255));
        }

        u::string inputText;
        textState text = neoTextState(inputText);
        if (text != textState::kInactive) {
            gui::drawTriangle(5, 10, 10, 10, 1, gui::RGBA(155, 155, 155, 255));
            gui::drawText(20, 10, gui::kAlignLeft,
                inputText.c_str(), gui::RGBA(255, 255, 255, 255));
            if (text == textState::kFinished) {
                auto values = u::split(inputText);
                if (values.size() == 2) {
                    switch (varChange(values[0], values[1])) {
                    case kVarSuccess:
                        u::print("changed `%s' to `%s'\n", values[0], values[1]);
                        break;
                    case kVarRangeError:
                        u::print("invalid range for `%s'\n", values[0]);
                        break;
                    case kVarTypeError:
                        u::print("invalid type for `%s'\n", values[0]);
                        break;
                    case kVarNotFoundError:
                        u::print("variable `%s' not found\n", values[0]);
                        break;
                    case kVarReadOnlyError:
                        u::print("variable `%s' is read-only\n", values[0]);
                        break;
                    }
                } else if (values.size() == 1) {
                    if (values[0] == "quit" || values[0] == "exit")
                        gRunning = false;
                }
            }
        }

        // Cursor above all else
        if ((gMenuState & ~kMenuConsole || gMenuState & kMenuEdit || gMenuState & kMenuCreate) && !neoRelativeMouse())
            gui::drawImage(mouse.x, mouse.y - (32 - 3), 32, 32, "textures/ui/cursor");

        gui::finish();
    }

    return 0;
}
