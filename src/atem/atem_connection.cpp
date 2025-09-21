#include "atem_connection.h"
#include <iostream>

using namespace std::chrono_literals;

namespace atem {

ATEMConnection::ATEMConnection(uint16_t mock_inputs)
    : mock_input_count_(mock_inputs)
    , rng_(std::random_device {}())
    , last_mock_update_(std::chrono::steady_clock::now())
{
    init_mock_data();
    atem_discovery_ = ATEMDiscovery::create();
}

ATEMConnection::~ATEMConnection()
{
    disconnect();
}

bool ATEMConnection::connect(const std::string& ip_address)
{
    if (connected_)
        return true;

    atem_device_ = atem_discovery_->connect_to(ip_address);

    if (atem_device_) {
        std::cout << "Successfully connected to ATEM: " << atem_device_->get_product_name() << std::endl;
        atem_device_->set_callback(this);
        connected_ = true;
        return true;
    } else {
        std::cerr << "Could not connect to ATEM switcher." << std::endl;
        return false;
    }
}

void ATEMConnection::disconnect()
{
    connected_ = false;
    if (atem_device_) {
        atem_device_->disconnect();
        atem_device_.reset();
    }
}

bool ATEMConnection::is_connected() const
{
    return connected_;
}

void ATEMConnection::on_tally_change(TallyCallback callback)
{
    tally_callback_ = std::move(callback);
}

void ATEMConnection::poll()
{
    if (mock_mode_) {
        update_mock_tally();
        return;
    }

    if (connected_ && atem_device_) {
        // The SDK is callback-driven, but some operations might require polling.
        // The BMDSwitcherAPIDispatch.cpp file needs to be called periodically
        // to dispatch events on non-Windows platforms.
        atem_device_->poll();
    }
}

void ATEMConnection::set_mock_mode(bool enabled)
{
    mock_mode_ = enabled;
    if (enabled) {
        std::cout << "Mock mode enabled - generating synthetic tally data\n";
        init_mock_data();
    }
}

bool ATEMConnection::is_mock_mode() const
{
    return mock_mode_;
}

void ATEMConnection::on_tally_state_changed(const TallyUpdate& update)
{
    if (tally_callback_) {
        tally_callback_(update);
    }
}

void ATEMConnection::on_disconnected()
{
    std::cerr << "ATEM Connection Lost." << std::endl;
    disconnect();
}

void ATEMConnection::init_mock_data()
{
    // Initialize 8 mock inputs
    for (uint16_t i = 1; i <= mock_input_count_; ++i) {
        mock_tally_states_[i] = TallyState {
            i, false, false, std::chrono::system_clock::now()
        };
    }
}

void ATEMConnection::update_mock_tally()
{
    auto now = std::chrono::steady_clock::now();

    // Update mock tally every 2-5 seconds
    if (now - last_mock_update_ < 2s) {
        return;
    }

    last_mock_update_ = now;

    if (!tally_callback_) {
        return;
    }

    // Find current program and preview inputs
    uint16_t old_program_id = 0;
    uint16_t old_preview_id = 0;
    for (const auto& [id, state] : mock_tally_states_) {
        if (state.program)
            old_program_id = id;
        if (state.preview)
            old_preview_id = id;
    }

    // Choose new program and preview inputs
    std::uniform_int_distribution<uint16_t> input_dist(1, mock_input_count_);
    uint16_t new_program_id = input_dist(rng_);
    uint16_t new_preview_id = input_dist(rng_);

    // To make it more realistic, when we change the program, the old program becomes the new preview.
    if (new_program_id != old_program_id) {
        new_preview_id = old_program_id;
    }

    // If the program input has changed, send updates
    if (new_program_id != old_program_id) {
        // Update old program (which is now off-air)
        if (old_program_id != 0) {
            mock_tally_states_[old_program_id].program = false;
        }
        // Update new program
        mock_tally_states_[new_program_id].program = true;
        tally_callback_(mock_tally_states_[new_program_id].to_update(true));
    }

    // If the preview input has changed, send updates
    if (new_preview_id != old_preview_id && new_preview_id != 0) {
        // Update old preview
        if (old_preview_id != 0) {
            mock_tally_states_[old_preview_id].preview = false;
            tally_callback_(mock_tally_states_[old_preview_id].to_update(true));
        }
        mock_tally_states_[new_preview_id].preview = true;
        tally_callback_(mock_tally_states_[new_preview_id].to_update(true));
    }
}

} // namespace atem