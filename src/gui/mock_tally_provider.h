#pragma once

#include "config.h"
#include "gui/tally_provider.h"
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <random>

namespace atem {
namespace gui {

    /**
     * @class MockTallyProvider
     * @brief A mock implementation of ITallyProvider for testing.
     *
     * This class simulates tally data changes at a regular interval,
     * cycling through different program and preview states.
     */
    class MockTallyProvider final : public ITallyProvider {
    public:
        MockTallyProvider(boost::asio::io_context& io_context, const Config& config);

        void start() override;
        void stop() override;

        void add_tally_change_callback(TallyChangeCallback callback) override;
        void on_mode_change(ModeChangeCallback callback) override;

    private:
        void schedule_update();
        void perform_update();

        const Config& config_;
        boost::asio::steady_timer timer_;
        std::vector<TallyChangeCallback> tally_callbacks_;
        ModeChangeCallback mode_callback_;
        std::mt19937 random_engine_;
        uint16_t current_program_ = 0;
        uint16_t current_preview_ = 0;
    };

} // namespace gui
} // namespace atem
