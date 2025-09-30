#include "gui/MainFrame.h"
#include "gui/PreferencesDialog.h"
#include "version.h"
#include <wx/aboutdlg.h>
#include <wx/sizer.h>

#include <gsl/gsl>

namespace atem {
namespace gui {

    // clang-format off
    // Event table for MainFrame
    wxBEGIN_EVENT_TABLE(MainFrame, wxFrame)
        EVT_BUTTON(static_cast<int>(EventId::StartStop), MainFrame::OnStartStop)
        EVT_MENU(wxID_PREFERENCES, MainFrame::OnPreferences)
        EVT_MENU(wxID_EXIT, MainFrame::OnExit)
        EVT_CLOSE(MainFrame::OnClose)
        EVT_THREAD(EVT_BACKEND_UPDATE, MainFrame::OnBackendUpdate)
        EVT_THREAD(EVT_TALLY_UPDATE, MainFrame::OnTallyUpdate)
        EVT_THREAD(EVT_MODE_UPDATE, MainFrame::OnModeUpdate)
        EVT_THREAD(EVT_BACKEND_ERROR, MainFrame::OnBackendError)
    wxEND_EVENT_TABLE()
        // clang-format on

        MainFrame::MainFrame(const wxString& title, const wxPoint& pos, const wxSize& size, const Config& config)
        : wxFrame(nullptr, wxID_ANY, title, pos, size)
        , config_(config)
        , backend_service_(std::make_unique<BackendService>(this, config_))
    {
        // --- Menu Bar ---
        gsl::owner<wxMenu*> menu_file = new wxMenu;
        // On macOS, using wxID_PREFERENCES automatically moves this item to the app menu
        // and assigns the correct shortcut (Cmd+,). On other platforms, it remains here.
        menu_file->Append(wxID_PREFERENCES);
        menu_file->AppendSeparator();
        menu_file->Append(wxID_EXIT);

        gsl::owner<wxMenu*> menu_help = new wxMenu;
        menu_help->Append(wxID_ABOUT);

        gsl::owner<wxMenuBar*> menu_bar = new wxMenuBar;
        menu_bar->Append(menu_file, "&File");
        menu_bar->Append(menu_help, "&Help");
        SetMenuBar(menu_bar);

        // --- Main Panel and Sizer ---
        auto* panel = new wxPanel(this, wxID_ANY);
        auto* main_sizer = new wxBoxSizer(wxVERTICAL);

        // --- Mode Indicator ---
        mode_indicator_ = new wxStaticText(panel, wxID_ANY, "Mode: Unknown");
        main_sizer->Add(mode_indicator_, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, 5);

        // --- Tally Panel ---
        tally_panel_ = new TallyPanel(panel); // TallyPanel is a wxPanel, owned by its parent
        main_sizer->Add(tally_panel_, 1, wxEXPAND | wxALL, 10);

        // --- Start/Stop Button ---
        start_stop_button_ = new wxButton(panel, static_cast<int>(EventId::StartStop), "Start Server");
        main_sizer->Add(start_stop_button_, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, 10);

        panel->SetSizerAndFit(main_sizer);
        CreateStatusBar();
        SetStatusText("Welcome to ATEM Tally Server!");

        // Initialize UI state
        SetServerState(false);
        RecreateTallyIndicators();

        // Bind menu events not in the event table
        Bind(wxEVT_MENU, [this](wxCommandEvent&) {
                wxAboutDialogInfo about_info;
                about_info.SetName("ATEM Tally Server");
                about_info.SetVersion(wxString(atem::version::GIT_VERSION.data(), atem::version::GIT_VERSION.length()));
                about_info.SetDescription("Provides tally information from a Blackmagic ATEM switcher via WebSocket.");
                about_info.SetCopyright("(C) 2024 Your Name");
                wxAboutBox(about_info, this); }, wxID_ABOUT);
    }

    MainFrame::~MainFrame()
    {
        // The BackendService destructor will handle stopping the thread
    }

    void MainFrame::OnStartStop(wxCommandEvent& /*event*/)
    {
        if (backend_service_->IsRunning()) {
            backend_service_->Stop();
        } else {
            backend_service_->Start();
        }
    }

    void MainFrame::OnPreferences(wxCommandEvent& /*event*/)
    {
        PreferencesDialog dialog(this, "Preferences", config_);
        if (dialog.ShowModal() == wxID_OK) {
            config_ = dialog.GetConfig();
            config_.save_to_file("config/server_config.json");
            // Notify backend of config change
            backend_service_->SetConfig(config_);
            RecreateTallyIndicators();
        }
    }

    void MainFrame::OnExit(wxCommandEvent& /*event*/)
    {
        Close(true);
    }

    void MainFrame::OnClose(wxCloseEvent& /*event*/)
    {
        // This ensures the backend thread is cleanly shut down before the window is destroyed.
        backend_service_.reset();
        Destroy();
    }

    void MainFrame::OnBackendUpdate(wxThreadEvent& event)
    {
        SetServerState(event.GetInt() != 0);
    }

    void MainFrame::OnTallyUpdate(wxThreadEvent& event)
    {
        // We must clone the payload as the event object will be destroyed after this handler.
        // Using a unique_ptr ensures the payload is deleted automatically.
        auto payload = std::unique_ptr<TallyUpdate>(event.GetPayload<TallyUpdate*>());
        if (payload) {
            UpdateTallyDisplay(*payload);
        }
    }

    void MainFrame::OnModeUpdate(wxThreadEvent& event)
    {
        UpdateModeDisplay(event.GetInt() != 0);
    }

    void MainFrame::OnBackendError(wxThreadEvent& event)
    {
        wxMessageBox("The backend service encountered a critical error:\n\n" + event.GetString(), "Backend Error", wxOK | wxICON_ERROR, this);
    }

    void MainFrame::SetServerState(bool running)
    {
        if (running) {
            gsl::not_null(start_stop_button_)->SetLabel("Stop Server");
            SetStatusText(wxString::Format("Server is running on %s:%d", config_.ws_address, config_.ws_port));
            GetMenuBar()->FindItem(wxID_PREFERENCES)->Enable(false);
        } else {
            gsl::not_null(start_stop_button_)->SetLabel("Start Server");
            SetStatusText("Server is stopped.");
            GetMenuBar()->FindItem(wxID_PREFERENCES)->Enable(true);
        }
    }

    void MainFrame::UpdateTallyDisplay(const std::vector<TallyState>& tallies)
    {
        for (const auto& tally : tallies) {
            tally_panel_->UpdateTally(tally.input_id, tally.program, tally.preview);
        }
    }

    void MainFrame::UpdateTallyDisplay(const TallyUpdate& update)
    {
        // This is a temporary compatibility function. The backend should ideally send the full state.
        // For now, we update the single tally state in the panel directly.
        tally_panel_->UpdateTally(update.input_id, update.program, update.preview);
    }

    void MainFrame::UpdateModeDisplay(bool is_mock)
    {
        mode_indicator_->SetLabel(is_mock ? "Mode: Mock" : "Mode: Live (ATEM)");
    }

    void MainFrame::RecreateTallyIndicators()
    {
        tally_panel_->CreateIndicators(config_.mock_inputs);
        Layout(); // Recalculate layout after adding/removing controls
    }

} // namespace gui
} // namespace atem
