#include "application.h"

int main()
{
    auto app = std::make_unique<Application>();
    app->initialize();
    app->mainLoop();
    app->terminate();
}