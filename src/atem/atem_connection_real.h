#pragma once

#include "atem_sdk_wrapper.h"
#include "iatem_connection.h"
#include <atomic>
#include <memory>
#include <string>

namespace atem {

class ATEMConnectionReal final : public IATEMConnection, private ATEMSwitcherCallback {
public:
    ATEMConnectionReal();
    ~ATEMConnectionReal() noexcept override;

    // IATEMConnection implementation
    bool connect(const std::string& ip_address) override;
    void disconnect() override;
    void poll() override;
    void on_tally_change(TallyCallback callback) override;
    bool is_mock_mode() const override;
    uint16_t get_input_count() const override;
    std::vector<InputInfo> get_inputs() const override;

private:
    // ATEMSwitcherCallback implementation
    void on_tally_state_changed(const TallyUpdate& update) override;
    void on_disconnected() override;

    std::atomic<bool> connected_ { false };

    TallyCallback tally_callback_;

    std::unique_ptr<ATEMDiscovery> atem_discovery_;
    std::unique_ptr<ATEMDevice> atem_device_;
};

} // namespace atem
