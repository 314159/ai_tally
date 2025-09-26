#pragma once

#include "tally_state.h"
#include <boost/asio.hpp>
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
    explicit HttpAndWebSocketSession(tcp::socket&& socket, const Config& config, TallyMonitor& monitor);
    ~HttpAndWebSocketSession();

    // Non-copyable, movable
    HttpAndWebSocketSession(const HttpAndWebSocketSession&) = delete;
    HttpAndWebSocketSession& operator=(const HttpAndWebSocketSession&) = delete;
    HttpAndWebSocketSession(HttpAndWebSocketSession&&) = default;
    HttpAndWebSocketSession& operator=(HttpAndWebSocketSession&&) = delete;

    void run();
    void send(std::string message);
    void close();

private:
    void do_http_read();
    void on_http_read(beast::error_code ec, std::size_t bytes_transferred);
    void handle_http_request(http::request<http::string_body>&& req);

    void on_send(std::string message);
    void on_ws_accept(beast::error_code ec);
    void do_read();
    void on_read(beast::error_code ec, std::size_t bytes_transferred);
    void on_write(beast::error_code ec, std::size_t bytes_transferred);

    websocket::stream<beast::tcp_stream> ws_;
    beast::flat_buffer buffer_;
    std::deque<std::shared_ptr<const std::string>> queue_;
    boost::optional<http::request_parser<http::string_body>> http_parser_;
    const Config& config_;
    TallyMonitor& monitor_;
};

class HttpAndWebSocketServer final {
public:
    HttpAndWebSocketServer(net::io_context& ioc, tcp::endpoint endpoint, const Config& config, TallyMonitor& monitor);
    HttpAndWebSocketServer(net::io_context& ioc, net::ip::address address, unsigned short port, const Config& config, TallyMonitor& monitor);
    ~HttpAndWebSocketServer();

    // Non-copyable, non-movable
    HttpAndWebSocketServer(const HttpAndWebSocketServer&) = delete;
    HttpAndWebSocketServer& operator=(const HttpAndWebSocketServer&) = delete;
    HttpAndWebSocketServer(HttpAndWebSocketServer&&) = delete;
    HttpAndWebSocketServer& operator=(HttpAndWebSocketServer&&) = delete;

    void start();
    void stop();
    void broadcast_tally_update(const TallyUpdate& update);

private:
    void do_accept();
    void on_accept(beast::error_code ec, tcp::socket socket);

    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    const Config& config_;
    TallyMonitor& monitor_;
    std::vector<std::weak_ptr<HttpAndWebSocketSession>> sessions_;
    std::mutex sessions_mutex_;
    bool running_ { false };
};

} // namespace atem
