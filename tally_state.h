#pragma once

#include "atem/iatem_connection.h" // For TallyUpdate
#include <boost/json.hpp>
#include <chrono>
#include <cstdint>
#include <string>

namespace atem {

struct TallyState {
    uint16_t input_id;
    std::string short_name;
    bool program;
    bool preview;
    std::chrono::system_clock::time_point last_updated;

    [[nodiscard]] TallyUpdate to_update(bool is_mock) const;
};

// JSON serialization for TallyState
void tag_invoke(boost::json::value_from_tag, boost::json::value& jv, const TallyState& ts);

} // namespace atem
