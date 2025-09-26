#pragma once

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <memory>
#include <string>
#include <vector>
#include <atomic>
#include <mutex>

#include "SearchEngine.h"

class SearchAssetsUI
{
public:
    SearchAssetsUI();
    ~SearchAssetsUI();

    void run();

private:
    void create_ui();
    void update_progress(const std::string &message, size_t current, size_t total);
    void add_result(const SearchResult &result);
    void perform_search();
    void reset_search();
    void update_filtered_results();
    void copy_selected_result();
    void copy_all_results();
    std::string remove_unreal_prefix(const std::string& filename);

    // UI State
    std::string search_pattern_;
    std::string custom_path_;
    std::string result_filter_{""};
    bool search_plugins_{false};
    bool remove_unreal_prefixes_{true};

    // File size limits (in KB for easier UI)
    std::string min_file_size_str_{"0.1"}; // 100 bytes = 0.1 KB
    std::string max_file_size_str_{"1000"};

    // Search state
    std::atomic<bool> is_searching_{false};
    std::string progress_message_;
    std::atomic<size_t> progress_current_{0};
    std::atomic<size_t> progress_total_{0};

    // Results
    mutable std::mutex results_mutex_;
    std::vector<std::string> result_lines_;
    std::vector<std::string> filtered_result_lines_;
    int selected_result_{0};
    std::string last_copied_item_;

    // Components
    ftxui::Component main_container_;
    ftxui::Component input_search_;
    ftxui::Component input_path_;
    ftxui::Component input_filter_;
    ftxui::Component input_min_size_;
    ftxui::Component input_max_size_;
    ftxui::Component checkbox_plugins_;
    ftxui::Component checkbox_unreal_prefixes_;
    ftxui::Component button_search_;
    ftxui::Component button_stop_;
    ftxui::Component button_clear_;
    ftxui::Component button_copy_selected_;
    ftxui::Component button_copy_all_;
    ftxui::Component results_list_;

    ftxui::ScreenInteractive screen_;
    std::unique_ptr<SearchEngine> search_engine_;

    // UI refresh
    std::atomic<bool> needs_refresh_{false};
};