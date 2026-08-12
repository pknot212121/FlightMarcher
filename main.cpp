#include "application.h"

Application* g_app = nullptr;

int main()
{
    g_app = new Application();
    if (!g_app->initialize())
        return 1;

    #ifndef __EMSCRIPTEN__
    g_app->mainLoop();
    g_app->terminate();
    delete g_app;
    #endif

    return 0;
}