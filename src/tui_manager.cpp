#include "tui_manager.h"
#include "ftxui/component/component.hpp"
#include "ftxui/component/component_base.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/dom/node.hpp"
#include "ftxui/dom/table.hpp"
#include "ftxui/screen/screen.hpp"

namespace atem {

using namespace ftxui;

TuiManager::TuiManager(Config& config)
    : m_config(config)
    , m_screen(ScreenInteractive::Fullscreen())
{
    // Initialize tally states based on config
    std::lock_guard<std::mutex> lock(m_mutex);
    m_tally_states.resize(m_config.mock_inputs);
    for (uint16_t i = 0; i < m_config.mock_inputs; ++i) {
        m_tally_states[i].input_id = i + 1;
    }
    m_editable_atem_ip = m_config.atem_ip;
    m_is_mock_mode = m_config.mock_enabled;

    // --- Component Setup ---
    auto tally_grid_renderer = Renderer([this] {
        Elements rows;
        Elements row;
        std::vector<TallyState> states_copy;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            states_copy = m_tally_states;
        }

        for (const auto& state : states_copy) {
            auto box_label = text(std::to_string(state.input_id)) | bold | center;
            auto box_element = vbox({ box_label }) | center | size(WIDTH, EQUAL, 10) | size(HEIGHT, EQUAL, 3);

            if (state.program) {
                box_element |= bgcolor(Color::Red) | color(Color::White);
            } else if (state.preview) {
                box_element |= bgcolor(Color::Green) | color(Color::Black);
            } else {
                box_element |= bgcolor(Color::GrayDark);
            }

            row.push_back(box_element);

            if (row.size() == m_boxes_per_row) {
                rows.push_back(hbox(row));
                row.clear();
            }
        }
        if (!row.empty()) {
            rows.push_back(hbox(row));
        }

        return vbox(rows) | center;
    });

    m_main_component = tally_grid_renderer;

    // Create the config modal component once.
    m_config_modal_component = render_config_modal();
}

void TuiManager::update_tally_state(const TallyUpdate& update)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (update.input_id > 0 && static_cast<size_t>(update.input_id) <= m_tally_states.size()) {
        m_tally_states[update.input_id - 1].program = update.program;
        m_tally_states[update.input_id - 1].preview = update.preview;
        // Post an event to redraw the screen
        m_screen.PostEvent(Event::Custom);
    }
}

void TuiManager::set_mock_mode(bool is_mock)
{
    if (m_is_mock_mode.exchange(is_mock) != is_mock) {
        // Post an event to redraw the screen if the state changed
        m_screen.PostEvent(Event::Custom);
    }
}

void TuiManager::on_reconnect(ReconnectCallback callback)
{
    m_reconnect_callback = std::move(callback);
}

void TuiManager::run()
{
    // --- Component Composition ---
    auto main_layout = Renderer(m_main_component, [&] {
        auto title = m_is_mock_mode.load()
            ? hbox({ text("ATEM Tally Server "), text("(MOCK MODE)") | bold | color(Color::Yellow) })
            : text("ATEM Tally Server");

        return vbox({
                   title | bold | hcenter,
                   separator(),
                   m_main_component->Render() | flex,
                   separator(),
                   render_status_panel(),
                   separator(),
                   text("Press 'c' for config, 'q' to quit.") | center,
               })
            | border;
    });

    // Add global event handling
    auto event_handler = CatchEvent([&](Event event) {
        if (event == Event::Character('q')) {
            stop();
            return true;
        }
        if (event == Event::Character('c')) {
            // When opening the modal, ensure the editable IP is up-to-date.
            if (!m_show_config_modal) {
                m_editable_atem_ip = m_config.atem_ip;
            }
            m_show_config_modal = !m_show_config_modal;
            return true;
        }
        return false;
    });

    // The Modal component wraps the main layout and is controlled by m_show_config_modal.
    // The event handler wraps the modal to catch global events like 'q' and 'c'.
    auto root_component = Modal(main_layout, m_config_modal_component, &m_show_config_modal);
    m_screen.Loop(root_component | event_handler);
}

void TuiManager::stop()
{
    m_screen.Exit();
}

Element TuiManager::render_status_panel()
{
    auto status_table = Table({ { "ATEM IP", m_config.atem_ip },
        { "WebSocket", m_config.ws_address + ":" + std::to_string(m_config.ws_port) },
        { "Mock Mode", m_is_mock_mode.load() ? "Enabled" : "Disabled" } });

    status_table.SelectAll().Border(LIGHT);
    status_table.SelectColumn(0).Decorate(bold);

    return status_table.Render() | flex;
}

Component TuiManager::render_config_modal()
{
    auto on_save = [&] {
        m_config.atem_ip = m_editable_atem_ip;
        m_show_config_modal = false;
        if (m_reconnect_callback) {
            m_reconnect_callback();
        }
    };

    auto on_cancel = [&] {
        m_editable_atem_ip = m_config.atem_ip; // Revert changes
        m_show_config_modal = false;
    };

    auto ip_input = Input(&m_editable_atem_ip, "ATEM IP");
    auto save_button = Button("Save", on_save);
    auto cancel_button = Button("Cancel", on_cancel);

    auto container = Container::Vertical({
        ip_input,
        Container::Horizontal({
            save_button,
            cancel_button,
        }),
    });

    return Renderer(container, [&] {
        return vbox({ text("Configuration") | bold,
                   separator(),
                   hbox(text(" ATEM IP: "), ip_input->Render()),
                   separator(),
                   hbox(save_button->Render(), cancel_button->Render()) | center })
            | border;
    });
}

} // namespace atem
