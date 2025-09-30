#pragma once

#include <cstdint>
#include <unordered_map>
#include <wx/panel.h>
#include <wx/stattext.h>

namespace atem {
namespace gui {

    /**
     * @class TallyPanel
     * @brief A custom panel to display tally light indicators.
     *
     * This panel dynamically creates and manages a grid of indicators,
     * each representing an input. The color of each indicator is updated
     * based on tally state changes (Off, Preview, Program).
     */
    class TallyPanel final : public wxPanel {
    public:
        explicit TallyPanel(wxWindow* parent);

        // Creates or recreates the indicator widgets based on the number of inputs.
        void CreateIndicators(uint16_t count);

        // Updates the color and text of a specific tally indicator.
        void UpdateTally(uint16_t input_id, bool is_program, bool is_preview);

        // Resets all indicators to their default 'off' state.
        void ClearAll();

    private:
        std::unordered_map<uint16_t, wxStaticText*> indicators_;
    };

} // namespace gui
} // namespace atem
