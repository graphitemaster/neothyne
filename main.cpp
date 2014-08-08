#include <SDL2/SDL.h>

#include "kdtree.h"
#include "kdmap.h"
#include "renderer.h"

static size_t      gScreenWidth  = 1024;
static size_t      gScreenHeight = 768;
static SDL_Window *gScreen       = nullptr;

struct camera {
    static constexpr float kStepScale = 0.9f;

    void fromKeyboard(SDL_Keycode key) {
        m::vec3 left;
        m::vec3 right;
        switch (key) {
            case SDLK_w:
                m_position += (m_target * kStepScale);
                break;
            case SDLK_s:
                m_position -= (m_target * kStepScale);
                break;
            case SDLK_a:
                left = m_target.cross(m_up);
                left.normalize();
                left *= kStepScale;
                m_position += left;
                break;
            case SDLK_d:
                right = m_up.cross(m_target);
                right.normalize();
                right *= kStepScale;
                m_position += right;
                break;
        }
    }

    void fromMouse(int x, int y) {
        const int deltaX = x - m_mouseX;
        const int deltaY = y - m_mouseY;

        if (deltaX == 0 && deltaY == 0)
            return;

        m_angleH += (float)deltaX / 20.0f;
        m_angleV += (float)deltaY / 20.0f;

        update();

        SDL_WarpMouseInWindow(gScreen, m_windowWidth / 2, m_windowHeight / 2);
    }

    const m::vec3 &getPosition(void) const { return m_position; }
    const m::vec3 &getTarget(void) const { return m_target; }
    const m::vec3 &getUp(void) const { return m_up; }

    void init(size_t width, size_t height) {
        m_windowWidth = width;
        m_windowHeight = height;
        m_target = m::vec3(0.0f, 0.0f, 1.0f);
        m_up = m::vec3(0.0f, 1.0f, 0.0f);
        m_target.normalize();

        m::vec3 target(m_target.x, 0.0f, m_target.z);
        target.normalize();
        if (target.z >= 0.0f)
            m_angleH = (target.x >= 0.0f)
                ? 360.0f - m::toDegree(asinf(target.z))
                : 180.0f + m::toDegree(asinf(target.z));
        else
            m_angleH = (target.x >= 0.0f)
                ? m::toDegree(asinf(-target.z))
                : 90.0f + m::toDegree(asinf(-target.z));
        m_angleV = -m::toDegree(asinf(m_target.y));
        m_mouseX = m_windowWidth / 2;
        m_mouseY = m_windowHeight / 2;

        SDL_WarpMouseInWindow(gScreen, m_mouseX, m_mouseY);
    }

private:
    void update(void) {
        const m::vec3 vaxis(0.0f, 1.0f, 0.0f);

        // rotate view vector by horizontal angle around vertical axis
        m::vec3 view(1.0f, 0.0f, 0.0f);
        view.rotate(m_angleH, vaxis);
        view.normalize();

        // rotate view vector by vertical angle around horizontal axis
        m::vec3 haxis = vaxis.cross(view);
        haxis.normalize();
        view.rotate(m_angleV, haxis);

        m_target = view;
        m_target.normalize();
        m_up = m_target.cross(haxis);
        m_up.normalize();
    }

    m::vec3 m_position;
    m::vec3 m_target;
    m::vec3 m_up;
    size_t m_windowWidth;
    size_t m_windowHeight;
    float m_angleH;
    float m_angleV;
    int m_mouseX;
    int m_mouseY;
};

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
    camera gCamera; // controller

    gCamera.init(gScreenWidth, gScreenHeight);

    printf("Loading OBJ into KD-tree (this may take awhile)\n");
    gTree.load("test.obj");
    gRenderer.load(gTree.serialize());

    // go go go!
    while (true) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        rendererPipeline pipeline;
        pipeline.position(0.0f, 0.0f, 3.0f);
        pipeline.setCamera(gCamera.getPosition(), gCamera.getTarget(), gCamera.getUp());
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
                    gCamera.fromKeyboard(e.key.keysym.sym);
                    break;

                case SDL_MOUSEMOTION:
                    gCamera.fromMouse(e.motion.x, e.motion.y);
                    break;
            }
        }
    }
    return 0;
}
