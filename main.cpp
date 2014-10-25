#include <time.h>
#include <SDL2/SDL.h>

#include "kdmap.h"
#include "kdtree.h"
#include "client.h"
#include "engine.h"

#include "r_world.h"
#include "r_splash.h"

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

bool bmpWrite(const u::string &file, int width, int height, unsigned char *rgb) {
    struct {
        char bfType[2];
        int32_t bfSize;
        int32_t bfReserved;
        int32_t bfOffBits;
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

    // Populate header
    strncpy(bmph.bfType, "BM", 2);
    bmph.bfOffBits = 54;
    bmph.bfSize = bmph.bfOffBits + bytesPerLine * height;
    bmph.bfReserved = 0;
    bmph.biSize = 40;
    bmph.biWidth = width;
    bmph.biHeight = height;
    bmph.biPlanes = 1;
    bmph.biBitCount = 24;
    bmph.biCompression = 0;
    bmph.biSizeImage = bytesPerLine * height;
    bmph.biXPelsPerMeter = 0;
    bmph.biYPelsPerMeter = 0;
    bmph.biClrUsed = 0;
    bmph.biClrImportant = 0;

    // Calculate how much memory we need for the buffer
    size_t calcSize = 0;
    for (int i = height - 1; i >= 0; i--)
        calcSize += bytesPerLine;

    // line and data buffer
    u::vector<unsigned char> line;
    line.resize(bytesPerLine);
    u::vector<unsigned char> data;
    data.resize(sizeof(bmph) + calcSize);

    unsigned char *store = &data[0];

    bmph.bfSize = u::endianSwap(bmph.bfSize);
    bmph.bfReserved = u::endianSwap(bmph.bfReserved);
    bmph.bfOffBits = u::endianSwap(bmph.bfOffBits);
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

    memcpy(store, &bmph.bfType, 2);          store += 2;
    memcpy(store, &bmph.bfSize, 4);          store += 4;
    memcpy(store, &bmph.bfReserved, 4);      store += 4;
    memcpy(store, &bmph.bfOffBits, 4);       store += 4;
    memcpy(store, &bmph.biSize, 4);          store += 4;
    memcpy(store, &bmph.biWidth, 4);         store += 4;
    memcpy(store, &bmph.biHeight, 4);        store += 4;
    memcpy(store, &bmph.biPlanes, 2);        store += 2;
    memcpy(store, &bmph.biBitCount, 2);      store += 2;
    memcpy(store, &bmph.biCompression, 4);   store += 4;
    memcpy(store, &bmph.biSizeImage, 4);     store += 4;
    memcpy(store, &bmph.biXPelsPerMeter, 4); store += 4;
    memcpy(store, &bmph.biYPelsPerMeter, 4); store += 4;
    memcpy(store, &bmph.biClrUsed, 4);       store += 4;
    memcpy(store, &bmph.biClrImportant, 4);  store += 4;

    // Write the bitmap
    for (int i = height - 1; i >= 0; i--) {
        for (int j = 0; j < width; j++) {
            int ipos = 3 * (width * i + j);
            line[3*j] = rgb[ipos + 2];
            line[3*j+1] = rgb[ipos + 1];
            line[3*j+2] = rgb[ipos];
        }
        memcpy(store, &line[0], bytesPerLine);
        store += bytesPerLine;
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
    gl::ReadPixels(0, 0, screenWidth, screenHeight, GL_RGB, GL_UNSIGNED_BYTE,
        (GLvoid *)pixels.get());

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

int neoMain(frameTimer &timer, int, char **) {
    r::splashScreen gSplash;
    if (!gSplash.load("textures/logo"))
        neoFatal("failed to load splash screen");
    gSplash.upload();

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

    // we're done loading the resources, now upload them (we can't render a loading
    // screen when uploading so we assume the process will be sufficently fast.)
    loadData.gWorld.upload(projection);

    // Now render the world
    client gClient;

    bool running = true;
    while (running) {
        neoSetWindowTitle(u::format("Neothyne: %d fps : %.2f mspf",
            timer.fps(), timer.mspf()).c_str());

        gClient.update(loadData.gMap, timer.delta());

        pipeline.setRotation(gClient.getRotation());
        pipeline.setPosition(gClient.getPosition());
        pipeline.setTime(timer.ticks());

        loadData.gWorld.render(pipeline);

        neoSwap();

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
                case SDL_QUIT:
                    running = false;
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
                            running = false;
                            break;
                        case SDLK_F8:
                            screenShot();
                            break;
                        case SDLK_F12:
                            neoToggleRelativeMouseMode();
                            break;
                    }
                    neoKeyState(e.key.keysym.sym, true);
                    break;
                case SDL_KEYUP:
                    neoKeyState(e.key.keysym.sym, false, true);
                    break;
            }
        }
    }

    c::writeConfig();

    SDL_Quit();
    return 0;
}
