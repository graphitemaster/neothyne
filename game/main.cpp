#include <time.h>

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
#include "u_pair.h"

// Game globals
bool gRunning = true;
bool gPlaying = false;
world::descriptor *gSelected = nullptr; // Selected world entity
client gClient;
world gWorld;
r::pipeline gPipeline;
m::perspective gPerspective;

template <typename T>
static inline void memput(unsigned char *&store, const T &data) {
    memcpy(store, &data, sizeof(T));
    store += sizeof(T);
}

static bool bmpWrite(const u::string &file, int width, int height, unsigned char *rgb) {
    struct {
        char bfType[2];
        int32_t bfSize;
        int32_t bfReserved;
        int32_t bfDataOffset;
        int32_t biSize;
        int32_t biWidth;
        int32_t biHeight;
        int16_t biPlanes;
        int16_t biBitCount;
        int32_t biCompression;
        int32_t biSizeImage;
        int32_t biXPelsPerMeter;
        int32_t biYPelsPerMeter;
        int32_t biClrUsed;
        int32_t biClrImportant;
    } bmph;

    const size_t bytesPerLine = (3 * (width + 1) / 4) * 4;
    const size_t headerSize = 54;
    const size_t imageSize = bytesPerLine * height;
    const size_t dataSize = headerSize + imageSize;

    // Populate header
    memcpy(bmph.bfType, (const void *)"BM", 2);
    bmph.bfSize = dataSize;
    bmph.bfReserved = 0;
    bmph.bfDataOffset = headerSize;
    bmph.biSize = 40;
    bmph.biWidth = width;
    bmph.biHeight = height;
    bmph.biPlanes = 1;
    bmph.biBitCount = 24;
    bmph.biCompression = 0;
    bmph.biSizeImage = imageSize;
    bmph.biXPelsPerMeter = 0;
    bmph.biYPelsPerMeter = 0;
    bmph.biClrUsed = 0;
    bmph.biClrImportant = 0;

    // line and data buffer
    u::vector<unsigned char> line;
    line.resize(bytesPerLine);
    u::vector<unsigned char> data;
    data.resize(dataSize);

    bmph.bfSize = u::endianSwap(bmph.bfSize);
    bmph.bfReserved = u::endianSwap(bmph.bfReserved);
    bmph.bfDataOffset = u::endianSwap(bmph.bfDataOffset);
    bmph.biSize = u::endianSwap(bmph.biSize);
    bmph.biWidth = u::endianSwap(bmph.biWidth);
    bmph.biHeight = u::endianSwap(bmph.biHeight);
    bmph.biPlanes = u::endianSwap(bmph.biPlanes);
    bmph.biBitCount = u::endianSwap(bmph.biBitCount);
    bmph.biCompression = u::endianSwap(bmph.biCompression);
    bmph.biSizeImage = u::endianSwap(bmph.biSizeImage);
    bmph.biXPelsPerMeter = u::endianSwap(bmph.biXPelsPerMeter);
    bmph.biYPelsPerMeter = u::endianSwap(bmph.biYPelsPerMeter);
    bmph.biClrUsed = u::endianSwap(bmph.biClrUsed);
    bmph.biClrImportant = u::endianSwap(bmph.biClrImportant);

    unsigned char *store = &data[0];
    memput(store, bmph.bfType);
    memput(store, bmph.bfSize);
    memput(store, bmph.bfReserved);
    memput(store, bmph.bfDataOffset);
    memput(store, bmph.biSize);
    memput(store, bmph.biWidth);
    memput(store, bmph.biHeight);
    memput(store, bmph.biPlanes);
    memput(store, bmph.biBitCount);
    memput(store, bmph.biCompression);
    memput(store, bmph.biSizeImage);
    memput(store, bmph.biXPelsPerMeter);
    memput(store, bmph.biYPelsPerMeter);
    memput(store, bmph.biClrUsed);
    memput(store, bmph.biClrImportant);

    // Write the bitmap
    for (int i = height - 1; i >= 0; i--) {
        for (int j = 0; j < width; j++) {
            int ipos = 3 * (width * i + j);
            line[3*j]   = rgb[ipos + 2];
            line[3*j+1] = rgb[ipos + 1];
            line[3*j+2] = rgb[ipos];
        }
        memcpy(store, &line[0], line.size());
        store += line.size();
    }

    return u::write(data, file, "wb");
}

static void screenShot() {
    // Generate a unique filename from the time
    time_t t = time(nullptr);
    struct tm tm = *localtime(&t);
    u::string file = u::format("%sscreenshots/%d-%d-%d-%d%d%d.bmp",
        neoUserPath(), tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
        tm.tm_hour, tm.tm_min, tm.tm_sec);

    // Get metrics for reading the final composite from GL
    const int screenWidth = int(neoWidth());
    const int screenHeight = int(neoHeight());
    const int screenSize = screenWidth * screenHeight;
    auto pixels = u::unique_ptr<unsigned char>(new unsigned char[screenSize * 3]);

    // make sure we're reading from the final framebuffer when obtaining the pixels
    // for the screenshot.
    gl::BindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    gl::PixelStorei(GL_PACK_ALIGNMENT, 1);
    gl::ReadPixels(0, 0, screenWidth, screenHeight, GL_RGB, GL_UNSIGNED_BYTE,
        (GLvoid *)pixels.get());
    gl::PixelStorei(GL_PACK_ALIGNMENT, 8);

    // Reorient because it will be upside down
    auto temp = u::unique_ptr<unsigned char>(new unsigned char[screenSize * 3]);
    texture::reorient(pixels.get(), screenWidth, screenHeight, 3, screenWidth * 3,
        temp.get(), false, true, false);

    // Write the data
    if (bmpWrite(file, screenWidth, screenHeight, temp.get()))
        u::print("[screenshot] => %s\n", file);
}

VAR(float, cl_fov, "field of view", 45.0f, 270.0f, 90.0f);
VAR(float, cl_nearp, "near plane", 0.0f, 10.0f, 1.0f);
VAR(float, cl_farp, "far plane", 128.0f, 4096.0f, 2048.0f);

static u::pair<size_t, size_t> scaleImage(size_t iw, size_t ih, size_t w, size_t h) {
    float ratio = float(iw) / float(ih);
    size_t ow = w;
    size_t oh = h;
    if (w > h)
        oh = size_t(float(ow) / ratio);
    else
        ow = size_t(float(oh) * ratio);
    return { ow, oh };
}

static void setBinds() {
    neoBindSet("MouseDnL", []() {
        if (varGet<int>("cl_edit").get() && !(gMenuState & kMenuEdit))
            edit::select();
    });

    neoBindSet("EscapeDn", []() {
        if (gPlaying && gMenuState & kMenuMain) {
            if (gMenuState & kMenuConsole) {
                gMenuState = kMenuConsole;
            } else {
                gMenuState &= ~kMenuMain;
            }
        } else if (!(gMenuState & kMenuEdit)) {
            // If the console is opened leave it open
            gMenuState = (gMenuState & kMenuConsole)
                ? kMenuMain | kMenuConsole
                : kMenuMain;
        } else {
            gMenuState &= ~kMenuEdit;
        }
        if (gPlaying && !(gMenuState & kMenuMain))
            neoRelativeMouse(true);
        else
            neoRelativeMouse(false);
        neoCenterMouse();
    });

    neoBindSet("F8Dn", []() {
        screenShot();
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

int neoMain(frameTimer &timer, int, char **) {
    // Setup rendering pipeline
    gPerspective.fov = cl_fov;
    gPerspective.nearp = cl_nearp;
    gPerspective.farp = cl_farp;
    gPerspective.width = neoWidth();
    gPerspective.height = neoHeight();

    gPipeline.setPerspective(gPerspective);
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

#if 0
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
    light.ambient = 1.0f;
    light.radius = 30.0f;
    for (size_t i = 0; i < sizeof(places)/sizeof(*places); i++) {
        light.color = { u::randf(), u::randf(), u::randf() };
        light.position = places[i];
        light.position.y -= 10.0f;
        gWorld.insert(light);
    }
    light.position = { 0, 110, 0 };
    gWorld.insert(light);

    directionalLight dlight;
    dlight.color = { 0.7, 0.7, 0.7 };
    dlight.ambient = 0.75f;
    dlight.diffuse = 0.75f;
    dlight.direction = { -1.0f, 0.0f, 0.0f };

    gWorld.insert(dlight);

    // and some map models
    mapModel m;
    m.name = "models/lg";
    m.position = { 0, 120, 0 };
    m.rotate = { 0, 90, 0 };
    gWorld.insert(m);

    m.name = "models/rl";
    m.position.x += 40.0f;
    gWorld.insert(m);

    if (!gWorld.load("garden.kdgz"))
        neoFatal("failed to load world");
#endif

    while (gRunning) {
        gClient.update(gWorld, timer.delta());

        gPerspective.fov = cl_fov;
        gPerspective.nearp = cl_nearp;
        gPerspective.farp = cl_farp;

        gPipeline.setPerspective(gPerspective);
        gPipeline.setRotation(gClient.getRotation());
        gPipeline.setPosition(gClient.getPosition());
        gPipeline.setTime(timer.ticks());

        auto mouse = neoMouseState();

        // Update dragging/moving entity
        if (mouse.button & mouseState::kMouseButtonLeft && gSelected && !(gMenuState & kMenuEdit))
            edit::move();

        if (gPlaying && gWorld.isLoaded()) {
            gWorld.upload(gPerspective);
            gl::ClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            gWorld.render(gPipeline);
        } else {
            gl::ClearColor(40/255.0f, 30/255.0f, 50/255.0f, 0.1f);
            gl::Clear(GL_COLOR_BUFFER_BIT);
        }
        gGui.render(gPipeline);
        neoSwap();

        gui::begin(mouse);

        // Must come first as we want the menu to go over the cross hair if it's
        // launched after playing
        if (gPlaying && !(gMenuState & ~kMenuConsole)) {
            gui::drawLine(neoWidth() / 2, neoHeight() / 2 - 10, neoWidth() / 2, neoHeight() / 2 - 4, 2, 0xFFFFFFE1);
            gui::drawLine(neoWidth() / 2, neoHeight() / 2 + 4, neoWidth() / 2, neoHeight() / 2 + 10, 2, 0xFFFFFFE1);
            gui::drawLine(neoWidth() / 2 + 10, neoHeight() / 2, neoWidth() / 2 + 4, neoHeight() / 2, 2, 0xFFFFFFE1);
            gui::drawLine(neoWidth() / 2 - 10, neoHeight() / 2, neoWidth() / 2 - 4, neoHeight() / 2, 2, 0xFFFFFFE1);
        }
        if (!gPlaying) {
            const size_t w = (neoWidth() / 3); // 1/3rd the screen width
            const size_t h = (neoHeight() / 3); // 1/3rd the screen height
            const size_t x = neoWidth() / 2 - w / 2; // Center on X
            const size_t y = neoHeight() - h;
            auto size = scaleImage(640, 200, w, h); // resize (while preserving aspect ratio)
            gui::drawImage(x, y, u::get<0>(size), u::get<1>(size), "<nocompress>textures/menu_logo", true);
        }

        menuUpdate();

        // Render FPS/MSPF
        gui::drawText(neoWidth(), 10, gui::kAlignRight,
            u::format("%d fps : %.2f mspf\n", timer.fps(), timer.mspf()).c_str(),
            gui::RGBA(255, 255, 255, 255));

        if (varGet<int>("cl_edit").get() && !(gMenuState & kMenuEdit)) {
            gui::drawText(neoWidth() / 2, neoHeight() - 20, gui::kAlignCenter, "F12 to toggle edit menu",
                gui::RGBA(0, 0, 0, 255));
        }

        //if (input) {
        //    if (inputString[0] == '/')
        //        inputString.pop_front();
            // Accepting console commands
        //    gui::drawTriangle(5, 10, 10, 10, 1, gui::RGBA(155, 155, 155, 255));
        //    gui::drawText(20, 10, gui::kAlignLeft,
        //        inputString.c_str(), gui::RGBA(255, 255, 255, 255));
        //}

        // Cursor above all else
        if ((gMenuState & ~kMenuConsole || gMenuState & kMenuEdit) && !neoRelativeMouse())
            gui::drawImage(mouse.x, mouse.y - (32 - 3), 32, 32, "<nocompress>textures/ui/cursor");

        gui::finish();
    }

    return 0;
}
