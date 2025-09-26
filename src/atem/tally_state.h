#pragma once

#include <boost/json.hpp>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <unordered_map>

namespace atem {

struct TallyUpdate {
    uint16_t input_id;
    bool program;
    bool preview;
    bool mock = false;

    TallyUpdate() = default;
    TallyUpdate(uint16_t id, bool prog, bool prev, bool is_mock = false)
        : input_id(id)
        , program(prog)
        , preview(prev)
        , mock(is_mock)
    {
    }

    // Rule of 7 - explicitly defaulted
    TallyUpdate(const TallyUpdate&) = default;
    TallyUpdate& operator=(const TallyUpdate&) = default;
    TallyUpdate(TallyUpdate&&) = default;
    TallyUpdate& operator=(TallyUpdate&&) = default;
    ~TallyUpdate() = default;

    bool operator==(const TallyUpdate& other) const noexcept
    {
        return input_id == other.input_id && program == other.program && preview == other.preview && mock == other.mock;
    }

    bool operator!=(const TallyUpdate& other) const
    {
        return !(*this == other);
    }
};

// Provide a serialization mapping for TallyUpdate to Boost.JSON
inline void tag_invoke(const boost::json::value_from_tag&, boost::json::value& jv, const TallyUpdate& update)
{
    jv = {
        { "type", "tally_update" },
        { "input", update.input_id },
        { "program", update.program },
        { "preview", update.preview },
        { "mock", update.mock },
        { "timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() }
    };
}

struct TallyState {
    uint16_t input_id;
    bool program;
    bool preview;
    std::chrono::system_clock::time_point last_updated;

    TallyState() = default;
    TallyState(uint16_t id, bool prog, bool prev, std::chrono::system_clock::time_point updated)
        : input_id(id)
        , program(prog)
        , preview(prev)
        , last_updated(updated)
    {
    }

    // Rule of 7 - explicitly defaulted
    TallyState(const TallyState&) = default;
    TallyState& operator=(const TallyState&) = default;
    TallyState(TallyState&&) = default;
    TallyState& operator=(TallyState&&) = default;
    ~TallyState() = default;

    bool operator==(const TallyState& other) const noexcept
    {
        return input_id == other.input_id && program == other.program && preview == other.preview;
    }

    bool operator!=(const TallyState& other) const
    {
        return !(*this == other);
    }

    // Convert to TallyUpdate for broadcasting
    TallyUpdate to_update(bool is_mock = false) const
    {
        return TallyUpdate { input_id, program, preview, is_mock };
    }
};

} // namespace atem
