#include "websocket_server.h"
#include "tally_state.h"
#include <boost/json.hpp>
#include <iostream>

namespace atem {

// Handles an HTTP GET request
template <class Body, class Allocator>
http::response<http::string_body> handle_request(http::request<Body, http::basic_fields<Allocator>>&& req)
{
    http::response<http::string_body> res { http::status::ok, req.version() };
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "text/html");
    res.keep_alive(req.keep_alive());
    res.body() = "ATEM Tally WebSocket Server is running.";
    res.prepare_payload();
    return res;
}

//------------------------------------------------------------------------------

class WebSocketSession : public std::enable_shared_from_this<WebSocketSession> {
    websocket::stream<beast::tcp_stream> ws_;
    HttpAndWebSocketServer& server_;
    std::vector<std::string> write_queue_;
    bool is_writing_ = false;

public:
    explicit WebSocketSession(tcp::socket&& socket, HttpAndWebSocketServer& server)
        : ws_(std::move(socket))
        , server_(server)
    {
    }

    void run(http::request<http::string_body> req)
    {
        ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));
        ws_.set_option(websocket::stream_base::decorator([](websocket::response_type& res) {
            res.set(http::field::server, std::string(BOOST_BEAST_VERSION_STRING) + " websocket-server-async");
        }));

        ws_.async_accept(req, beast::bind_front_handler(&WebSocketSession::on_accept, shared_from_this()));
    }

    void on_accept(beast::error_code ec)
    {
        if (ec) {
            std::cerr << "accept: " << ec.message() << "\n";
            return;
        }
        server_.join(this);
        do_read();
    }

    void do_read()
    {
        beast::flat_buffer buffer;
        ws_.async_read(buffer, beast::bind_front_handler(&WebSocketSession::on_read, shared_from_this()));
    }

    void on_read(beast::error_code ec, std::size_t)
    {
        if (ec == websocket::error::closed) {
            server_.leave(this);
            return;
        }
        if (ec) {
            std::cerr << "read: " << ec.message() << "\n";
            server_.leave(this);
            return;
        }
        // We ignore incoming messages for now.
        do_read();
    }

    void send(const std::string& message)
    {
        // Add the message to the queue
        write_queue_.push_back(message);

        // If a write is not already in progress, start one
        if (!is_writing_) {
            do_write();
        }
    }

    void on_write(beast::error_code ec, std::size_t)
    {
        is_writing_ = false;

        if (ec) {
            std::cerr << "write: " << ec.message() << "\n";
            server_.leave(this);
            return;
        }

        // If there are more messages, continue writing
        if (!write_queue_.empty()) {
            do_write();
        }
    }

    void do_write()
    {
        is_writing_ = true;
        ws_.async_write(net::buffer(write_queue_.front()), beast::bind_front_handler(&WebSocketSession::on_write, shared_from_this()));
        write_queue_.erase(write_queue_.begin());
    }
};

//------------------------------------------------------------------------------

class HttpSession : public std::enable_shared_from_this<HttpSession> {
    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    HttpAndWebSocketServer& server_;
    http::request<http::string_body> req_;

public:
    HttpSession(tcp::socket&& socket, HttpAndWebSocketServer& server)
        : stream_(std::move(socket))
        , server_(server)
    {
    }

    void run()
    {
        do_read();
    }

private:
    void do_read()
    {
        req_ = {};
        http::async_read(stream_, buffer_, req_, beast::bind_front_handler(&HttpSession::on_read, shared_from_this()));
    }

    void on_read(beast::error_code ec, std::size_t)
    {
        if (ec == http::error::end_of_stream) {
            return do_close();
        }
        if (ec) {
            std::cerr << "read: " << ec.message() << "\n";
            return;
        }

        if (websocket::is_upgrade(req_)) {
            std::make_shared<WebSocketSession>(stream_.release_socket(), server_)->run(std::move(req_));
            return;
        }

        http::response<http::string_body> res = handle_request(std::move(req_));
        http::async_write(stream_,
            std::move(res),
            [self = shared_from_this()](beast::error_code ec, std::size_t) {
                if (ec) {
                    std::cerr << "write: " << ec.message() << "\n";
                    return;
                }
                self->do_close();
            });
    }

    void do_close()
    {
        beast::error_code ec;
        stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
    }
};

//------------------------------------------------------------------------------

HttpAndWebSocketServer::HttpAndWebSocketServer(net::io_context& ioc,
    net::ip::address address,
    unsigned short port,
    gui::ITallyProvider& tally_provider)
    : ioc_(ioc)
    , acceptor_(ioc)
    , tally_provider_(tally_provider)
{
    tcp::endpoint endpoint(address, port);
    tally_provider_.add_tally_change_callback(
        [this](const TallyUpdate& update) { broadcast_tally_update(update); });

    beast::error_code ec;
    acceptor_.open(endpoint.protocol(), ec);
    if (ec) {
        throw std::runtime_error("acceptor open: " + ec.message());
    }
    acceptor_.set_option(net::socket_base::reuse_address(true), ec);
    if (ec) {
        throw std::runtime_error("acceptor set_option: " + ec.message());
    }
    acceptor_.bind(endpoint, ec);
    if (ec) {
        throw std::runtime_error("acceptor bind: " + ec.message());
    }
    acceptor_.listen(net::socket_base::max_listen_connections, ec);
    if (ec) {
        throw std::runtime_error("acceptor listen: " + ec.message());
    }
}

void HttpAndWebSocketServer::start()
{
    do_accept();
}

void HttpAndWebSocketServer::stop()
{
    acceptor_.close();
}

void HttpAndWebSocketServer::do_accept()
{
    acceptor_.async_accept(net::make_strand(ioc_), beast::bind_front_handler(&HttpAndWebSocketServer::on_accept, shared_from_this()));
}

void HttpAndWebSocketServer::on_accept(beast::error_code ec, tcp::socket socket)
{
    if (ec) {
        std::cerr << "accept: " << ec.message() << "\n";
    } else {
        std::make_shared<HttpSession>(std::move(socket), *this)->run();
    }
    if (acceptor_.is_open()) {
        do_accept();
    }
}

void HttpAndWebSocketServer::broadcast_tally_update(const TallyUpdate& update)
{
    boost::json::object jv;
    jv["inputId"] = update.input_id;
    jv["program"] = update.program;
    jv["preview"] = update.preview;
    std::string msg = boost::json::serialize(jv);

    std::lock_guard<std::mutex> lock(mutex_);
    for (auto* session : sessions_) {
        session->send(msg);
    }
}

void HttpAndWebSocketServer::join(WebSocketSession* session)
{
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.insert(session);
}

void HttpAndWebSocketServer::leave(WebSocketSession* session)
{
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.erase(session);
}

} // namespace atem
