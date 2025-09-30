#include "gui/PreferencesDialog.h"
#include <wx/valgen.h>
#include <wx/valnum.h>
#include <wx/valtext.h>

namespace atem {
namespace gui {

    // clang-format off
wxBEGIN_EVENT_TABLE(PreferencesDialog, wxDialog)
    EVT_BUTTON(wxID_OK, PreferencesDialog::OnOK)
wxEND_EVENT_TABLE();
    // clang-format on

    PreferencesDialog::PreferencesDialog(gsl::not_null<wxWindow*> parent, const wxString& title, const Config& config)
        : wxDialog(parent.get(), wxID_ANY, title, wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
        , initial_config_(config)
        , ws_address_(initial_config_.ws_address)
        , ws_port_(initial_config_.ws_port)
        , atem_ip_(initial_config_.atem_ip)
        , mock_enabled_(initial_config_.mock_enabled)
    {
        auto* main_sizer = new wxBoxSizer(wxVERTICAL);
        auto* form_sizer = new wxFlexGridSizer(2, wxSize(10, 5));
        form_sizer->AddGrowableCol(1, 1);

        // --- WebSocket Settings ---
        form_sizer->Add(new wxStaticText(this, wxID_ANY, "WebSocket Address:"), 0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT);
        auto* ws_address_ctrl = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, wxTextValidator(wxFILTER_NONE, &ws_address_));
        form_sizer->Add(ws_address_ctrl, 1, wxEXPAND);

        form_sizer->Add(new wxStaticText(this, wxID_ANY, "WebSocket Port:"), 0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT);
        auto* ws_port_ctrl = new wxTextCtrl(this, wxID_ANY, wxEmptyString);
        ws_port_ctrl->SetValidator(wxIntegerValidator<unsigned short>(&ws_port_));
        form_sizer->Add(ws_port_ctrl, 1, wxEXPAND);

        // --- ATEM Settings ---
        form_sizer->Add(new wxStaticText(this, wxID_ANY, "ATEM IP Address:"), 0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT);
        auto* atem_ip_ctrl = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, wxTextValidator(wxFILTER_NONE, &atem_ip_));
        form_sizer->Add(atem_ip_ctrl, 1, wxEXPAND);

        // --- Mock Mode Settings ---
        form_sizer->Add(new wxStaticText(this, wxID_ANY, "Enable Mock Mode:"), 0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT);
        auto* mock_enabled_ctrl = new wxCheckBox(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, wxGenericValidator(&mock_enabled_));
        form_sizer->Add(mock_enabled_ctrl, 0, wxEXPAND);

        form_sizer->Add(new wxStaticText(this, wxID_ANY, "Number of Mock Inputs:"), 0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT);
        mock_inputs_ctrl_ = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, 24, initial_config_.mock_inputs);
        form_sizer->Add(mock_inputs_ctrl_, 1, wxEXPAND);

        main_sizer->Add(form_sizer, 1, wxEXPAND | wxALL, 10);

        // --- Buttons ---
        auto* button_sizer = CreateButtonSizer(wxOK | wxCANCEL);
        main_sizer->Add(button_sizer, 0, wxALIGN_CENTER | wxBOTTOM, 10);

        SetSizerAndFit(main_sizer);
        Centre();
    }

    void PreferencesDialog::OnOK(wxCommandEvent& event)
    {
        // This method is primarily for validation before accepting.
        // The data transfer happens in GetConfig().
        if (Validate() && TransferDataFromWindow()) {
            event.Skip(); // Allow the dialog to close
        }
    }

    Config PreferencesDialog::GetConfig() const
    {
        Config new_config = initial_config_; // Start with old to preserve non-UI settings
        new_config.ws_address = ws_address_.ToStdString();
        new_config.ws_port = ws_port_;
        new_config.atem_ip = atem_ip_.ToStdString();
        new_config.mock_enabled = mock_enabled_;
        // Since there's no validator, we get the value directly from the control.
        if (mock_inputs_ctrl_) {
            new_config.mock_inputs = mock_inputs_ctrl_->GetValue();
        }
        return new_config;
    }

} // namespace gui
} // namespace atem
