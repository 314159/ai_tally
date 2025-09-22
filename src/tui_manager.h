#pragma once

#include "config.h"
#include "ftxui/component/component.hpp"
#include "tally_state.h"
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <memory>
#include <mutex>
#include <vector>

namespace atem {

class TuiManager {
public:
    TuiManager(Config& config);

    // Update the tally state displayed in the TUI. Thread-safe.
    void update_tally_state(const TallyUpdate& update);

    // Run the TUI event loop. This is a blocking call.
    void run();

    // Stop the TUI event loop. Thread-safe.
    void stop();

private:
    // Renders the status and info panel.
    ftxui::Element render_status_panel();

    // Renders the configuration modal window.
    ftxui::Component render_config_modal();

    Config& m_config;
    ftxui::ScreenInteractive m_screen;

    std::mutex m_mutex;
    std::vector<TallyState> m_tally_states;
    size_t m_boxes_per_row = 4;

    ftxui::Component m_main_component;

    bool m_show_config_modal = false;
    std::string m_editable_atem_ip;
};

} // namespace atem
