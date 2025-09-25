#include "src/SearchEngine.h"
#include "src/Logger.h"
#include <iostream>

int main() {
    Logger::instance().set_log_file("test_search.log");
    LOG_INFO("=== Testing Search Engine ===");

    SearchEngine engine;
    std::vector<std::filesystem::path> paths = {"Content/Assets"};

    std::cout << "Testing search for 'test' in Content/Assets...\n";

    engine.search("test", paths,
        [](const std::string& msg, size_t current, size_t total) {
            std::cout << "Progress: " << msg << " (" << current << "/" << total << ")\n";
        },
        [](const SearchResult& result) {
            std::cout << "Found: " << result.file_path.filename() << " at line " << result.line_number << "\n";
        }
    );

    auto results = engine.get_results();
    std::cout << "Total results: " << results.size() << "\n";

    for (const auto& result : results) {
        std::cout << "- " << result.file_path.filename() << "\n";
    }

    return 0;
}