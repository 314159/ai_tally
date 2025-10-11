#pragma once

#include "tally_state.h"
#include <functional>
#include <string>
#include <vector>

namespace atem {

struct InputInfo {
    uint16_t id;
    std::string short_name;
    std::string long_name;
};

struct TallyUpdate; // Forward declaration

class IATEMConnection {
public:
    using TallyCallback = std::function<void(const TallyUpdate&)>;

    virtual ~IATEMConnection() = default;

    virtual bool connect(const std::string& ip_address) = 0;
    virtual void disconnect() = 0;
    virtual void poll() = 0;
    virtual void on_tally_change(TallyCallback callback) = 0;
    virtual bool is_mock_mode() const = 0;
    virtual uint16_t get_input_count() const = 0;
    [[nodiscard]] virtual std::vector<InputInfo> get_inputs() const = 0;
};

} // namespace atem
