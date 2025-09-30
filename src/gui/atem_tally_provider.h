#pragma once

#include "atem/atem_connection.h"
#include "config.h"
#include "gui/tally_provider.h"
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <memory>

namespace atem {
namespace gui {

    /**
     * @class AtemTallyProvider
     * @brief An implementation of ITallyProvider that connects to a real ATEM switcher.
     *
     * This class manages the connection to the ATEM hardware, polls for tally
     * state changes, and invokes callbacks when updates occur.
     */
    class AtemTallyProvider final : public ITallyProvider {
    public:
        AtemTallyProvider(boost::asio::io_context& io_context, const Config& config);
        ~AtemTallyProvider() override;

        void start() override;
        void stop() override;

        void add_tally_change_callback(TallyChangeCallback callback) override;
        void on_mode_change(ModeChangeCallback callback) override;

    private:
        void schedule_poll();

        const Config& config_;
        boost::asio::steady_timer poll_timer_;
        std::unique_ptr<atem::ATEMConnection> atem_connection_;
        std::vector<TallyChangeCallback> tally_callbacks_;
    };

} // namespace gui
} // namespace atem
