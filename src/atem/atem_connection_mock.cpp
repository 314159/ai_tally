#include "atem_connection_mock.h"
#include <iostream>

namespace atem {

ATEMConnectionMock::ATEMConnectionMock(boost::asio::io_context& ioc, uint16_t num_inputs)
    : update_timer_(ioc)
    , random_generator_(std::random_device {}())
{
    for (uint16_t i = 1; i <= num_inputs; ++i) {
        mock_states_.push_back({ i, false, false, std::chrono::system_clock::now() });
    }
}

bool ATEMConnectionMock::connect(const std::string& /*ip_address*/)
{
    std::cout << "Mock ATEM connection enabled." << std::endl;
    schedule_random_update();
    return true;
}

void ATEMConnectionMock::disconnect()
{
    update_timer_.cancel();
}

void ATEMConnectionMock::poll()
{
    // No-op for mock. Updates are timer-based.
}

void ATEMConnectionMock::on_tally_change(TallyCallback callback)
{
    tally_callback_ = std::move(callback);
}

void ATEMConnectionMock::schedule_random_update()
{
    std::uniform_int_distribution<int> time_dist(2000, 5000); // 2-5 seconds
    update_timer_.expires_after(std::chrono::milliseconds(time_dist(random_generator_)));

    update_timer_.async_wait([this](const boost::system::error_code& ec) {
        if (ec) {
            return; // Timer cancelled
        }

        if (tally_callback_ && !mock_states_.empty()) {
            std::uniform_int_distribution<size_t> input_dist(0, mock_states_.size() - 1);
            std::uniform_int_distribution<int> state_dist(0, 2);

            auto& state = mock_states_[input_dist(random_generator_)];
            int new_state = state_dist(random_generator_);
            state.program = (new_state == 1);
            state.preview = (new_state == 2);

            tally_callback_(state.to_update(true));
        }

        schedule_random_update();
    });
}

} // namespace atem
