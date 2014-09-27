#include <SDL2/SDL.h>

#include "kdmap.h"
#include "kdtree.h"
#include "client.h"
#include "engine.h"

#include "r_world.h"
#include "r_splash.h"

#include "u_file.h"

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

    auto read = u::read("maps/garden.kdgz", "rb");
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

static void screenShot(const u::string &file) {
    const size_t screenWidth = neoWidth();
    const size_t screenHeight = neoHeight();
    const size_t screenSize = screenWidth * screenHeight;
    SDL_Surface *temp = SDL_CreateRGBSurface(SDL_SWSURFACE, screenWidth, screenHeight,
        8*3, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
    SDL_LockSurface(temp);
    unsigned char *pixels = new unsigned char[screenSize * 3];
    // make sure we're reading from the final framebuffer when obtaining the pixels
    // for the screenshot.
    gl::BindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    gl::ReadPixels(0, 0, screenWidth, screenHeight, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    texture::reorient(pixels, screenWidth, screenHeight, 3, screenWidth * 3,
        (unsigned char *)temp->pixels, false, true, false);
    SDL_UnlockSurface(temp);
    delete[] pixels;
    SDL_SaveBMP(temp, file.c_str());
    SDL_FreeSurface(temp);
}

int neoMain(frameTimer &timer, int argc, char **argv) {
    argc--;
    argv++;
    if (argc == 2)
        neoResize(atoi(argv[0]), atoi(argv[1]));

    r::splashScreen gSplash;
    if (!gSplash.load("textures/logo"))
        neoFatal("failed to load splash screen");
    gSplash.upload();

    r::rendererPipeline pipeline;
    m::perspectiveProjection projection;
    projection.fov = 90.0f;
    projection.width = neoWidth();
    projection.height = neoHeight();
    projection.nearp = 1.0f;
    projection.farp = 2048.0f;

    pipeline.setPerspectiveProjection(projection);
    pipeline.setWorldPosition(m::vec3::origin);

    // create a loader thread
    loadThreadData loadData;
    SDL_AtomicSet(&loadData.done, kLoadInProgress);
    SDL_CreateThread((SDL_ThreadFunction)&loadThread, nullptr, (void*)&loadData);

    neoSetVSyncOption(kSyncNone);

    // while that thread is running render the loading screen
    timer.cap(30); // Loading screen at 30fps
    while (SDL_AtomicGet(&loadData.done) == kLoadInProgress) {
        glClear(GL_COLOR_BUFFER_BIT);
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

    timer.cap(0); // don't cap the world
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
                    SDL_Quit();
                    return 0;
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
                            screenShot("screenshot.bmp");
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
    return 0;
}
