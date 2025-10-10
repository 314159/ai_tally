#pragma once

#include "tally_state.h"
#include <boost/asio.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <deque>
#include <gsl/gsl>
#include <memory>
#include <mutex>
#include <vector>

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;

namespace atem {
struct Config;
class TallyMonitor;

class HttpAndWebSocketSession final : public std::enable_shared_from_this<HttpAndWebSocketSession> {
public:
    explicit HttpAndWebSocketSession(tcp::socket&& socket, const Config& config, gsl::not_null<TallyMonitor*> monitor);
    ~HttpAndWebSocketSession() noexcept;

    // Non-copyable, movable
    HttpAndWebSocketSession(const HttpAndWebSocketSession&) = delete;
    HttpAndWebSocketSession& operator=(const HttpAndWebSocketSession&) = delete;
    HttpAndWebSocketSession(HttpAndWebSocketSession&&) = delete;
    HttpAndWebSocketSession& operator=(HttpAndWebSocketSession&&) = delete;

    void start();
    void send(std::shared_ptr<const std::string> message);
    void close();
    auto get_strand()
    {
        return strand_;
    }

private:
    boost::asio::awaitable<void> run();
    boost::asio::awaitable<void> reader();
    boost::asio::awaitable<void> writer();
    void send_json(const boost::json::value& jv);
    void handle_http_request(http::request<http::string_body>&& req);

    websocket::stream<beast::tcp_stream> ws_;
    beast::flat_buffer buffer_;
    net::strand<net::any_io_executor> strand_;
    std::deque<std::shared_ptr<const std::string>> queue_;
    net::steady_timer writer_timer_;
    std::string remote_endpoint_str_;
    boost::optional<http::request_parser<http::string_body>> http_parser_;
    const Config& config_;
    TallyMonitor& monitor_;
};

class HttpAndWebSocketServer final {
public:
    HttpAndWebSocketServer(net::io_context& ioc, tcp::endpoint endpoint, const Config& config, gsl::not_null<TallyMonitor*> monitor);
    HttpAndWebSocketServer(net::io_context& ioc, net::ip::address address, unsigned short port, const Config& config, gsl::not_null<TallyMonitor*> monitor);
    ~HttpAndWebSocketServer() noexcept;

    // Non-copyable, non-movable
    HttpAndWebSocketServer(const HttpAndWebSocketServer&) = delete;
    HttpAndWebSocketServer& operator=(const HttpAndWebSocketServer&) = delete;
    HttpAndWebSocketServer(HttpAndWebSocketServer&&) = delete;
    HttpAndWebSocketServer& operator=(HttpAndWebSocketServer&&) = delete;

    void start();
    void stop() noexcept;
    void broadcast_tally_update(const TallyUpdate& update);
    void broadcast_mode_change(bool is_mock);

private:
    void do_accept();
    void on_accept(beast::error_code ec, tcp::socket socket);
    void broadcast(const std::shared_ptr<const std::string>& message);

    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    const Config& config_;
    TallyMonitor& monitor_;
    std::vector<std::weak_ptr<HttpAndWebSocketSession>> sessions_;
    std::mutex sessions_mutex_;
    bool running_ { false };
};

} // namespace atem
