#pragma once

#include "atem_connection.h"
#include "tally_state.h"
#include <atomic>
#include <boost/asio.hpp>
#include <functional>
#include <memory>
#include <thread>

namespace atem {

struct Config;

class TallyMonitor {
public:
    using TallyCallback = std::function<void(const TallyUpdate&)>;

    explicit TallyMonitor(boost::asio::io_context& ioc, const Config& config);
    ~TallyMonitor();

    // Non-copyable, movable
    TallyMonitor(const TallyMonitor&) = delete;
    TallyMonitor& operator=(const TallyMonitor&) = delete;
    TallyMonitor(TallyMonitor&&) = delete;
    TallyMonitor& operator=(TallyMonitor&&) = delete;

    void start();
    void stop();

    void on_tally_change(TallyCallback callback);

    // Get current tally state for a specific input
    TallyState get_tally_state(uint16_t input_id) const;

    // Get all current tally states
    std::vector<TallyState> get_all_tally_states() const;

    bool is_mock_mode() const;

private:
    void monitor_loop();
    void handle_tally_change(const TallyUpdate& update);

    boost::asio::io_context& ioc_;
    const Config& config_;
    std::unique_ptr<ATEMConnection> atem_connection_;
    std::unique_ptr<boost::asio::steady_timer> monitor_timer_;

    TallyCallback tally_callback_;
    std::atomic<bool> running_ { false };

    mutable std::mutex tally_states_mutex_;
    std::unordered_map<uint16_t, TallyState> current_tally_states_;
};

} // namespace atem
