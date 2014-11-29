#include <time.h>
#include <SDL2/SDL.h>

#include "kdmap.h"
#include "kdtree.h"
#include "client.h"
#include "menu.h"
#include "engine.h"

#include "r_world.h"
#include "r_splash.h"
#include "r_gui.h"

#include "c_var.h"

#include "u_file.h"
#include "u_misc.h"

// we load assets in a different thread
enum {
    kLoadInProgress, // loading in progress
    kLoadFailed,     // loading failed
    kLoadSucceeded   // loading succeeded
};

struct loadThreadData {
    kdMap gMap;
    r::world gWorld;
    SDL_atomic_t done;
};

static bool loadThread(void *threadData) {
    loadThreadData *data = (loadThreadData*)threadData;

    auto read = u::read(neoGamePath() + "maps/garden.kdgz", "rb");
    if (read) {
        if (!data->gMap.load(*read))
            goto error;
        if (!data->gWorld.load(data->gMap))
            goto error;
        SDL_AtomicSet(&data->done, kLoadSucceeded);
        return true;
    }
error:
    SDL_AtomicSet(&data->done, kLoadFailed);
    return false;
}

template <typename T>
static inline void memput(unsigned char *&store, const T &data) {
  memcpy(store, &data, sizeof(T));
  store += sizeof(T);
}

bool bmpWrite(const u::string &file, int width, int height, unsigned char *rgb) {
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
    strncpy(bmph.bfType, "BM", 2);
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

bool gRunning = true;
bool gPlaying = false;

int neoMain(frameTimer &timer, int, char **) {
    r::splashScreen gSplash;
    if (!gSplash.load("textures/logo"))
        neoFatal("failed to load splash screen");
    gSplash.upload();

    r::gui gGui;
    if (!gGui.load("fonts/droidsans"))
        neoFatal("failed to load font");
    if (!gGui.upload())
        neoFatal("failed to initialize GUI rendering method\n");

    r::rendererPipeline pipeline;
    m::perspectiveProjection projection;
    projection.fov = cl_fov;
    projection.width = neoWidth();
    projection.height = neoHeight();
    projection.nearp = cl_nearp;
    projection.farp = cl_farp;

    pipeline.setPerspectiveProjection(projection);
    pipeline.setWorldPosition(m::vec3::origin);

    // create a loader thread
    loadThreadData loadData;
    SDL_AtomicSet(&loadData.done, kLoadInProgress);
    SDL_CreateThread((SDL_ThreadFunction)&loadThread, nullptr, (void*)&loadData);

    // while that thread is running render the loading screen
    while (SDL_AtomicGet(&loadData.done) == kLoadInProgress) {
        gl::Clear(GL_COLOR_BUFFER_BIT);
        pipeline.setTime(timer.ticks() / 500.0f);
        gSplash.render(pipeline);
        neoSwap();
    }

    if (SDL_AtomicGet(&loadData.done) != kLoadSucceeded)
        return 1;

    // Now render the menu
    client gClient;

    bool input = false;
    u::string inputString = "";

    menuReset(); // To initialize

    neoSetWindowTitle("Neothyne");
    neoCenterMouse();
    int mouse[4] = {0}; // X, Y, Scroll, Button
    while (gRunning) {
        if (!input)
            gClient.update(loadData.gMap, timer.delta());

        pipeline.setRotation(gClient.getRotation());
        pipeline.setPosition(gClient.getPosition());
        pipeline.setTime(timer.ticks());

        if (gPlaying) {
            loadData.gWorld.upload(projection);
            gl::ClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            loadData.gWorld.render(pipeline);
        } else {
            gl::ClearColor(40/255.0f, 30/255.0f, 50/255.0f, 0.1f);
            gl::Clear(GL_COLOR_BUFFER_BIT);
        }
        gGui.render(pipeline);
        neoSwap();

        SDL_Event e;
        mouse[2] = 0;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
                case SDL_QUIT:
                    gRunning = false;
                    break;
                case SDL_WINDOWEVENT:
                    switch (e.window.event) {
                        case SDL_WINDOWEVENT_RESIZED:
                            // Resize the window
                            neoResize(e.window.data1, e.window.data2);
                            projection.width = neoWidth();
                            projection.height = neoHeight();
                            pipeline.setPerspectiveProjection(projection);
                            break;
                    }
                    break;
                case SDL_KEYDOWN:
                    switch (e.key.keysym.sym) {
                        case SDLK_ESCAPE:
                            // The effect of the ESC key is strange. When in game
                            // it should bring up the menu. While in the menu it
                            // should always bring you to main menu, except while
                            // in game and in menu and already at main menu then
                            // ESC should bring you back in game.
                            if (gPlaying && gMenuState & kMenuMain) {
                                if (gMenuState & kMenuConsole) {
                                    gMenuState = kMenuConsole;
                                } else {
                                    gMenuState &= ~kMenuMain;
                                    //neoRelativeMouse(true);
                                }
                            } else {
                                // If the console is opened leave it open
                                gMenuState = (gMenuState & kMenuConsole)
                                    ? kMenuMain | kMenuConsole
                                    : kMenuMain;
                            }
                            if (gPlaying && !(gMenuState & kMenuMain))
                                neoRelativeMouse(true);
                            else
                                neoRelativeMouse(false);
                            neoCenterMouse();
                            break;
                        case SDLK_F8:
                            screenShot();
                            break;
                        case SDLK_F9:
                            u::print("%d fps : %.2f mspf\n", timer.fps(), timer.mspf());
                            break;
                        case SDLK_F11:
                            gMenuState ^= kMenuConsole;
                            break;
                        case SDLK_BACKSPACE:
                            if (input)
                                inputString.pop_back();
                            break;
                        case SDLK_SLASH:
                            input = !input;
                            if (input) {
                                SDL_StartTextInput();
                            } else {
                                SDL_StopTextInput();
                            }
                            inputString.reset();
                            break;
                        case SDLK_RETURN:
                            if (input) {
                                auto split = u::split(inputString);
                                switch (c::varChange(split[0], split[1], true)) {
                                    case c::kVarNotFoundError:
                                        u::print("'%s' not found\n", split[0]);
                                        break;
                                    case c::kVarRangeError:
                                        u::print("invalid range '%s' for '%s'\n", split[1], split[0]);
                                        break;
                                    case c::kVarReadOnlyError:
                                        u::print("'%s' is read only\n", split[0]);
                                        break;
                                    case c::kVarTypeError:
                                        u::print("value '%s' wrong type for '%s'\n", split[1], split[0]);
                                        break;
                                    case c::kVarSuccess:
                                        u::print("'%s' changed\n", split[0]);
                                        break;
                                }
                                input = false;
                                inputString.reset();
                            }
                    }
                    neoKeyState(e.key.keysym.sym, true);
                    break;
                case SDL_KEYUP:
                    neoKeyState(e.key.keysym.sym, false, true);
                    break;
                case SDL_MOUSEMOTION:
                    mouse[0] = e.motion.x;
                    mouse[1] = neoHeight() - e.motion.y;
                    break;
                case SDL_MOUSEWHEEL:
                    mouse[2] = e.wheel.y;
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    switch (e.button.button) {
                        case SDL_BUTTON_LEFT:
                            mouse[3] |= gui::kMouseButtonLeft;
                            break;
                        case SDL_BUTTON_RIGHT:
                            mouse[3] |= gui::kMouseButtonRight;
                            break;
                    }
                    break;
                case SDL_MOUSEBUTTONUP:
                    switch (e.button.button) {
                        case SDL_BUTTON_LEFT:
                            mouse[3] &= ~gui::kMouseButtonLeft;
                            break;
                        case SDL_BUTTON_RIGHT:
                            mouse[3] &= ~gui::kMouseButtonRight;
                            break;
                    }
                    break;
                case SDL_TEXTINPUT:
                    inputString += e.text.text;
                    break;
            }
        }

        gui::begin(mouse);

        // Must come first as we want the menu to go over the cross hair if it's
        // launched after playing
        if (gPlaying && !(gMenuState & ~kMenuConsole)) {
            gui::drawLine(neoWidth() / 2, neoHeight() / 2 - 10, neoWidth() / 2, neoHeight() / 2 - 4, 2, 0xFFFFFFE1);
            gui::drawLine(neoWidth() / 2, neoHeight() / 2 + 4, neoWidth() / 2, neoHeight() / 2 + 10, 2, 0xFFFFFFE1);
            gui::drawLine(neoWidth() / 2 + 10, neoHeight() / 2, neoWidth() / 2 + 4, neoHeight() / 2, 2, 0xFFFFFFE1);
            gui::drawLine(neoWidth() / 2 - 10, neoHeight() / 2, neoWidth() / 2 - 4, neoHeight() / 2, 2, 0xFFFFFFE1);
        }
        if (!gPlaying)
            gui::drawImage(neoWidth()/2-320, neoHeight()/2+50, 640, 200, "<nocompress>textures/menu_logo");

        menuUpdate();

        // Render FPS/MSPF
        gui::drawText(neoWidth(), 10, gui::kAlignRight,
            u::format("%d fps : %.2f mspf\n", timer.fps(), timer.mspf()),
            gui::RGBA(255, 255, 255, 255));

        if (input) {
            if (inputString[0] == '/')
                inputString.pop_front();
            // Accepting console commands
            gui::drawTriangle(5, 10, 10, 10, 1, gui::RGBA(155, 155, 155, 255));
            gui::drawText(20, 10, gui::kAlignLeft,
                inputString, gui::RGBA(255, 255, 255, 255));
        }

        // Cursor above all else
        if (gMenuState & ~kMenuConsole)
            gui::drawImage(mouse[0], mouse[1] - (32 - 3), 32, 32, "<nocompress>textures/ui/cursor");

        gui::finish();
    }

    c::writeConfig();

    SDL_Quit();
    return 0;
}
