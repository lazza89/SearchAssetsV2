cmake_minimum_required(VERSION 3.20)
project(SearchAssets VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(Threads REQUIRED)

# Include FetchContent for downloading ftxui
include(FetchContent)

# Fetch ftxui
FetchContent_Declare(
    ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
    GIT_TAG v5.0.0
)
FetchContent_MakeAvailable(ftxui)

# Create executable
add_executable(SearchAssets
    src/main.cpp
    src/SearchEngine.cpp
    src/SearchEngine.h
    src/UI.cpp
    src/UI.h
    src/Logger.h
)

# Link libraries
target_link_libraries(SearchAssets
    PRIVATE
    ftxui::screen
    ftxui::dom
    ftxui::component
    Threads::Threads
)

# Include directories
target_include_directories(SearchAssets PRIVATE src)

# Compiler-specific options
if(MSVC)
    target_compile_options(SearchAssets PRIVATE /W4)
else()
    target_compile_options(SearchAssets PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Create src directory if it doesn't exist
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src)