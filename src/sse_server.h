#pragma once

#include "tally_state.h"
#include <gsl/gsl>
#include <memory>
#include <mutex>
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#include <restbed>
#pragma clang diagnostic pop
#include <string>
#include <unordered_set>

namespace atem {

struct Config;
class TallyMonitor;

class SseServer final {
public:
    SseServer(const Config& config, gsl::not_null<TallyMonitor*> monitor);
    ~SseServer();

    // Non-copyable, non-movable
    SseServer(const SseServer&) = delete;
    SseServer& operator=(const SseServer&) = delete;
    SseServer(SseServer&&) = delete;
    SseServer& operator=(SseServer&&) = delete;

    void start();
    void stop();

    void broadcast_tally_update(const TallyUpdate& update);
    void broadcast_mode_change(bool is_mock);

private:
    void setup_endpoints();
    void broadcast(const std::string& event, const std::string& data);

    const Config& config_;
    TallyMonitor& monitor_;
    const std::shared_ptr<restbed::Service> service_;
    std::unordered_set<std::shared_ptr<restbed::Session>> sse_sessions_;
    std::mutex sessions_mutex_;
};

} // namespace atem
