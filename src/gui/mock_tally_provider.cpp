#include "gui/mock_tally_provider.h"
#include <iostream>

namespace atem {
namespace gui {

    MockTallyProvider::MockTallyProvider(boost::asio::io_context& io_context, const Config& config)
        : config_(config)
        , timer_(io_context)
        , random_engine_(std::random_device {}())
    {
    }

    void MockTallyProvider::start()
    {
        std::cout << "Starting Mock Tally Provider..." << std::endl;
        if (mode_callback_) {
            mode_callback_(true); // true for mock mode
        }
        schedule_update();
    }

    void MockTallyProvider::stop()
    {
        std::cout << "Stopping Mock Tally Provider..." << std::endl;
        timer_.cancel();
    }

    void MockTallyProvider::add_tally_change_callback(TallyChangeCallback callback)
    {
        tally_callbacks_.push_back(std::move(callback));
    }

    void MockTallyProvider::on_mode_change(ModeChangeCallback callback)
    {
        mode_callback_ = std::move(callback);
    }

    void MockTallyProvider::schedule_update()
    {
        timer_.expires_after(std::chrono::milliseconds(config_.mock_update_interval_ms));
        timer_.async_wait([this](const boost::system::error_code& ec) {
            if (!ec) {
                perform_update();
                schedule_update();
            }
        });
    }

    void MockTallyProvider::perform_update()
    {
        if (tally_callbacks_.empty()) {
            return;
        }

        std::uniform_int_distribution<uint16_t> dist(1, config_.mock_inputs);

        uint16_t new_program = dist(random_engine_);
        uint16_t new_preview = dist(random_engine_);
        while (new_preview == new_program) {
            new_preview = dist(random_engine_);
        }

        // Send "off" for old states
        if (current_program_ > 0) {
            for (const auto& cb : tally_callbacks_)
                cb({ current_program_, false, false });
        }
        if (current_preview_ > 0) {
            for (const auto& cb : tally_callbacks_)
                cb({ current_preview_, false, false });
        }

        // Send "on" for new states
        for (const auto& cb : tally_callbacks_) {
            cb({ new_program, true, false });
            cb({ new_preview, false, true });
        }

        current_program_ = new_program;
        current_preview_ = new_preview;

        std::cout << "Mock Update: Program=" << new_program << ", Preview=" << new_preview << std::endl;
    }

} // namespace gui
} // namespace atem
