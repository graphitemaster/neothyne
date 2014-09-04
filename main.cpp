#include <SDL2/SDL.h>

#include "kdtree.h"
#include "kdmap.h"
#include "renderer.h"
#include "client.h"

static constexpr size_t kScreenWidth = 1600;
static constexpr size_t kScreenHeight = 852;

u::map<int, int> &getKeyState(int key = 0, bool keyDown = false, bool keyUp = false) {
    static u::map<int, int> gKeyMap;
    if (keyDown) gKeyMap[key]++;
    if (keyUp) gKeyMap[key] = 0;
    return gKeyMap;
}

void getMouseDelta(int *deltaX, int *deltaY) {
    SDL_GetRelativeMouseState(deltaX, deltaY);
}

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

static SDL_Window *initSDL(size_t width, size_t height) {
    SDL_Window *window;
    SDL_Init(SDL_INIT_VIDEO);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    //SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    //SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 6);

    window = SDL_CreateWindow("Neothyne",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        width,
        height,
        SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL
    );

    if (!SDL_GL_CreateContext(window)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
            "Neothyne: Initialization error",
            "OpenGL 3.3 or higher is required",
            nullptr);
        SDL_DestroyWindow(window);
        SDL_Quit();
    }

    SDL_SetRelativeMouseMode(SDL_TRUE);
    SDL_GL_SetSwapInterval(0); // no vsync please

    return window;
}

int main(int argc, char **argv) {
    argc--;
    argv++;

    size_t screenWidth = kScreenWidth;
    size_t screenHeight = kScreenHeight;

    if (argc == 2) {
        screenWidth = atoi(argv[0]);
        screenHeight = atoi(argv[1]);
    }

    SDL_Window *gScreen = initSDL(screenWidth, screenHeight);

    // before loading any art assets we need to establish a loading screen
    initGL();
    splashScreen gSplash;
    gSplash.load("textures/logo.png");
    gSplash.upload();

    rendererPipeline pipeline;
    m::perspectiveProjection projection;
    projection.fov = 90.0f;
    projection.width = screenWidth;
    projection.height = screenHeight;
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
        SDL_GL_SwapWindow(gScreen);
    }

    if (SDL_AtomicGet(&loadData.done) != kLoadSucceeded)
        return 1;

    // we're done loading the resources, now upload them (we can't render a loading
    // screen when uploading so we assume the process will be sufficently fast.)
    loadData.gWorld.upload(loadData.gMap);

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
            SDL_SetWindowTitle(gScreen, title.c_str());
            fpsCounter = 0;
            fpsTime = time;
        }

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        gClient.update(loadData.gMap, dt);

        pipeline.setRotation(gClient.getRotation());
        pipeline.setPosition(gClient.getPosition());

        loadData.gWorld.draw(pipeline);

        //glFinish();
        SDL_GL_SwapWindow(gScreen);

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
                case SDL_QUIT:
                    SDL_Quit();
                    return 0;
                case SDL_KEYDOWN:
                    switch (e.key.keysym.sym) {
                        case SDLK_ESCAPE:
                            running = false;
                            break;
                        case SDLK_F8:
                            screenShot("screenshot.bmp", projection);
                            break;
                        case SDLK_F12:
                            SDL_SetRelativeMouseMode(
                                SDL_GetRelativeMouseMode() == SDL_TRUE
                                    ? SDL_FALSE : SDL_TRUE);
                            break;
                    }
                    getKeyState(e.key.keysym.sym, true);
                    break;
                case SDL_KEYUP:
                    getKeyState(e.key.keysym.sym, false, true);
                    break;
            }
        }
    }
    return 0;
}
