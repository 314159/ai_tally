#include "gui/BackendService.h"
#include "gui/MainFrame.h" // For event types
#include "gui/atem_tally_provider.h"
#include "gui/mock_tally_provider.h"
#include "gui/tally_provider.h"
#include "platform_interface.h"
#include "tally_monitor.h"
#include "websocket_server.h"
#include <boost/asio.hpp>
#include <wx/event.h>

namespace atem {
namespace gui {

    BackendService::BackendService(wxEvtHandler* event_handler, const Config& config)
        : event_handler_(event_handler)
        , config_(config)
    {
    }

    BackendService::~BackendService()
    {
        Stop();
    }

    void BackendService::Start()
    {
        if (running_.exchange(true)) {
            return; // Already running
        }
        service_thread_ = std::thread(&BackendService::Run, this);
    }

    void BackendService::Stop()
    {
        if (!running_.exchange(false)) {
            return; // Already stopped
        }

        if (service_thread_.joinable()) {
            service_thread_.join();
        }
    }

    void BackendService::Restart()
    {
        Stop();
        Start();
    }

    bool BackendService::IsRunning() const
    {
        return running_.load();
    }

    void BackendService::SetConfig(const Config& config)
    {
        config_ = config;
    }

    void BackendService::Run()
    {
        auto cleanup_guard = gsl::finally([this] {
            platform::cleanup();
            auto* event = new wxThreadEvent(EVT_BACKEND_UPDATE);
            event->SetInt(0); // 0 for stopped
            wxQueueEvent(event_handler_, event);
        });

        try {
            if (!platform::initialize()) {
                // In a real app, you'd post an error message to the GUI
                auto* event = new wxThreadEvent(EVT_BACKEND_ERROR);
                event->SetString("Platform initialization failed.");
                wxQueueEvent(event_handler_, event);
                return;
            }

            boost::asio::io_context io_context;
            auto work_guard = boost::asio::make_work_guard(io_context);

            std::unique_ptr<ITallyProvider> tally_provider;
            if (config_.mock_enabled) {
                tally_provider = std::make_unique<MockTallyProvider>(io_context, config_);
            } else {
                tally_provider = std::make_unique<AtemTallyProvider>(io_context, config_);
            }

            auto server = std::make_shared<atem::HttpAndWebSocketServer>(io_context,
                boost::asio::ip::make_address(config_.ws_address),
                config_.ws_port, *tally_provider);

            // Register a callback for the WebSocket server to broadcast updates.
            tally_provider->add_tally_change_callback([&server](const TallyUpdate& update) {
                server->broadcast_tally_update(update);
            });

            // This callback sends tally updates to the GUI thread.
            tally_provider->add_tally_change_callback([this](const TallyUpdate& update) {
                auto* event = new wxThreadEvent(EVT_TALLY_UPDATE);
                // We must heap-allocate the payload as the event is processed asynchronously
                event->SetPayload(new TallyUpdate(update));
                wxQueueEvent(event_handler_, event);
            });

            tally_provider->on_mode_change([this](bool is_mock) {
                auto* event = new wxThreadEvent(EVT_MODE_UPDATE);
                event->SetInt(is_mock ? 1 : 0);
                wxQueueEvent(event_handler_, event);
            });

            tally_provider->start();
            server->start();

            auto* event = new wxThreadEvent(EVT_BACKEND_UPDATE);
            event->SetInt(1); // 1 for started
            wxQueueEvent(event_handler_, event);

            // After starting, immediately notify the UI of the current operating mode.
            // The provider will do this itself in its start() method.

            while (running_) {
                io_context.poll();
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            io_context.stop();
            server->stop();
            tally_provider->stop();
            work_guard.reset();

        } catch (const std::exception& e) {
            auto* event = new wxThreadEvent(EVT_BACKEND_ERROR);
            event->SetString(wxString::FromUTF8(e.what()));
            wxQueueEvent(event_handler_, event);
        }
    }

} // namespace gui
} // namespace atem
