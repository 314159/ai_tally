#pragma once

#include "tally_state.h"
#include <functional>
#include <memory>
#include <string>

namespace atem {

class IATEMConnection {
public:
    using TallyCallback = std::function<void(const TallyUpdate&)>;

    virtual ~IATEMConnection() = default;

    virtual bool connect(const std::string& ip_address) = 0;
    virtual void disconnect() = 0;
    virtual void poll() = 0;
    virtual void on_tally_change(TallyCallback callback) = 0;
    virtual bool is_mock_mode() const = 0;
};

} // namespace atem
