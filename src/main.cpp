#include "UI.h"
#include <iostream>
#include <exception>

int main() {
    try {
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

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}