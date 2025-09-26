#include "UI.h"
#include <iostream>
#include <exception>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

int main()
{
    try
    {
#ifdef _WIN32
        // Set console window size (width, height)
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(hConsole, &csbi);

        // Set buffer size (wider for scrollback)
        COORD bufferSize = {120, 4000}; // width: 120, height: 3000 lines
        SetConsoleScreenBufferSize(hConsole, bufferSize);

        // Set window size (what's visible)
        SMALL_RECT windowSize = {0, 0, 119, 45}; // width: 120, height: 36 lines
        SetConsoleWindowInfo(hConsole, TRUE, &windowSize);

        // Set window title
        SetConsoleTitle("SearchAssetsV2 - Unreal Engine Asset Search Tool");
#endif

        SearchAssetsUI ui;

        std::cout << "Starting Asset Search Tool...\n";
        std::cout << "Keyboard shortcuts:\n";
        std::cout << "  F5: Start search\n";
        std::cout << "  F6: Paste clipboard\n";
        std::cout << "  Escape: Stop search\n";
        std::cout << "  q: Quit application\n\n";

        ui.run();

        std::cout << "Thank you for using Asset Search Tool!\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}