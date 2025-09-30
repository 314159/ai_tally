#pragma once

#include "config.h"
#include <gsl/gsl>
#include <wx/spinctrl.h>
#include <wx/wx.h>

namespace atem {
namespace gui {

    /**
     * @class PreferencesDialog
     * @brief A dialog for editing application settings.
     *
     * This dialog provides controls to modify the settings stored in a Config
     * object. It is displayed modally from the MainFrame.
     */
    class PreferencesDialog final : public wxDialog {
    public:
        PreferencesDialog(gsl::not_null<wxWindow*> parent, const wxString& title, const Config& config);

        // Returns the updated configuration
        Config GetConfig() const;

    private:
        void OnOK(wxCommandEvent& event);

        Config initial_config_;

        // Data members for data transfer with validators
        wxString ws_address_;
        unsigned short ws_port_;
        wxString atem_ip_;
        bool mock_enabled_;

        // We need to store a pointer to this control to retrieve its value manually.
        wxSpinCtrl* mock_inputs_ctrl_ { nullptr };

        wxDECLARE_EVENT_TABLE();
    };

} // namespace gui
} // namespace atem
