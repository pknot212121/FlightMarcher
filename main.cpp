#include "application.h"

Application* g_app = nullptr;

int main()
{
    g_app = new Application();
    if (!g_app->initialize())
        return 1;
    return 0;
}