#include "gui/TallyPanel.h"
#include <wx/wrapsizer.h>

namespace atem {
namespace gui {

    namespace {
        // Define colors for tally states
        const wxColour OFF_COLOUR(45, 45, 45);
        const wxColour PREVIEW_COLOUR(0, 153, 0); // Green
        const wxColour PROGRAM_COLOUR(255, 0, 0); // Red
        const wxColour TEXT_COLOUR(255, 255, 255);
    } // namespace

    TallyPanel::TallyPanel(wxWindow* parent)
        : wxPanel(parent, wxID_ANY)
    {
        SetBackgroundColour(*wxBLACK);
        auto* sizer = new wxWrapSizer(wxHORIZONTAL, wxWRAPSIZER_DEFAULT_FLAGS);
        SetSizer(sizer);
    }

    void TallyPanel::CreateIndicators(uint16_t count)
    {
        GetSizer()->Clear(true);
        indicators_.clear();

        for (uint16_t i = 1; i <= count; ++i) {
            auto* indicator = new wxStaticText(this, wxID_ANY, std::to_string(i), wxDefaultPosition, wxSize(60, 40), wxALIGN_CENTER | wxST_NO_AUTORESIZE);
            indicator->SetFont(wxFont(14, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD));
            indicator->SetForegroundColour(TEXT_COLOUR);
            indicator->SetBackgroundColour(OFF_COLOUR);
            GetSizer()->Add(indicator, 0, wxALL, 5);
            indicators_[i] = indicator;
        }

        GetSizer()->Layout();
        Refresh();
    }

    void TallyPanel::UpdateTally(uint16_t input_id, bool is_program, bool is_preview)
    {
        auto it = indicators_.find(input_id);
        if (it == indicators_.end()) {
            return; // No indicator for this ID
        }

        wxStaticText* indicator = it->second;
        if (is_program) {
            indicator->SetBackgroundColour(PROGRAM_COLOUR);
        } else if (is_preview) {
            indicator->SetBackgroundColour(PREVIEW_COLOUR);
        } else {
            indicator->SetBackgroundColour(OFF_COLOUR);
        }
        indicator->Refresh();
    }

    void TallyPanel::ClearAll()
    {
        for (auto const& [id, indicator] : indicators_) {
            if (indicator) {
                indicator->SetBackgroundColour(OFF_COLOUR);
                indicator->Refresh();
            }
        }
    }

} // namespace gui
} // namespace atem
