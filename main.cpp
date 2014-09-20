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

struct frameTimer {
    frameTimer() :
        m_deltaTime(0.0f),
        m_lastFrameTicks(0),
        m_currentTicks(0),
        m_frameAverage(0.0f),
        m_framesPerSecond(0)
    {
        reset();
        cap(kMaxFPS);
    }

    static constexpr size_t kMaxFPS = 0; // For capping framerate (0 = disabled)
    static constexpr float kDampenEpsilon = 0.00001f; // The dampening to remove flip-flip in frame metrics

    void cap(float maxFps) {
        m_maxFrameTicks = maxFps <= 0.0f
            ? -1 : (1000.0f / maxFps) - kDampenEpsilon;
    }

    void reset(void) {
        m_frameCount = 0;
        m_minTicks = 1000;
        m_maxTicks = 0;
        m_averageTicks = 0;
        m_lastSecondTicks = SDL_GetTicks();
    }

    bool update(void) {
        m_frameCount++;
        m_targetTicks = m_maxFrameTicks != -1 ?
            m_lastSecondTicks + uint32_t(m_frameCount * m_maxFrameTicks) : 0;
        m_currentTicks = SDL_GetTicks();
        m_averageTicks += m_currentTicks - m_lastFrameTicks;
        if (m_currentTicks - m_lastFrameTicks <= m_minTicks)
            m_minTicks = m_currentTicks - m_lastFrameTicks;
        if (m_currentTicks - m_lastFrameTicks >= m_maxTicks)
            m_maxTicks = m_currentTicks - m_lastFrameTicks;
        if (m_targetTicks && m_currentTicks < m_targetTicks) {
            uint32_t beforeDelay = SDL_GetTicks();
            SDL_Delay(m_targetTicks - m_currentTicks);
            m_currentTicks = SDL_GetTicks();
            m_averageTicks += m_currentTicks - beforeDelay;
        }
        m_deltaTime = 0.001f * (m_currentTicks - m_lastFrameTicks);
        m_lastFrameTicks = m_currentTicks;
        if (m_currentTicks - m_lastSecondTicks >= 1000) {
            m_framesPerSecond = m_frameCount;
            m_frameAverage = m_averageTicks / m_frameCount;
            m_frameMin = m_minTicks;
            m_frameMax = m_maxTicks;
            reset();
            return true;
        }
        return false;
    }

    float mspf(void) const {
        return m_frameAverage;
    }

    int fps(void) const {
        return m_framesPerSecond;
    }

    float delta(void) const {
        return m_deltaTime;
    }

    uint32_t ticks(void) const {
        return m_currentTicks;
    }

private:
    float m_maxFrameTicks;
    uint32_t m_lastSecondTicks;
    int m_frameCount;
    uint32_t m_minTicks;
    uint32_t m_maxTicks;
    float m_averageTicks;
    float m_deltaTime;
    uint32_t m_lastFrameTicks;
    uint32_t m_currentTicks;
    uint32_t m_targetTicks;
    uint32_t m_frameMin;
    uint32_t m_frameMax;
    float m_frameAverage;
    int m_framesPerSecond;
};

int neoMain(int argc, char **argv) {
    argc--;
    argv++;
    if (argc == 2)
        neoResize(atoi(argv[0]), atoi(argv[1]));

    frameTimer gTimer;

    r::splashScreen gSplash;
    gSplash.load("textures/logo");
    gSplash.upload();

    r::rendererPipeline pipeline;
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
    gTimer.cap(30); // Loading screen at 30fps
    while (SDL_AtomicGet(&loadData.done) == kLoadInProgress) {
        glClear(GL_COLOR_BUFFER_BIT);
        pipeline.setTime(gTimer.ticks() / 500.0f);
        gSplash.render(pipeline);
        neoSwap();
        gTimer.update();
    }

    if (SDL_AtomicGet(&loadData.done) != kLoadSucceeded)
        return 1;

    // we're done loading the resources, now upload them (we can't render a loading
    // screen when uploading so we assume the process will be sufficently fast.)
    loadData.gWorld.upload(projection);

    // Now render the world
    client gClient;

    gTimer.cap(0); // don't cap the world
    bool running = true;
    while (running) {
        neoSetWindowTitle(u::format("Neothyne: %d fps : %.2f mspf",
            gTimer.fps(), gTimer.mspf()).c_str());

        gClient.update(loadData.gMap, gTimer.delta());

        pipeline.setRotation(gClient.getRotation());
        pipeline.setPosition(gClient.getPosition());
        pipeline.setTime(gTimer.ticks());

        loadData.gWorld.render(pipeline);

        neoSwap();
        gTimer.update();

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
