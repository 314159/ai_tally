#pragma once

#include "config.h"
#include "iatem_connection.h"
#include <boost/asio/steady_timer.hpp>
#include <chrono>
#include <cstdint>
#include <random>
#include <vector>

using namespace std::chrono_literals;

namespace atem {

class ATEMConnectionMock final : public IATEMConnection { // NOLINT(cppcoreguidelines-pro-type-member-init)
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
    uint16_t get_input_count() const override
    {
        return static_cast<uint16_t>(mock_states_.size());
    }

private:
    enum class Action { Ready, Cut, Dissolve };

    void schedule_next_action(Action action, std::chrono::steady_clock::duration delay);
    void perform_action(Action action);

    void set_program(uint16_t input_id, bool clear_old = true);
    void set_preview(uint16_t input_id);

    TallyCallback tally_callback_;
    std::vector<TallyState> mock_states_;

    boost::asio::io_context& ioc_;
    boost::asio::steady_timer update_timer_;
    std::unique_ptr<boost::asio::steady_timer> dissolve_timer_;

    std::mt19937 random_generator_;

    uint16_t current_program_input_id_ { 0 };
    uint16_t current_preview_input_id_ { 0 };
};

} // namespace atem
