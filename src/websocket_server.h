#pragma once

#include "config.h"
#include "gui/tally_provider.h"
#include "tally_state.h"
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <memory>
#include <mutex>
#include <set>

namespace atem {

namespace net = boost::asio;
using tcp = net::ip::tcp;
namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;

class WebSocketSession;

class HttpAndWebSocketServer : public std::enable_shared_from_this<HttpAndWebSocketServer> {
public:
    HttpAndWebSocketServer(net::io_context& ioc,
        net::ip::address address,
        unsigned short port,
        gui::ITallyProvider& tally_provider);

    HttpAndWebSocketServer(const HttpAndWebSocketServer&) = delete;
    HttpAndWebSocketServer& operator=(const HttpAndWebSocketServer&) = delete;
    HttpAndWebSocketServer(HttpAndWebSocketServer&&) = delete;
    HttpAndWebSocketServer& operator=(HttpAndWebSocketServer&&) = delete;

    void start();
    void stop();

    void broadcast_tally_update(const TallyUpdate& update);

    void join(WebSocketSession* session);
    void leave(WebSocketSession* session);

private:
    void do_accept();
    void on_accept(beast::error_code ec, tcp::socket socket);

    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    gui::ITallyProvider& tally_provider_;

    std::mutex mutex_;
    std::set<WebSocketSession*> sessions_;
};

} // namespace atem
