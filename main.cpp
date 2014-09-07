#include <SDL2/SDL.h>

#include "kdmap.h"
#include "kdtree.h"
#include "client.h"
#include "renderer.h"
#include "engine.h"

// we load assets in a different thread
enum {
    kLoadInProgress, // loading in progress
    kLoadFailed,     // loading failed
    kLoadSucceeded   // loading succeeded
};

struct loadThreadData {
    kdMap gMap;
    world gWorld;
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

int neoMain(int argc, char **argv) {
    argc--;
    argv++;
    if (argc == 2)
        neoResize(atoi(argv[0]), atoi(argv[1]));

    splashScreen gSplash;
    gSplash.load("textures/logo.png");
    gSplash.upload();

    rendererPipeline pipeline;
    m::perspectiveProjection projection;
    projection.fov = 90.0f;
    projection.width = neoWidth();
    projection.height = neoHeight();
    projection.nearp = 1.0f;
    projection.farp = 2048.0f;

    pipeline.setPerspectiveProjection(projection);
    pipeline.setWorldPosition(m::vec3(0.0f, 0.0f, 0.0f));

    // create a loader thread
    loadThreadData loadData;
    SDL_AtomicSet(&loadData.done, kLoadInProgress);
    SDL_CreateThread((SDL_ThreadFunction)&loadThread, nullptr, (void*)&loadData);

    // while that thread is running render the loading screen
    uint32_t splashTime = SDL_GetTicks();
    while (SDL_AtomicGet(&loadData.done) == kLoadInProgress) {
        glClear(GL_COLOR_BUFFER_BIT);
        gSplash.render(0.002f * (float)(SDL_GetTicks() - splashTime), pipeline);
        neoSwap();
    }

    if (SDL_AtomicGet(&loadData.done) != kLoadSucceeded)
        return 1;

    // we're done loading the resources, now upload them (we can't render a loading
    // screen when uploading so we assume the process will be sufficently fast.)
    loadData.gWorld.upload(projection);

    // Now render the world
    client gClient;

    uint32_t time;
    uint32_t oldTime;
    uint32_t fpsTime;
    uint32_t fpsCounter = 0;

    uint32_t curTime = SDL_GetTicks();
    oldTime = curTime;
    fpsTime = curTime;

    bool running = true;
    while (running) {
        time = SDL_GetTicks();
        float dt = 0.001f * (float)(time - oldTime);
        oldTime = time;
        fpsCounter++;
        if (time - fpsTime > 1000.0f) {
            u::string title = u::format("Neothyne: (%zu FPS)", fpsCounter);
            neoSetWindowTitle(title.c_str());
            fpsCounter = 0;
            fpsTime = time;
        }

        gClient.update(loadData.gMap, dt);

        pipeline.setRotation(gClient.getRotation());
        pipeline.setPosition(gClient.getPosition());

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
                            projection.height = neoWidth();
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
