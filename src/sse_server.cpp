#include "sse_server.h"
#include "config.h"
#include "tally_monitor.h"
#include "tally_state.h"
#include "version.h"
#include <boost/json.hpp>
#include <chrono>
#include <iostream>
#include <memory>
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#include <restbed>
#pragma clang diagnostic pop
#include <string>
#include <string_view>

namespace atem {
namespace {

    // Generates a dashboard page showing the status of all inputs.
    std::string generate_status_page(const uint16_t num_inputs, const std::string_view server_ip, const std::string_view sdk_version)
    {
        std::string html = R"(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>ATEM Tally Status</title>
    <style>
        body { font-family: sans-serif; background-color: #2c3e50; color: #ecf0f1; text-align: center; padding-top: 20px; margin: 0; }
        h1 { color: #3498db; }
        .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); gap: 20px; max-width: 1200px; margin: 20px auto; padding: 0 20px; }
        .tally-cell {
            display: flex;
            justify-content: center;
            align-items: center;
            padding: 40px 20px;
            font-size: 2em;
            font-weight: bold;
            border-radius: 8px;
            transition: background-color 0.3s, color 0.3s;
            color: rgba(255, 255, 255, 0.8);
            text-shadow: 2px 2px 4px rgba(0,0,0,0.5);
        }
        .off { background-color: #34495e; }
        .preview { background-color: #009900; color: #fff; }
        .program { background-color: #FF0000; color: #fff; }
        .footer { position: fixed; bottom: 10px; left: 10px; font-size: 14px; color: #FFFFFF; font-family: monospace; text-shadow: -1px -1px 0 #000, 1px -1px 0 #000, -1px 1px 0 #000, 1px 1px 0 #000; }
        .footer a { color: #3498db; text-decoration: none; }
        .footer a:hover { text-decoration: underline; }
        .nav-link { margin-bottom: 5px; font-family: sans-serif; font-size: 16px; }
        .status-line { font-size: 14px; }
    </style>
</head>
<body>
    <h1>Tally Status Overview</h1>
    <div class="grid">
)";
        for (uint16_t i = 1; i <= num_inputs; ++i) {
            html += "<div id=\"input-" + std::to_string(i) + R"(" class="tally-cell off">)" + std::to_string(i) + "</div>\n";
        }
        html += R"(
    </div>
    <div class="footer">
        <div class="nav-link"><a href="/">Switch to Single Input View</a></div>
        <div class="status-line">
            <span id="server-details"></span> | Status: <span id="connection-status">Connecting...</span>
        </div>
    </div>
    <script>
        const serverVersion = ")"
            + std::string(version::GIT_VERSION) + R"(";
        const sdkVersion = ")"
            + std::string(sdk_version) + R"(";

        let isConnected = false;
        let currentMockStatus = false;

        function connect() {
            console.log('Attempting to connect to SSE endpoint...');
            const eventSource = new EventSource('/events');

            eventSource.onopen = () => {
                console.log('SSE connection opened.');
            };

            eventSource.addEventListener('tally_update', (event) => {
                if (!isConnected) {
                    isConnected = true;
                    document.getElementById('connection-status').textContent = 'Connected';
                    console.log('Client is now marked as connected.');
                }
                const data = JSON.parse(event.data);
                const cell = document.getElementById('input-' + data.input);
                if (cell) {
                    if (data.program) {
                        cell.className = 'tally-cell program';
                    } else if (data.preview) {
                        cell.className = 'tally-cell preview';
                    } else {
                        cell.className = 'tally-cell off';
                    }
                }
            });

            eventSource.addEventListener('mode_change', (event) => {
                const data = JSON.parse(event.data);
                if (data.mock !== currentMockStatus) {
                    console.log('Mock status changed, reloading page.');
                    location.reload();
                }
                currentMockStatus = data.mock;
            });

            eventSource.addEventListener('server_info', (event) => {
                const data = JSON.parse(event.data);
                if (data.server_version !== serverVersion) {
                    console.log('Server version mismatch, reloading page.');
                    location.reload();
                }
            });

            eventSource.onerror = (err) => {
                console.error('SSE connection error:', err);
                isConnected = false;
                document.getElementById('connection-status').textContent = 'Disconnected';
                // Reset all tally cells to 'off' state to prevent showing stale status
                const cells = document.querySelectorAll('.tally-cell');
                cells.forEach(cell => {
                    cell.className = 'tally-cell off';
                });
                eventSource.close();
                setTimeout(connect, 1000); // Try to reconnect after 1 second
            };
        }

        const serverDetails = `Server ${serverVersion} (SDK ${sdkVersion}) @ )"
            + std::string(server_ip) + R"(`;
        document.getElementById('server-details').textContent = serverDetails;

        // Initial state from initial tally updates
        document.addEventListener('DOMContentLoaded', () => {
            // We can infer initial mock status from the first tally update
            eventSource.addEventListener('tally_update', (e) => {
                currentMockStatus = JSON.parse(e.data).mock;
            }, { once: true });
        });

        connect();
    </script>
</body>
</html>
)";
        return html;
    }

    // Generates an HTML page listing all tally inputs based on config.
    std::string generate_index_page(const uint16_t num_inputs)
    {
        std::string html = R"(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>ATEM Tally - Input Selection</title>
    <style>
        body { font-family: sans-serif; background-color: #2c3e50; color: #ecf0f1; text-align: center; padding-top: 50px; }
        h1 { color: #3498db; }
        .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); gap: 20px; max-width: 800px; margin: 50px auto; }
        a { display: block; padding: 40px 20px; background-color: #34495e; color: #ecf0f1; text-decoration: none; font-size: 1.5em; border-radius: 8px; transition: background-color 0.3s; }
        a:hover { background-color: #46627f; }
        .footer { margin-top: 40px; }
        .footer a { display: inline-block; padding: 10px 20px; font-size: 1em; background-color: #3498db; }
        .footer a:hover { background-color: #2980b9; }
    </style>
</head>
<body>
    <h1>Select an Input for Tally View</h1>
    <div class="grid">
)";
        for (uint16_t i = 1; i <= num_inputs; ++i) {
            html += "<a href=\"/tally/" + std::to_string(i) + "\">Input " + std::to_string(i) + "</a>\n";
        }
        html += R"(
    </div>
    <div class="footer">
        <a href="/status">Show All Inputs (Status Overview)</a>
    </div>
</body>
</html>
)";
        return html;
    }

    // Generates a full-screen tally client page for a specific input.
    std::string generate_tally_page(const int input_id, const bool is_mock, const std::string_view server_ip, const std::string_view sdk_version)
    {
        return R"(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>Tally - Input )"
            + std::to_string(input_id) + R"(</title>
    <style> /* NOLINT(bugprone-suspicious-string-integer-assignment) */
        html, body { margin: 0; padding: 0; width: 100%; height: 100%; overflow: hidden; font-family: sans-serif; }
        body { transition: background-color 0.3s ease; display: flex; justify-content: center; align-items: center; }
        .off { background-color: #000000; }
        .preview { background-color: #009900; }
        .program { background-color: #FF0000; }
        .input-number { display: flex; align-items: baseline; justify-content: center; font-size: 50vmin; font-weight: bold; color: rgba(255, 255, 255, 0.5); text-shadow: 2px 2px 8px rgba(0,0,0,0.5); }
        .mock-indicator { font-size: 12.5vmin; position: relative; top: -0.1em; }
        .connection-details { position: absolute; bottom: 10px; left: 10px; font-size: 14px; color: #FFFFFF; font-family: monospace; text-shadow: -1px -1px 0 #000, 1px -1px 0 #000, -1px 1px 0 #000, 1px 1px 0 #000; }
        @keyframes fade { 0%, 100% { opacity: 0.2; } 50% { opacity: 0.8; } }
        body.disconnected .input-number { animation: fade 2s infinite ease-in-out; }
        body.disconnected .mock-indicator { display: none; }
    </style>
</head>
<body class="off disconnected">
    <div class="input-number">)"
            + "<span>" + std::to_string(input_id) + "</span>" + (is_mock ? R"(<span class="mock-indicator"> (mock)</span>)" : "") + R"(</div>
    <div class="connection-details">
        <span id="server-details"></span> | Status: <span id="connection-status">Connecting...</span>
    </div>
    <script>
    const inputId = )"
            + std::to_string(input_id) + R"(;
    const isMock = )"
            + (is_mock ? "true" : "false") + R"(;
    const serverVersion = ")"
            + std::string(version::GIT_VERSION) + R"(";
    const sdkVersion = ")"
            + std::string(sdk_version) + R"(";

    let isConnected = false;

    function connect() {
        console.log('Attempting to connect to SSE endpoint...');
        const eventSource = new EventSource('/events');

        eventSource.onopen = () => {
            console.log('SSE connection opened.');
            // We will wait for the first message to set the status to "Connected"
        };

        eventSource.addEventListener('tally_update', (event) => {
            // console.log('Received tally_update event:', event.data);
            if (!isConnected) {
                isConnected = true;
                document.body.classList.remove('disconnected');
                document.getElementById('connection-status').textContent = 'Connected';
                console.log('Client is now marked as connected.');
            }
            const data = JSON.parse(event.data);
            if (data.input === inputId) {
                if (data.program) {
                    document.body.className = 'program';
                } else if (data.preview) {
                    document.body.className = 'preview';
                } else {
                    document.body.className = 'off';
                }
            }
        });

        eventSource.addEventListener('server_info', (event) => {
            console.log('Received server_info event:', event.data);
            const data = JSON.parse(event.data);
            if (data.server_version !== serverVersion) {
                console.log('Server version mismatch, reloading page.');
                location.reload();
            }
        });

        eventSource.addEventListener('mode_change', (event) => {
            console.log('Received mode_change event:', event.data);
            const data = JSON.parse(event.data);
            const mockIndicator = document.querySelector('.mock-indicator');

            if (data.mock && !mockIndicator) {
                // Add mock indicator
                const inputNumberSpan = document.querySelector('.input-number span');
                const newIndicator = document.createElement('span');
                newIndicator.className = 'mock-indicator';
                newIndicator.textContent = ' (mock)';
                inputNumberSpan.parentNode.appendChild(newIndicator);
            } else if (!data.mock && mockIndicator) {
                // Remove mock indicator
                mockIndicator.remove();
            }
            // Update the local state
            isMock = data.mock;
        });

        eventSource.onerror = (err) => {
            console.error('SSE connection error:', err);
            isConnected = false;
            document.body.className = 'off disconnected';
            document.getElementById('connection-status').textContent = 'Disconnected';
            eventSource.close();
            setTimeout(connect, 1000); // Try to reconnect after 1 second
        };
    }

    const serverDetails = `Server ${serverVersion} (SDK ${sdkVersion}) @ )"
            + std::string(server_ip) + R"(`;
    document.getElementById('server-details').textContent = serverDetails;

    connect();
</script>
</body>
</html>
)";
    }

} // namespace

SseServer::SseServer(const Config& config, gsl::not_null<TallyMonitor*> monitor)
    : config_(config)
    , monitor_(*monitor)
    , service_(std::make_shared<restbed::Service>())
{
    // Set a service-wide error handler to catch session closures.
    // This is the correct way to manage cleanup for persistent connections like SSE.
    service_->set_error_handler([this](const int, const std::exception&, const std::shared_ptr<restbed::Session> session) {
        if (session and session->is_open() == false) {
            const std::scoped_lock lock(sessions_mutex_);
            sse_sessions_.erase(session);
        }
    });
    setup_endpoints();
}

SseServer::~SseServer()
{
    stop();
}

void SseServer::start()
{
    auto settings = std::make_shared<restbed::Settings>();
    settings->set_port(config_.ws_port);
    settings->set_bind_address(config_.ws_address);
    settings->set_worker_limit(std::thread::hardware_concurrency());
    settings->set_connection_limit(config_.ws_connection_limit);

    std::cout << "SSE Server starting on " << config_.ws_address << ":" << config_.ws_port << std::endl;
    service_->start(settings);
}

void SseServer::stop()
{
    if (service_ && service_->is_up()) {
        std::cout << "Stopping SSE Server..." << std::endl;
        {
            // Close all SSE sessions before stopping the service
            std::scoped_lock lock(sessions_mutex_);
            for (const auto& session : sse_sessions_) {
                session->close();
            }
            sse_sessions_.clear();
        }
        service_->stop();
    }
}

void SseServer::broadcast_tally_update(const TallyUpdate& update)
{
    broadcast("tally_update", boost::json::serialize(boost::json::value_from(update))); // NOLINT(performance-move-const-arg)
}

void SseServer::broadcast_mode_change(bool is_mock)
{
    boost::json::object msg;
    msg["mock"] = is_mock;
    broadcast("mode_change", boost::json::serialize(boost::json::value_from(msg)));
}

void SseServer::setup_endpoints()
{
    // --- Index Page ---
    auto index_resource = std::make_shared<restbed::Resource>();
    index_resource->set_path("/");
    index_resource->set_method_handler("GET", [&](const std::shared_ptr<restbed::Session> session) {
        const auto body = generate_index_page(config_.mock_inputs);
        session->close(restbed::OK, body, { { "Content-Type", "text/html" }, { "Content-Length", std::to_string(body.length()) } }); });
    service_->publish(index_resource);

    // --- Status Page (All Inputs) ---
    auto status_resource = std::make_shared<restbed::Resource>();
    status_resource->set_path("/status");
    status_resource->set_method_handler("GET", [&](const std::shared_ptr<restbed::Session> session) {
        const auto request = session->get_request();
        const auto server_ip = request->get_header("Host", "localhost");
        const auto body = generate_status_page(config_.mock_inputs, server_ip, ATEM_SDK_VERSION);
        session->close(restbed::OK, body, { { "Content-Type", "text/html" }, { "Content-Length", std::to_string(body.length()) } });
    });
    service_->publish(status_resource);

    // --- Tally Page ---
    auto tally_resource = std::make_shared<restbed::Resource>();
    tally_resource->set_path("/tally/{id: \\d+}");
    tally_resource->set_method_handler("GET", [&](const std::shared_ptr<restbed::Session> session) {
        const auto request = session->get_request();
        const auto id = request->get_path_parameter("id", 0);
        const auto server_ip = request->get_header("Host", "localhost");

        const auto body = generate_tally_page(id, monitor_.is_mock_mode(), server_ip, ATEM_SDK_VERSION);
        session->close(restbed::OK, body, { { "Content-Type", "text/html" }, { "Content-Length", std::to_string(body.length()) } });
    });
    service_->publish(tally_resource);

    // --- SSE Events Endpoint ---
    auto sse_resource = std::make_shared<restbed::Resource>();
    sse_resource->set_path("/events");
    sse_resource->set_method_handler("GET", [this](const std::shared_ptr<restbed::Session> session) {
        // Add session to our list
        {
            const std::scoped_lock lock(sessions_mutex_);
            sse_sessions_.insert(session);
        }

        const std::multimap<std::string, std::string> headers = {
            { "Content-Type", "text/event-stream" },
            { "Cache-Control", "no-cache" },
            { "Connection", "keep-alive" }
        };

        // Send the headers to start the event stream.
        session->yield(restbed::OK, headers);

        // Send server info to the newly connected client for version checking
        {
            boost::json::object msg;
            msg["server_version"] = version::GIT_VERSION;
            session->yield("event: server_info\ndata: " + boost::json::serialize(boost::json::value_from(msg)) + "\n\n");
        }

        // Send initial state to the newly connected client
        for (const auto& state : monitor_.get_all_tally_states()) {
            const TallyUpdate update = state.to_update(monitor_.is_mock_mode());
            const auto data = boost::json::serialize(boost::json::value_from(update));
            const auto message = "event: tally_update\ndata: " + data + "\n\n";
            session->yield(message);
        }
    });

    service_->publish(sse_resource);
}

void SseServer::broadcast(const std::string& event, const std::string& data)
{
    const std::scoped_lock lock(sessions_mutex_);
    if (sse_sessions_.empty()) {
        return;
    }

    const auto message = "event: " + event + "\ndata: " + data + "\n\n";

    // `yield` is thread-safe, so we can call it directly.
    for (const auto& session : sse_sessions_) {
        if (session->is_open()) {
            session->yield(message);
        }
    }
}

} // namespace atem
