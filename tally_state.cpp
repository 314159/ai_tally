#include "tally_state.h"

namespace atem {

TallyUpdate TallyState::to_update(bool is_mock) const
{
    return { input_id, short_name, program, preview, is_mock };
}

void tag_invoke(boost::json::value_from_tag, boost::json::value& jv, const TallyState& ts)
{
    jv = {
        { "input", ts.input_id },
        { "short_name", ts.short_name },
        { "program", ts.program },
        { "preview", ts.preview },
    };
}

} // namespace atem
