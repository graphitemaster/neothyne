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

static SDL_Window *initSDL(void) {
    SDL_Window *window;
    SDL_Init(SDL_INIT_VIDEO);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 6);

    window = SDL_CreateWindow("Neothyne",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        kScreenWidth,
        kScreenHeight,
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

uint32_t getTicks() {
    static uint32_t base = 0;
    if (base == 0)
        base = SDL_GetTicks();
    return SDL_GetTicks() - base;
}

int main(void) {
    SDL_Window *gScreen;
    gScreen = initSDL();
    initGL();

    client gClient;
    kdMap gMap;
    world gWorld;
    splashScreen gSplash;

    // render the splash screen
    gSplash.init("textures/splash.jpg");
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    gSplash.render();
    SDL_GL_SwapWindow(gScreen);

    // open map
    FILE *fp = fopen("maps/garden.kdgz", "r");
    u::vector<unsigned char> mapData;
    if (!fp) {
        fprintf(stderr, "failed opening map\n");
        return 0;
    } else {
        fseek(fp, 0, SEEK_END);
        size_t length = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        mapData.resize(length);
        fread(&*mapData.begin(), length, 1, fp);
        fclose(fp);
    }
    gMap.load(mapData);
    gWorld.load(gMap);

    // setup projection
    perspectiveProjection projection;
    projection.fov = 90.0f;
    projection.width = kScreenWidth;
    projection.height = kScreenHeight;
    projection.near = 1.0f;
    projection.far = 4096.0f;

    // go
    uint32_t time;
    uint32_t oldTime;
    uint32_t fpsTime;
    uint32_t fpsCounter = 0;

    uint32_t curTime = getTicks();
    oldTime = curTime;
    fpsTime = curTime;
    while (true) {
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

        // worldClient.Update(dt, time)
        //  world.Update(dt, ticks)
        //  client.move(dt)
        // client.Update(dt, ti me)

        gClient.update(gMap, dt);
        m::vec3 target;
        m::vec3 up;
        m::vec3 side;
        gClient.getDirection(&target, &up, &side);

        rendererPipeline pipeline;
        pipeline.setWorldPosition(0.0f, 0.0f, 0.0f);
        pipeline.setCamera(gClient.getPosition(), target, up);
        pipeline.setPerspectiveProjection(projection);

        gWorld.draw(pipeline);

        //glFinish();
        SDL_GL_SwapWindow(gScreen);

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
                case SDL_QUIT:
                    return 0;
                case SDL_KEYDOWN:
                    if (e.key.keysym.sym == SDLK_ESCAPE)
                        return 0;
                    if (e.key.keysym.sym == SDLK_F8)
                        screenShot("screenshot.bmp");
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
