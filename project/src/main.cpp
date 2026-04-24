#include "Core/Application.h"

int main()
{
    Application app;

    if (!app.Init())
        return -1;
    app.Run();
    return 0;
}
