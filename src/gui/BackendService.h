#pragma once

#include "config.h"
#include <atomic>
#include <boost/asio/io_context.hpp>
#include <memory>
#include <thread>

// Forward declarations
class wxEvtHandler;
namespace atem {
class TallyMonitor;
class HttpAndWebSocketServer;
} // namespace atem

namespace atem {
namespace gui {

    /**
     * @class BackendService
     * @brief Manages the core application logic in a background thread.
     *
     * This class encapsulates the io_context, TallyMonitor, and WebSocket server.
     * It runs the io_context in a dedicated thread and communicates with the
     * wxWidgets GUI (the event_handler) via thread-safe wxThreadEvents.
     */
    class BackendService {
    public:
        BackendService(wxEvtHandler* event_handler, const Config& config);
        ~BackendService();

        // Non-copyable, non-movable
        BackendService(const BackendService&) = delete;
        BackendService& operator=(const BackendService&) = delete;
        BackendService(BackendService&&) = delete;
        BackendService& operator=(BackendService&&) = delete;

        void Start();
        void Stop();
        void Restart();
        bool IsRunning() const;
        void SetConfig(const Config& config);

    private:
        void Run();

        std::atomic<bool> running_ { false };
        wxEvtHandler* const event_handler_;
        Config config_;
        std::thread service_thread_;
    };

} // namespace gui
} // namespace atem
