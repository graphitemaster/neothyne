#include <SDL2/SDL.h>

#include "kdtree.h"
#include "kdmap.h"
#include "renderer.h"
#include "client.h"

static constexpr size_t kScreenWidth = 1024;
static constexpr size_t kScreenHeight = 768;

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

    window = SDL_CreateWindow("Neothyne",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        kScreenWidth,
        kScreenHeight,
        SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL
    );

    SDL_GL_CreateContext(window);
    SDL_SetRelativeMouseMode(SDL_TRUE);

    return window;
}

static void initGL(void) {
    glClearColor(0.48f, 0.58f, 0.72f, 0.0f);

    // back face culling
    glFrontFace(GL_CW);
    glCullFace(GL_BACK);
    glEnable(GL_CULL_FACE);

    // depth buffer + depth test
    glClearDepth(1.0f);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_DEPTH_TEST);

    // shade model
    glShadeModel(GL_SMOOTH);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // nice perspective correction hint
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
}

int main(void) {
    SDL_Window *gScreen;
    gScreen = initSDL();
    initGL();

    kdTree gTree;
    renderer gRenderer;
    client gClient;
    kdMap gMap;

    printf("Loading OBJ into KD-tree (this may take awhile)\n");
    gTree.load("test.obj");
    gMap.load(gTree.serialize());
    gRenderer.load(gMap);

    while (true) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        gClient.update(gMap);
        m::vec3 target;
        m::vec3 up;
        m::vec3 side;
        gClient.getDirection(&target, &up, &side);

        rendererPipeline pipeline;
        pipeline.position(0.0f, 0.0f, 3.0f);
        pipeline.setCamera(gClient.getPosition(), target, up);
        pipeline.setPerspectiveProjection(90.0f, kScreenWidth, kScreenHeight, 1.0f, 1200.0f);

        gRenderer.draw((const GLfloat*)pipeline.getTransform());

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
                        gRenderer.screenShot("screenshot.bmp");
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
