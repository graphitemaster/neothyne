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

    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 6);

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

static void checkError(const char *statement, const char *name, size_t line) {
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        fprintf(stderr, "GL error %08x, at %s:%zu - for %s\n", err, name, line,
            statement);
        abort();
    }
}

#define GL_CHECK(X) \
    do { \
        X; \
        checkError(#X, __FILE__, __LINE__); \
    } while (0)


static void initGL(void) {
    GL_CHECK(glClearColor(0.0f, 0.0f, 0.0f, 0.0f));

    // back face culling
    GL_CHECK(glFrontFace(GL_CW));
    GL_CHECK(glCullFace(GL_BACK));
    GL_CHECK(glEnable(GL_CULL_FACE));

    // depth buffer + depth test
    GL_CHECK(glClearDepth(1.0f));
    GL_CHECK(glDepthFunc(GL_LEQUAL));
    GL_CHECK(glEnable(GL_DEPTH_TEST));

    // shade model
    GL_CHECK(glShadeModel(GL_SMOOTH));
    GL_CHECK(glEnable(GL_BLEND));
    GL_CHECK(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

    // multisample anti-aliasing
    GL_CHECK(glEnable(GL_MULTISAMPLE));
}

int main(void) {
    SDL_Window *gScreen;
    gScreen = initSDL();
    initGL();

    kdTree gTree;
    renderer gRenderer;
    client gClient;
    kdMap gMap;

#if 0
    gTree.load("map.obj");
    u::vector<unsigned char> data = gTree.serialize();
    u::vector<unsigned char> compress = u::compress(data);
    FILE *efp = fopen("maps/garden.kdgz", "w");
    fwrite(&*compress.begin(), compress.size(), 1, efp);
    fclose(efp);
    return 0;
#endif

    // clear the screen black
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    SDL_GL_SwapWindow(gScreen);

    GL_CHECK(glClearColor(0.4f, 0.6f, 0.9f, 0.0f));

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
    gRenderer.load(gMap);

    perspectiveProjection projection;
    projection.fov = 90.0f;
    projection.width = kScreenWidth;
    projection.height = kScreenHeight;
    projection.near = 1.0f;
    projection.far = 4096.0f;

    while (true) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        gClient.update(gMap);
        m::vec3 target;
        m::vec3 up;
        m::vec3 side;
        gClient.getDirection(&target, &up, &side);

        rendererPipeline pipeline;
        pipeline.setWorldPosition(0.0f, 0.0f, 0.0f);
        pipeline.setCamera(gClient.getPosition(), target, up);
        pipeline.setPerspectiveProjection(projection);

        gRenderer.draw(pipeline);

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
