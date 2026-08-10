#include "application.h"

int main()
{
    Application app;
    app.initialize();
    app.mainLoop();
    app.terminate();
}