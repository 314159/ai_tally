#pragma once

#include "iatem_connection.h"
#include <boost/asio/steady_timer.hpp>
#include <chrono>
#include <cstdint>
#include <random>
#include <vector>

namespace atem {

class ATEMConnectionMock final : public IATEMConnection {
public:
    explicit ATEMConnectionMock(boost::asio::io_context& ioc, uint16_t num_inputs);

    bool connect(const std::string& ip_address) override;
    void disconnect() override;
    void poll() override;
    void on_tally_change(TallyCallback callback) override;
    bool is_mock_mode() const override
    {
        return true;
    }

private:
    void schedule_random_update();

    TallyCallback tally_callback_;
    std::vector<TallyState> mock_states_;
    boost::asio::steady_timer update_timer_;
    std::mt19937 random_generator_;
};

} // namespace atem
