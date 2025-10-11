#include "atem_connection_real.h"
#include <iostream>

namespace atem {

ATEMConnectionReal::ATEMConnectionReal()
{
    atem_discovery_ = ATEMDiscovery::create();
}

ATEMConnectionReal::~ATEMConnectionReal() noexcept
{
    disconnect();
}

bool ATEMConnectionReal::connect(const std::string& ip_address)
{
    if (connected_)
        return true;

    atem_device_ = atem_discovery_->connect_to(ip_address);

    if (atem_device_) {
        std::cout << "Successfully connected to ATEM: " << atem_device_->get_product_name() << std::endl;
        atem_device_->set_callback(this);
        connected_ = true;
        return true;
    }

    std::cerr << "Could not connect to ATEM switcher." << std::endl;
    return false;
}

void ATEMConnectionReal::disconnect()
{
    connected_ = false;
    if (atem_device_) {
        atem_device_->disconnect();
        atem_device_.reset();
    }
}

void ATEMConnectionReal::on_tally_change(TallyCallback callback)
{
    tally_callback_ = std::move(callback);
}

void ATEMConnectionReal::poll()
{
    if (connected_ && atem_device_) {
        // The SDK is callback-driven, but some operations might require polling.
        // The BMDSwitcherAPIDispatch.cpp file needs to be called periodically
        // to dispatch events on non-Windows platforms.
        atem_device_->poll();
    }
}

bool ATEMConnectionReal::is_mock_mode() const
{
    return false;
}

uint16_t ATEMConnectionReal::get_input_count() const
{
    if (connected_ && atem_device_) {
        return atem_device_->get_input_count();
    }
    return 0;
}

void ATEMConnectionReal::on_tally_state_changed(const TallyUpdate& update)
{
    if (tally_callback_) {
        tally_callback_(update);
    }
}

void ATEMConnectionReal::on_disconnected()
{
    std::cerr << "ATEM Connection Lost." << std::endl;
    disconnect();
}

} // namespace atem
