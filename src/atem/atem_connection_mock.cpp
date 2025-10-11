#include "atem_connection_mock.h"
#include <iostream>

namespace atem {

ATEMConnectionMock::ATEMConnectionMock(boost::asio::io_context& ioc, uint16_t num_inputs)
    : ioc_(ioc)
    , update_timer_(ioc)
    , random_generator_(std::random_device {}())
{
    if (num_inputs == 0) {
        num_inputs = 1; // Avoid division by zero
    }
    for (uint16_t i = 1; i <= num_inputs; ++i) {
        mock_states_.push_back({ i, false, false, std::chrono::system_clock::now() });
    }
    // Start with input 1 on program so there's an initial state.
    if (!mock_states_.empty()) {
        mock_states_[0].program = true;
        current_program_input_id_ = 1;
    }
}

bool ATEMConnectionMock::connect(const std::string& /*ip_address*/)
{
    std::cout << "Mock ATEM connection enabled." << std::endl;
    schedule_next_action(Action::Ready, 3s);
    return true;
}

void ATEMConnectionMock::disconnect()
{
    update_timer_.cancel();
    dissolve_timer_.reset();
}

void ATEMConnectionMock::poll()
{
    // No-op for mock. Updates are timer-based and asynchronous.
}

void ATEMConnectionMock::on_tally_change(TallyCallback callback)
{
    tally_callback_ = std::move(callback);
    // If we are already running, send the initial state
    if (current_program_input_id_ > 0) {
        for (const auto& state : mock_states_) {
            tally_callback_(state.to_update(true));
        }
    }
}

void ATEMConnectionMock::schedule_next_action(Action action, std::chrono::steady_clock::duration delay)
{
    update_timer_.expires_after(delay);
    update_timer_.async_wait([this, action](const boost::system::error_code& ec) {
        if (ec) {
            return; // Timer cancelled
        }
        perform_action(action);
    });
}

void ATEMConnectionMock::perform_action(Action action)
{
    if (mock_states_.empty())
        return;

    switch (action) {
    case Action::Ready: {
        // "Ready" command: put the next input in preview
        uint16_t next_input_id = (current_program_input_id_ % mock_states_.size()) + 1;
        set_preview(next_input_id);

        // Decide if the next transition is a cut or dissolve
        std::uniform_int_distribution<> dist(0, 1);
        Action next_action = (dist(random_generator_) == 0) ? Action::Cut : Action::Dissolve;
        schedule_next_action(next_action, 3s);
        break;
    }
    case Action::Cut: {
        // "Cut" command: swap program and preview
        uint16_t old_program = current_program_input_id_;
        uint16_t new_program = current_preview_input_id_;

        set_program(new_program);
        set_preview(old_program); // Old program becomes new preview
        schedule_next_action(Action::Ready, 4s);
        break;
    }
    case Action::Dissolve: {
        // "Dissolve" command: both on program for a duration
        uint16_t old_program = current_program_input_id_;
        uint16_t new_program = current_preview_input_id_;

        // Both inputs are on program during the dissolve
        set_program(new_program, false); // Don't clear old program yet

        // After 2 seconds, the dissolve completes
        dissolve_timer_ = std::make_unique<boost::asio::steady_timer>(ioc_);
        dissolve_timer_->expires_after(2s);
        dissolve_timer_->async_wait([this, old_program](const boost::system::error_code& ec) {
            if (ec)
                return;
            // Dissolve finished: old program is no longer on program, and becomes the new preview.
            mock_states_[old_program - 1].program = false;
            set_preview(old_program); // This also sends the update for the old program state change.
            // The new program was already set before the timer started.
            schedule_next_action(Action::Ready, 4s);
        });
        break;
    }
    }
}

void ATEMConnectionMock::set_program(uint16_t input_id, bool clear_old)
{
    if (clear_old && current_program_input_id_ > 0) {
        mock_states_[current_program_input_id_ - 1].program = false;
        if (tally_callback_) {
            tally_callback_(mock_states_[current_program_input_id_ - 1].to_update(true));
        }
    }

    current_program_input_id_ = input_id;
    mock_states_[input_id - 1].program = true;
    mock_states_[input_id - 1].preview = false; // An input can't be program and preview

    if (input_id == current_preview_input_id_) {
        current_preview_input_id_ = 0; // It's no longer just in preview
    }

    if (tally_callback_) {
        tally_callback_(mock_states_[input_id - 1].to_update(true));
    }
}

void ATEMConnectionMock::set_preview(uint16_t input_id)
{
    if (current_preview_input_id_ > 0) {
        mock_states_[current_preview_input_id_ - 1].preview = false;
        if (tally_callback_) {
            tally_callback_(mock_states_[current_preview_input_id_ - 1].to_update(true));
        }
    }
    current_preview_input_id_ = input_id;
    mock_states_[input_id - 1].preview = true;
    if (tally_callback_)
        tally_callback_(mock_states_[input_id - 1].to_update(true));
}

} // namespace atem
