#pragma once

#include "tally_state.h"
#include <functional>

namespace atem {
namespace gui {

    /**
     * @class ITallyProvider
     * @brief An interface for objects that provide ATEM tally information.
     *
     * This abstract base class defines the contract for tally providers,
     * allowing the BackendService to be agnostic about whether it's using
     * a live ATEM connection or a mock data source.
     */
    class ITallyProvider {
    public:
        using TallyChangeCallback = std::function<void(const TallyUpdate&)>;
        using ModeChangeCallback = std::function<void(bool is_mock)>;

        virtual ~ITallyProvider() = default;

        virtual void start() = 0;
        virtual void stop() = 0;

        virtual void add_tally_change_callback(TallyChangeCallback callback) = 0;
        virtual void on_mode_change(ModeChangeCallback callback) = 0;
    };

} // namespace gui
} // namespace atem
