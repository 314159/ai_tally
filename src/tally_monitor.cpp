#include "tally_monitor.h"
#include <chrono>
#include <iostream>

using namespace std::chrono_literals;

namespace atem {

TallyMonitor::TallyMonitor(boost::asio::io_context& ioc, std::string atem_ip_address, uint16_t mock_inputs)
    : ioc_(ioc)
    , atem_ip_address_(std::move(atem_ip_address))
    , atem_connection_(std::make_unique<ATEMConnection>(mock_inputs))
    , monitor_timer_(std::make_unique<boost::asio::steady_timer>(ioc))
{
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
    if (!atem_connection_->connect(atem_ip_address_)) {
        std::cout << "Warning: Could not connect to ATEM switcher. Using mock data.\n";
        atem_connection_->set_mock_mode(true);
    }

    // Set up tally callback
    atem_connection_->on_tally_change([this](const TallyUpdate& update) { handle_tally_change(update); });

    // Start monitoring loop
    monitor_loop();
}

void TallyMonitor::stop()
{
    if (!running_.exchange(false)) {
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

void TallyMonitor::on_tally_change(TallyCallback callback)
{
    tally_callback_ = std::move(callback);
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

void TallyMonitor::monitor_loop()
{
    if (!running_) {
        return;
    }

    // Poll ATEM connection for updates
    if (atem_connection_) {
        atem_connection_->poll();
    }

    // Schedule next poll
    monitor_timer_->expires_after(16ms); // ~60fps polling rate
    monitor_timer_->async_wait([this](boost::system::error_code ec) {
        if (!ec && running_) {
            monitor_loop();
        } });
}

void TallyMonitor::handle_tally_change(const TallyUpdate& update)
{
    TallyState new_state {
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
        boost::asio::post(ioc_, [this, update]() { tally_callback_(update); });
    }
}

} // namespace atem