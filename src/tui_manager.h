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
    using ReconnectCallback = std::function<void()>;

    TuiManager(Config& config);

    // Update the tally state displayed in the TUI. Thread-safe.
    void update_tally_state(const TallyUpdate& update);

    // Update the mock mode status. Thread-safe.
    void set_mock_mode(bool is_mock);

    // Run the TUI event loop. This is a blocking call.
    void run();

    // Stop the TUI event loop. Thread-safe.
    void stop();

    // Set a callback to be invoked when a reconnect is requested.
    void on_reconnect(ReconnectCallback callback);

private:
    // Renders the status and info panel.
    ftxui::Element render_status_panel();

    // Renders the configuration modal window.
    ftxui::Component render_config_modal();

    ReconnectCallback m_reconnect_callback;

    Config& m_config;
    ftxui::ScreenInteractive m_screen;

    std::mutex m_mutex;
    std::vector<TallyState> m_tally_states;
    size_t m_boxes_per_row = 4;

    ftxui::Component m_main_component;

    bool m_show_config_modal = false;
    std::string m_editable_atem_ip;
    std::atomic<bool> m_is_mock_mode = false;
    ftxui::Component m_config_modal_component;
};

} // namespace atem
