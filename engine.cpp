#include "engine.h"
#include "r_common.h"

static constexpr size_t kDefaultScreenWidth = 1024;
static constexpr size_t kDefaultScreenHeight = 768;

static u::map<int, int> gKeyMap;
static size_t gScreenWidth = 0;
static size_t gScreenHeight = 0;
static SDL_Window *gScreen = nullptr;

u::map<int, int> &neoKeyState(int key, bool keyDown, bool keyUp) {
    if (keyDown)
        gKeyMap[key]++;
    if (keyUp)
        gKeyMap[key] = 0;
    return gKeyMap;
}

void neoMouseDelta(int *deltaX, int *deltaY) {
    SDL_GetRelativeMouseState(deltaX, deltaY);
}

void neoSwap(void) {
    SDL_GL_SwapWindow(gScreen);
}

size_t neoWidth(void) {
    return gScreenWidth;
}

size_t neoHeight(void) {
    return gScreenHeight;
}

void neoToggleRelativeMouseMode(void) {
    SDL_SetRelativeMouseMode(
        SDL_GetRelativeMouseMode() == SDL_TRUE
            ? SDL_FALSE : SDL_TRUE);
}

void neoSetWindowTitle(const char *title) {
    SDL_SetWindowTitle(gScreen, title);
}

void neoResize(size_t width, size_t height) {
    SDL_SetWindowSize(gScreen, width, height);
    gScreenWidth = width;
    gScreenHeight = height;
    gl::Viewport(0, 0, width, height);
}

static SDL_Window *getContext(void) {
    SDL_Init(SDL_INIT_VIDEO);

    // Get the display mode resolution
    SDL_DisplayMode mode;
    if (SDL_GetDesktopDisplayMode(0, &mode) != 0) {
        // Failed to get the desktop mode
        int displays = SDL_GetNumVideoDisplays();
        if (displays < 1) {
            // Failed to even get a display, worst case we'll assume a default
            gScreenWidth = kDefaultScreenWidth;
            gScreenHeight = kDefaultScreenHeight;
        } else {
            // Try all the display devices until we find one that actually contains
            // display information
            for (int i = 0; i < displays; i++) {
                if (SDL_GetCurrentDisplayMode(i, &mode) != 0)
                    continue;
                // Found a display mode
                gScreenWidth = mode.w;
                gScreenHeight = mode.h;
            }
        }
    } else {
        // Use the desktop display mode
        gScreenWidth = mode.w;
        gScreenHeight = mode.h;
    }

    if (gScreenWidth <= 0 || gScreenHeight <= 0) {
        // In this event we fall back to the default resolution
        gScreenWidth = kDefaultScreenWidth;
        gScreenHeight = kDefaultScreenHeight;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
        SDL_GL_CONTEXT_PROFILE_CORE | SDL_GL_CONTEXT_DEBUG_FLAG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_Window *window = SDL_CreateWindow("Neothyne",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        gScreenWidth,
        gScreenHeight,
        SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
    );

    if (!window || !SDL_GL_CreateContext(window)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
            "Neothyne: Initialization error",
            "OpenGL 3.3 or higher is required",
            nullptr);
        SDL_DestroyWindow(window);
        SDL_Quit();
    }

    SDL_SetRelativeMouseMode(SDL_TRUE);
    SDL_GL_SetSwapInterval(0); // Disable vsync

    return window;
}

// So we don't need to depend on SDL_main we provide our own
#ifdef _WIN32
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR szCmdLine, int sw) {
    auto parseCommandLine = [](const char *src, std::vector<char *> &args) {
            char *buf = new char[strlen(src) + 1], *dst = buf;
            for (;;) {
                while (isspace(*src))
                    src++;
                if (!*src)
                    break;
                args.push_back(dst);
                for (bool quoted = false; *src && (quoted || !isspace(*src)); src++)
                    if (*src != '"')
                        *dst++ = *src;
                    else if (dst > buf && src[-1] == '\\')
                        dst[-1] = '"';
                    else
                        quoted = !quoted;
                }
                *dst++ = '\0';
            }
        args.push_back(NULL);
        return buf;
    };
    u::vector<char *> args;
    char *buf = parseCommandLine(GetCommandLine(), args);
    SDL_SetMainReady();
    int status = SDL_main(args.size() - 1, &args[0]);
    delete[] buf;
    exit(status);
    return 0;
}
#endif

///
/// On Window the entry point is entered as such:
///     WinMain -> SDL_main -> main -> neoMain
/// For everywhere else:
///     main -> neoMain
///
extern int neoMain(int argc, char **argv);
int main(int argc, char **argv) {
    gScreen = getContext();
    gl::init();
    gl::ClearColor(0.0f, 0.0f, 0.0f, 0.0f);

    // back face culling
    gl::FrontFace(GL_CW);
    gl::CullFace(GL_BACK);
    gl::Enable(GL_CULL_FACE);

    const char *vendor = (const char *)gl::GetString(GL_VENDOR);
    const char *renderer = (const char *)gl::GetString(GL_RENDERER);
    const char *version = (const char *)gl::GetString(GL_VERSION);
    const char *shader = (const char *)gl::GetString(GL_SHADING_LANGUAGE_VERSION);

    printf("Vendor: %s\nRenderer: %s\nDriver: %s\nShading: %s\n",
        vendor, renderer, version, shader);

    return neoMain(argc, argv);
}
