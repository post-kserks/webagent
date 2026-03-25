#include "screamer_open.h"
#include <cstdlib>

#ifdef _WIN32
    #include <windows.h>
#elif __APPLE__
    #include <TargetConditionals.h>
#endif

void zapusk_exe() {
    #ifdef _WIN32
        ShellExecuteA(NULL, "open", "screamer.mp4", NULL, NULL, SW_SHOWNORMAL);
    #elif __APPLE__
        system("open screamer.mp4 2>/dev/null &");
    #else
        system("xdg-open screamer.mp4 2>/dev/null &");
    #endif
}