#include "config.h"

#include "atem/atem_connection_mock.h"
#include "atem/atem_connection_real.h"
#include "tally_monitor.h"
#include <chrono>
#include <iostream>

using namespace std::chrono_literals;

namespace atem {

TallyMonitor::TallyMonitor(boost::asio::io_context& ioc, const Config& config)
    : ioc_(ioc)
    , config_(config)
    , monitor_timer_(std::make_unique<boost::asio::steady_timer>(ioc))
{
    if (config_.mock_enabled) {
        atem_connection_ = std::make_unique<ATEMConnectionMock>(ioc_, config_.mock_inputs);
    } else {
        atem_connection_ = std::make_unique<ATEMConnectionReal>();
    }
}

TallyMonitor::~TallyMonitor()
{
    stop();
}

void TallyMonitor::start()
{
    if (running_.exchange(true)) {
        return; // Already running
    }

    std::cout << "Starting ATEM tally monitor...\n";

    // Initialize ATEM connection
    if (!atem_connection_->connect(config_.atem_ip)) {
        if (config_.use_mock_automatically) {
            std::cout << "Warning: Could not connect to ATEM switcher. Using mock data automatically.\n";
            // If connection fails, replace the connection object with a mock one and connect it.
            atem_connection_ = std::make_unique<ATEMConnectionMock>(ioc_, config_.mock_inputs);
            atem_connection_->connect(config_.atem_ip); // This starts the mock timer
            notify_mode_change(true);
        } else {
            std::cerr << "Error: Could not connect to ATEM switcher. Automatic mock fallback is disabled.\n";
            // No connection is made, the server will show a disconnected state.
        }
    }
    if (ready_callback_) {
        ready_callback_();
    }

    // Set up tally callback
    atem_connection_->on_tally_change([this](const TallyUpdate& update) { handle_tally_change(update); });

    // Start monitoring loop
    poll_atem();
}

void TallyMonitor::stop()
{
    if (!running_.exchange(false, std::memory_order_acq_rel)) {
        return; // Already stopped
    }

    std::cout << "Stopping ATEM tally monitor...\n";

    if (monitor_timer_) {
        monitor_timer_->cancel();
    }

    if (atem_connection_) {
        atem_connection_->disconnect();
    }
}

void TallyMonitor::reconnect()
{
    // Post to the io_context to ensure this happens on the correct thread.
    boost::asio::post(ioc_, [this]() {
        std::cout << "Reconnecting to ATEM switcher...\n";
        atem_connection_->disconnect();
        if (config_.mock_enabled) {
            atem_connection_ = std::make_unique<ATEMConnectionMock>(ioc_, config_.mock_inputs);
        } else {
            atem_connection_ = std::make_unique<ATEMConnectionReal>();
        }

        // Attempt to connect with the potentially updated IP from config_
        if (!atem_connection_->connect(config_.atem_ip)) {
            if (config_.use_mock_automatically) {
                std::cout << "Warning: Could not reconnect to ATEM switcher. Using mock data automatically.\n";
                // If connection fails, replace the connection object with a mock one and connect it.
                atem_connection_ = std::make_unique<ATEMConnectionMock>(ioc_, config_.mock_inputs);
                atem_connection_->connect(config_.atem_ip); // This starts the mock timer
                notify_mode_change(true);
            } else {
                std::cerr << "Error: Could not reconnect to ATEM switcher. Automatic mock fallback is disabled.\n";
            }
        } else {
            notify_mode_change(atem_connection_->is_mock_mode());
        }
        // Re-register the callback on the new connection object
        atem_connection_->on_tally_change([this](const TallyUpdate& update) { handle_tally_change(update); });
    });
}

void TallyMonitor::on_ready(ReadyCallback callback)
{
    ready_callback_ = std::move(callback);
}

void TallyMonitor::on_tally_change(TallyCallback callback)
{
    tally_callback_ = std::move(callback);
}

void TallyMonitor::on_mode_change(ModeChangeCallback callback)
{
    mode_change_callback_ = std::move(callback);
}

TallyState TallyMonitor::get_tally_state(uint16_t input_id) const
{
    std::lock_guard<std::mutex> lock(tally_states_mutex_);

    auto it = current_tally_states_.find(input_id);
    if (it != current_tally_states_.end()) {
        return it->second;
    }

    // Return default state if not found
    return TallyState { input_id, false, false, std::chrono::system_clock::now() };
}

std::vector<TallyState> TallyMonitor::get_all_tally_states() const
{
    std::lock_guard<std::mutex> lock(tally_states_mutex_);

    std::vector<TallyState> states;
    states.reserve(current_tally_states_.size());

    for (const auto& [input_id, state] : current_tally_states_) {
        states.push_back(state);
    }

    return states;
}

bool TallyMonitor::is_mock_mode() const
{
    return atem_connection_ ? atem_connection_->is_mock_mode() : false;
}

void TallyMonitor::poll_atem()
{
    if (!running_) {
        return;
    }

    // Poll ATEM connection for updates
    atem_connection_->poll();

    // Schedule next poll
    monitor_timer_->expires_after(16ms); // ~60fps polling rate
    monitor_timer_->async_wait([this](const boost::system::error_code& ec) {
        if (ec) {
            return; // Timer was cancelled
        }
        if (running_) {
            poll_atem();
        } });
}

void TallyMonitor::handle_tally_change(const TallyUpdate& update)
{
    auto new_state = TallyState {
        update.input_id,
        update.program,
        update.preview,
        std::chrono::system_clock::now()
    };

    // Update internal state
    {
        std::lock_guard<std::mutex> lock(tally_states_mutex_);
        current_tally_states_[update.input_id] = new_state;
    }

    std::cout << "Tally update - Input " << update.input_id
              << " Program: " << (update.program ? "ON" : "OFF")
              << " Preview: " << (update.preview ? "ON" : "OFF") << "\n";

    // Notify callback
    if (tally_callback_) {
        // Post to IO context to ensure thread safety
        tally_callback_(update);
    }
}

void TallyMonitor::notify_mode_change(bool is_mock)
{
    if (mode_change_callback_) {
        // Post to IO context to ensure thread safety
        mode_change_callback_(is_mock);
    }
}

} // namespace atem
