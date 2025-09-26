#pragma once

#include "atem_sdk_wrapper.h"
#include "tally_state.h"
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <random>
#include <string>
#include <thread>

namespace atem {
class ATEMConnection : public ATEMSwitcherCallback {
public:
    using TallyCallback = std::function<void(const TallyUpdate&)>;
    explicit ATEMConnection(uint16_t mock_inputs = 8);
    ~ATEMConnection() noexcept;

    // Non-copyable, not movable (Rule of 7)
    ATEMConnection(const ATEMConnection&) = delete;
    ATEMConnection& operator=(const ATEMConnection&) = delete;
    ATEMConnection(ATEMConnection&&) = delete;
    ATEMConnection& operator=(ATEMConnection&&) = delete;

    bool connect(const std::string& ip_address);
    void disconnect() noexcept;
    bool is_connected() const;

    void on_tally_change(TallyCallback callback);
    void poll();

    // For testing/demo purposes
    void set_mock_mode(bool enabled) noexcept;
    bool is_mock_mode() const;

private:
    // ATEMSwitcherCallback implementation
    void on_tally_state_changed(const TallyUpdate& update) override;
    void on_disconnected() override;

    void init_mock_data();
    void update_mock_tally();

    std::atomic<bool> connected_ { false };
    std::atomic<bool> mock_mode_ { false };
    uint16_t mock_input_count_;

    TallyCallback tally_callback_;

    std::unique_ptr<ATEMDiscovery> atem_discovery_;
    std::unique_ptr<ATEMDevice> atem_device_;

    // Mock data for demonstration
    std::unordered_map<uint16_t, TallyState> mock_tally_states_;
    std::mt19937 rng_;
    std::chrono::steady_clock::time_point last_mock_update_;
};

} // namespace atem
