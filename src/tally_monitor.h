#pragma once

#include "config.h" // Include the full definition of Config

#include "atem_connection.h"
#include "tally_state.h"
#include <atomic>
#include <boost/asio.hpp>
#include <functional>
#include <memory>
#include <thread>
#include <unordered_map>
#include <vector>

namespace atem {

class TallyMonitor {
public:
    using ReadyCallback = std::function<void()>;
    using ModeChangeCallback = std::function<void(bool)>;
    using TallyCallback = std::function<void(const TallyUpdate&)>;

    explicit TallyMonitor(boost::asio::io_context& ioc, const Config& config);
    ~TallyMonitor() noexcept;

    // Non-copyable, movable
    TallyMonitor(const TallyMonitor&) = delete;
    TallyMonitor& operator=(const TallyMonitor&) = delete;
    TallyMonitor(TallyMonitor&&) = delete;
    TallyMonitor& operator=(TallyMonitor&&) = delete;

    void start();
    void stop() noexcept;

    // Reconnect to the ATEM switcher with current config.
    void reconnect();

    void on_ready(ReadyCallback callback);
    void on_tally_change(TallyCallback callback);
    void on_mode_change(ModeChangeCallback callback);

    // Get current tally state for a specific input
    TallyState get_tally_state(uint16_t input_id) const;

    // Get all current tally states
    std::vector<TallyState> get_all_tally_states() const;

    bool is_mock_mode() const;

private:
    void monitor_loop();
    void handle_tally_change(const TallyUpdate& update);
    void notify_mode_change(bool is_mock);

    ReadyCallback ready_callback_;
    boost::asio::io_context& ioc_;
    const Config& config_;
    std::unique_ptr<ATEMConnection> atem_connection_;
    std::unique_ptr<boost::asio::steady_timer> monitor_timer_;

    ModeChangeCallback mode_change_callback_;
    TallyCallback tally_callback_;
    std::atomic<bool> running_ { false };

    mutable std::mutex tally_states_mutex_;
    std::unordered_map<uint16_t, TallyState> current_tally_states_;
};

} // namespace atem
