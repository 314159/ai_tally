#include "gui/atem_tally_provider.h"
#include <iostream>

namespace atem {
namespace gui {

    AtemTallyProvider::AtemTallyProvider(boost::asio::io_context& io_context, const Config& config)
        : config_(config)
        , poll_timer_(io_context)
    {
    }

    AtemTallyProvider::~AtemTallyProvider()
    {
        stop();
    }

    void AtemTallyProvider::start()
    {
        std::cout << "Starting ATEM Tally Provider..." << std::endl;
        atem_connection_ = std::make_unique<atem::ATEMConnection>();
        atem_connection_->set_mock_mode(false); // Ensure we are in live mode

        if (!atem_connection_->connect(config_.atem_ip)) {
            throw std::runtime_error("Failed to connect to ATEM switcher at " + config_.atem_ip);
        }

        // Register a single callback with the ATEM connection. This callback will
        // then dispatch the update to all registered listeners.
        atem_connection_->on_tally_change([this](const TallyUpdate& update) {
            for (const auto& cb : tally_callbacks_) {
                cb(update);
            }
        });

        std::cout << "Successfully connected to ATEM switcher." << std::endl;
        schedule_poll();
    }

    void AtemTallyProvider::stop()
    {
        std::cout << "Stopping ATEM Tally Provider..." << std::endl;
        poll_timer_.cancel();
        if (atem_connection_) {
            atem_connection_->disconnect();
        }
    }

    void AtemTallyProvider::add_tally_change_callback(TallyChangeCallback callback)
    {
        tally_callbacks_.push_back(std::move(callback));
    }

    void AtemTallyProvider::on_mode_change(ModeChangeCallback callback)
    {
        // In live mode, we immediately report that we are not in mock mode.
        callback(false);
    }

    void AtemTallyProvider::schedule_poll()
    {
        poll_timer_.expires_after(std::chrono::milliseconds(50)); // Poll frequently
        poll_timer_.async_wait([this](const boost::system::error_code& ec) {
            if (!ec && atem_connection_) {
                atem_connection_->poll();
                schedule_poll();
            }
        });
    }

} // namespace gui
} // namespace atem
