#include <SDL2/SDL.h>

#include "kdtree.h"
#include "kdmap.h"
#include "renderer.h"
#include "client.h"

static size_t      gScreenWidth  = 1024;
static size_t      gScreenHeight = 768;
static SDL_Window *gScreen       = nullptr;

u::map<int, int> &getKeyState(int key = 0, bool keyDown = false, bool keyUp = false) {
    static u::map<int, int> gKeyMap;
    if (keyDown) gKeyMap[key]++;
    if (keyUp) gKeyMap[key] = 0;
    return gKeyMap;
}

void getMouseDelta(int *deltaX, int *deltaY) {
    SDL_GetRelativeMouseState(deltaX, deltaY);
}

int main(void) {
    SDL_Init(SDL_INIT_VIDEO);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    gScreen = SDL_CreateWindow("Neothyne",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        gScreenWidth,
        gScreenHeight,
        SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL
    );

    SDL_GL_CreateContext(gScreen);
    SDL_SetRelativeMouseMode(SDL_TRUE);

    // prepare GL
    glClearColor(0.48f, 0.58f, 0.72f, 0.0f);
    glFrontFace(GL_CW);
    glClearDepth(1.0f);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glShadeModel(GL_SMOOTH);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

    kdTree gTree; // model
    renderer gRenderer; // view
    client gClient; // controller
    kdMap gMap; // model

    printf("Loading OBJ into KD-tree (this may take awhile)\n");
    gTree.load("test.obj");
    gMap.load(gTree.serialize());
    gRenderer.load(gMap);

    // go go go!
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
        pipeline.setPerspectiveProjection(90.0f, gScreenWidth, gScreenHeight, 1.0f, 1200.0f);

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
