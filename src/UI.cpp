#include "UI.h"
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <thread>
#include <chrono>
#include <filesystem>
#include <algorithm>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#endif

using namespace ftxui;

// Function to set clipboard content
void setClipboard(const std::string &text)
{
#ifdef _WIN32
    if (!OpenClipboard(nullptr))
        return;

    EmptyClipboard();

    HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
    if (hGlobal)
    {
        char *pGlobal = static_cast<char *>(GlobalLock(hGlobal));
        strcpy_s(pGlobal, text.size() + 1, text.c_str());
        GlobalUnlock(hGlobal);
        SetClipboardData(CF_TEXT, hGlobal);
    }

    CloseClipboard();
#else
    // For Linux/macOS, try xclip or pbcopy
    std::string command = "echo '" + text + "' | xclip -selection clipboard 2>/dev/null || echo '" + text + "' | pbcopy 2>/dev/null";
    system(command.c_str());
#endif
}

// Function to get clipboard content
std::string getClipboard()
{
#ifdef _WIN32
    if (!OpenClipboard(nullptr))
    {
        return "";
    }

    HANDLE hData = GetClipboardData(CF_TEXT);
    if (hData == nullptr)
    {
        CloseClipboard();
        return "";
    }

    char *pszText = static_cast<char *>(GlobalLock(hData));
    if (pszText == nullptr)
    {
        CloseClipboard();
        return "";
    }

    std::string text(pszText);
    GlobalUnlock(hData);
    CloseClipboard();
    return text;
#else
    // For Linux/macOS, try xclip or pbpaste
    FILE *pipe = popen("xclip -o -selection clipboard 2>/dev/null || pbpaste 2>/dev/null", "r");
    if (!pipe)
        return "";

    std::string result;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
    {
        result += buffer;
    }
    pclose(pipe);

    // Remove trailing newline if present
    if (!result.empty() && result.back() == '\n')
    {
        result.pop_back();
    }
    return result;
#endif
}

SearchAssetsUI::SearchAssetsUI() : screen_(ScreenInteractive::Fullscreen())
{
    search_engine_ = std::make_unique<SearchEngine>();
    // Initialize filtered results as empty
    filtered_result_lines_.clear();
    create_ui();
}

SearchAssetsUI::~SearchAssetsUI()
{
    if (search_engine_)
    {
        search_engine_->stop_search();
    }
}

void SearchAssetsUI::run()
{
    auto refresh_loop = std::thread([this]()
                                    {
        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (needs_refresh_) {
                screen_.PostEvent(Event::Custom);
                needs_refresh_ = false;
            }
        } });
    refresh_loop.detach();

    screen_.Loop(main_container_);
}

void SearchAssetsUI::create_ui()
{
    // Input components with paste support
    input_search_ = Input(&search_pattern_, "Enter search pattern... (press Enter to search)");
    input_search_ = CatchEvent(input_search_, [this](Event event)
                               {
        if (event == Event::F6) {
            std::string clipboard_content = getClipboard();
            if (!clipboard_content.empty()) {
                search_pattern_ += clipboard_content;
            }
            return true;
        }
        return false; });

    input_path_ = Input(&custom_path_, "Custom path (optional)...");
    input_path_ = CatchEvent(input_path_, [this](Event event)
                             {
        if (event == Event::F6) {
            std::string clipboard_content = getClipboard();
            if (!clipboard_content.empty()) {
                custom_path_ += clipboard_content;
            }
            return true;
        }
        return false; });

    input_filter_ = Input(&result_filter_, "Type to filter results...");

    input_min_size_ = Input(&min_file_size_str_, "Min size (KB)");
    input_max_size_ = Input(&max_file_size_str_, "Max size (KB)");

    checkbox_plugins_ = Checkbox("Search in Plugins/*/Content", &search_plugins_);
    checkbox_unreal_prefixes_ = Checkbox("Remove Unreal prefixes (A,U,F,S,T,E,I)", &remove_unreal_prefixes_);

    // Buttons
    button_search_ = Button("Search", [this]()
                            { perform_search(); });
    button_stop_ = Button("Stop", [this]()
                          { if (search_engine_) search_engine_->stop_search(); });
    button_clear_ = Button("Clear Results", [this]()
                           { reset_search(); });
    button_copy_selected_ = Button("Copy Selected", [this]()
                                   { copy_selected_result(); });
    button_copy_all_ = Button("Copy All Results", [this]()
                              { copy_all_results(); });

    // Results list - use filtered results with click handler
    results_list_ = Menu(&filtered_result_lines_, &selected_result_);

    // Main layout
    auto input_section = Container::Vertical({Container::Vertical({Container::Horizontal({Renderer([this]()
                                                                                                   { return text("Search Pattern:") | bold; }),
                                                                                          checkbox_unreal_prefixes_ | color(Color::Orange1)}),
                                                                   Renderer(input_search_, [this]()
                                                                            { return input_search_->Render() | border; })}),
                                              Renderer(input_path_, [this]()
                                                       { return vbox({text("Custom Path (leave empty for default Content/Assets):") | bold,
                                                                      input_path_->Render() | border}); }),
                                              Container::Horizontal({Renderer(input_min_size_, [this]()
                                                                              { return vbox({text("Min Size (KB):") | bold,
                                                                                             input_min_size_->Render() | border}); }),
                                                                     Renderer(input_max_size_, [this]()
                                                                              { return vbox({text("Max Size (KB):") | bold,
                                                                                             input_max_size_->Render() | border}); })}),
                                              checkbox_plugins_ | color(Color::Orange1)});

    // Filter section with copy button
    auto filter_section = Container::Vertical({Renderer([this]()
                                                        { return vbox({text("Filter Results:") | bold,
                                                                       text("Type to filter results in real-time") | dim | color(Color::Yellow)}); }),
                                               Renderer(input_filter_, [this]()
                                                        { return input_filter_->Render() | border; }),
                                               Container::Horizontal({button_search_,
                                                                      button_stop_,
                                                                      button_clear_,
                                                                      button_copy_selected_,
                                                                      button_copy_all_})});

    auto results_section = Renderer(results_list_, [this]()
                                    {
        // Update filtered results if filter changed
        static std::string last_filter = result_filter_;
        if (last_filter != result_filter_) {
            last_filter = result_filter_;
            update_filtered_results();
        }

        auto total_results = result_lines_.size();
        auto filtered_results = filtered_result_lines_.size();

        auto header = hbox({
            text("Results: ") | bold,
            text(std::to_string(filtered_results)) | color(Color::Green),
            text("/") | dim,
            text(std::to_string(total_results)) | dim,
            filler(),
            !last_copied_item_.empty() ?
                hbox({text("Copied: ") | color(Color::Green) | bold,
                      text(last_copied_item_) | color(Color::Yellow)}) :
                text(""),
            text("  ") | dim,
            text(progress_message_) | dim
        });

        if (is_searching_) {
            auto progress = progress_current_.load();
            auto total = progress_total_.load();

            if (total > 0) {
                float ratio = static_cast<float>(progress) / total;
                auto progress_bar = gauge(ratio) | size(WIDTH, EQUAL, 30);
                header = vbox({
                    header,
                    hbox({
                        text("Progress: "),
                        progress_bar,
                        text(" " + std::to_string(progress) + "/" + std::to_string(total))
                    })
                });
            }
        }

        if (total_results == 0 && !is_searching_) {
            return vbox({
                header | border,
                text("No results found. Try a different search pattern.") | center | dim | flex
            });
        }

        if (filtered_results == 0 && total_results > 0) {
            return vbox({
                header | border,
                text("No results match the current filter.") | center | dim | flex
            });
        }

        return vbox({
            header | border,
            separator(),
            results_list_->Render() | vscroll_indicator | frame | flex
        }); });

    main_container_ = Container::Vertical({input_section,
                                           filter_section,
                                           results_section});

    main_container_ = Renderer(main_container_, [this, input_section, filter_section, results_section]()
                               { return vbox({text("SEARCH ASSETS TOOL V2") | bold | center | color(Color::Cyan) | size(HEIGHT, EQUAL, 2),
                                              // text("Powered by 300 exc 2t six days a miscela al 3% perchè ho paura di grippare") | dim | center | color(Color::White),
                                              text("Enter/F5 to search | Ctrl + V/F6 to paste") | dim | center,
                                              separator(),
                                              input_section->Render() | size(HEIGHT, EQUAL, 14),
                                              separator(),
                                              filter_section->Render() | size(HEIGHT, EQUAL, 8),
                                              separator(),
                                              results_section->Render() | flex}) |
                                        border; });

    // Handle keyboard shortcuts
    main_container_ = CatchEvent(main_container_, [this](Event event)
                                 {
        if (event == Event::F5 || event == Event::Return) {
            perform_search();
            return true;
        }
        if (event == Event::Escape && is_searching_) {
            search_engine_->stop_search();
            return true;
        }
        if (event.is_character() && event.character() == "q" && !is_searching_) {
            screen_.ExitLoopClosure()();
            return true;
        }
        return false; });
}

void SearchAssetsUI::perform_search()
{
    if (is_searching_)
    {
        return;
    }

    if (search_pattern_.empty())
    {
        return;
    }

    // Update file size limits from UI
    try
    {
        double min_kb = std::stod(min_file_size_str_);
        double max_kb = std::stod(max_file_size_str_);
        size_t min_bytes = static_cast<size_t>(min_kb * 1024);
        size_t max_bytes = static_cast<size_t>(max_kb * 1024);
        search_engine_->set_file_size_limits(min_bytes, max_bytes);
    }
    catch (const std::exception &)
    {
        // Use default values if parsing fails
    }

    // Sanitize search pattern if Unreal prefix removal is enabled
    std::string actual_search_pattern = search_pattern_;
    if (remove_unreal_prefixes_)
    {
        actual_search_pattern = remove_unreal_prefix(search_pattern_);
    }

    reset_search();
    is_searching_ = true;

    std::vector<std::filesystem::path> search_paths;

    // Determine search paths
    if (!custom_path_.empty())
    {
        search_paths.push_back(std::filesystem::path(custom_path_));
    }
    else
    {
        // Default Content/Assets path
        if (std::filesystem::exists("Content/Assets"))
        {
            search_paths.push_back(std::filesystem::path("Content/Assets"));
        }

        // Add plugin paths if enabled
        if (search_plugins_)
        {
            try
            {
                if (std::filesystem::exists("Plugins"))
                {
                    size_t plugin_count = 0;
                    for (const auto &plugin_dir : std::filesystem::directory_iterator("Plugins"))
                    {
                        if (plugin_dir.is_directory())
                        {
                            auto content_path = plugin_dir.path() / "Content";
                            if (std::filesystem::exists(content_path))
                            {
                                search_paths.push_back(content_path);
                                plugin_count++;
                            }
                        }
                    }
                }
            }
            catch (const std::filesystem::filesystem_error &)
            {
                // Skip if can't access Plugins directory
            }
        }
    }

    if (search_paths.empty())
    {
        is_searching_ = false;
        update_progress("No search paths available", 0, 0);
        return;
    }

    // Start search in separate thread
    std::thread search_thread([this, search_paths, actual_search_pattern]()
                              {
        search_engine_->search(
            actual_search_pattern,
            search_paths,
            [this](const std::string& message, size_t current, size_t total) {
                update_progress(message, current, total);
            },
            [this](const SearchResult& result) {
                add_result(result);
            }
        );
        is_searching_ = false;
        needs_refresh_ = true; });
    search_thread.detach();
}

void SearchAssetsUI::reset_search()
{
    if (search_engine_)
    {
        search_engine_->stop_search();
        search_engine_->clear_results();
    }

    {
        std::lock_guard<std::mutex> lock(results_mutex_);
        result_lines_.clear();
        filtered_result_lines_.clear();
        selected_result_ = 0;
    }

    result_filter_.clear();
    last_copied_item_.clear();

    progress_message_ = "";
    progress_current_ = 0;
    progress_total_ = 0;
    is_searching_ = false;
    needs_refresh_ = true;
}

void SearchAssetsUI::update_progress(const std::string &message, size_t current, size_t total)
{
    progress_message_ = message;
    progress_current_ = current;
    progress_total_ = total;
    needs_refresh_ = true;
}

void SearchAssetsUI::add_result(const SearchResult &result)
{
    std::lock_guard<std::mutex> lock(results_mutex_);

    // Show only the filename (without path)
    std::string filename = result.file_path.filename().string();
    // Check if filename already exists in results to avoid duplicates
    if (std::find(result_lines_.begin(), result_lines_.end(), filename) == result_lines_.end())
    {
        result_lines_.push_back(filename);
        // Update filtered results inline to avoid double locking
        if (result_filter_.empty())
        {
            filtered_result_lines_ = result_lines_;
        }
        else
        {
            filtered_result_lines_.clear();
            std::string filter_lower = result_filter_;
            std::transform(filter_lower.begin(), filter_lower.end(), filter_lower.begin(), ::tolower);

            for (const auto &line : result_lines_)
            {
                std::string line_lower = line;
                std::transform(line_lower.begin(), line_lower.end(), line_lower.begin(), ::tolower);

                if (line_lower.find(filter_lower) != std::string::npos)
                {
                    filtered_result_lines_.push_back(line);
                }
            }
        }
        needs_refresh_ = true;
    }
}

void SearchAssetsUI::update_filtered_results()
{
    std::lock_guard<std::mutex> lock(results_mutex_);
    filtered_result_lines_.clear();

    if (result_filter_.empty())
    {
        filtered_result_lines_ = result_lines_;
    }
    else
    {
        std::string filter_lower = result_filter_;
        std::transform(filter_lower.begin(), filter_lower.end(), filter_lower.begin(), ::tolower);

        for (const auto &result : result_lines_)
        {
            std::string result_lower = result;
            std::transform(result_lower.begin(), result_lower.end(), result_lower.begin(), ::tolower);

            if (result_lower.find(filter_lower) != std::string::npos)
            {
                filtered_result_lines_.push_back(result);
            }
        }
    }

    if (selected_result_ >= static_cast<int>(filtered_result_lines_.size()))
    {
        selected_result_ = 0;
    }

    needs_refresh_ = true;
}

std::string SearchAssetsUI::remove_unreal_prefix(const std::string &filename)
{
    if (filename.length() < 2)
        return filename;

    // Get the base name without extension first
    std::string basename = filename;
    size_t dot_pos = basename.find_last_of('.');
    std::string extension = "";
    if (dot_pos != std::string::npos)
    {
        extension = basename.substr(dot_pos);
        basename = basename.substr(0, dot_pos);
    }

    // Check if it starts with Unreal prefixes
    char first_char = basename[0];
    if (basename.length() > 1 &&
        (first_char == 'A' || first_char == 'U' || first_char == 'F' ||
         first_char == 'S' || first_char == 'T' || first_char == 'E' ||
         first_char == 'I') &&
        std::isupper(basename[1]))
    { // Second char should be uppercase for valid Unreal class
        return basename.substr(1) + extension;
    }

    return filename;
}

void SearchAssetsUI::copy_selected_result()
{
    std::lock_guard<std::mutex> lock(results_mutex_);

    if (filtered_result_lines_.empty() ||
        selected_result_ < 0 ||
        selected_result_ >= static_cast<int>(filtered_result_lines_.size()))
    {
        last_copied_item_ = "No result selected";
        needs_refresh_ = true;
        return;
    }

    std::string selected_item = filtered_result_lines_[selected_result_];

    // Remove file extension
    size_t dot_pos = selected_item.find_last_of('.');
    if (dot_pos != std::string::npos)
    {
        selected_item = selected_item.substr(0, dot_pos);
    }

    setClipboard(selected_item);
    last_copied_item_ = selected_item;
    needs_refresh_ = true;
}

void SearchAssetsUI::copy_all_results()
{
    std::lock_guard<std::mutex> lock(results_mutex_);

    if (filtered_result_lines_.empty())
    {
        last_copied_item_ = "No results to copy";
        needs_refresh_ = true;
        return;
    }

    // Create a formatted string with all results
    std::stringstream ss;
    ss << "Search Results (" << filtered_result_lines_.size() << " items):\n";
    ss << "====================================\n";

    for (size_t i = 0; i < filtered_result_lines_.size(); ++i)
    {
        ss << (i + 1) << ". " << filtered_result_lines_[i] << "\n";
    }

    std::string all_results = ss.str();
    setClipboard(all_results);

    last_copied_item_ = std::to_string(filtered_result_lines_.size()) + " results copied to clipboard";
    needs_refresh_ = true;
}