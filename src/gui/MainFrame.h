#pragma once

#include "config.h"
#include "gui/BackendService.h"
#include "gui/TallyPanel.h"
#include "tally_state.h"
#include <memory>
#include <wx/wx.h>

namespace atem {
namespace gui {

    /**
     * @class MainFrame
     * @brief The main window of the application.
     *
     * This frame contains the main controls for starting and stopping the server,
     * displaying the tally status, and accessing preferences. It communicates
     * with the BackendService to perform core application logic.
     */
    class MainFrame final : public wxFrame {
    public:
        MainFrame(const wxString& title, const wxPoint& pos, const wxSize& size, const Config& config);
        ~MainFrame() override;

        // Non-copyable, non-movable
        MainFrame(const MainFrame&) = delete;
        MainFrame& operator=(const MainFrame&) = delete;
        MainFrame(MainFrame&&) = delete;
        MainFrame& operator=(MainFrame&&) = delete;

    private:
        // Event Handlers
        void OnStartStop(wxCommandEvent& event);
        void OnPreferences(wxCommandEvent& event);
        void OnExit(wxCommandEvent& event);
        void OnClose(wxCloseEvent& event);

        // Backend Event Handlers (called from the main thread)
        void OnBackendUpdate(wxThreadEvent& event);
        void OnTallyUpdate(wxThreadEvent& event);
        void OnModeUpdate(wxThreadEvent& event);
        void OnBackendError(wxThreadEvent& event);

        // UI Update methods
        void SetServerState(bool running);
        void UpdateTallyDisplay(const std::vector<TallyState>& tallies);
        void UpdateTallyDisplay(const TallyUpdate& update);
        void UpdateModeDisplay(bool is_mock);
        void RecreateTallyIndicators();

        // Member variables
        Config config_; // Holds the application's current configuration
        std::unique_ptr<BackendService> backend_service_;

        // UI Elements
        wxButton* start_stop_button_;
        TallyPanel* tally_panel_;
        wxStaticText* mode_indicator_;

        wxDECLARE_EVENT_TABLE();
    };

    // Custom event types for thread-safe communication from backend to GUI
    // EVT_BACKEND_UPDATE: Sent when the server starts or stops. GetInt() is 1 for running, 0 for stopped.
    wxDEFINE_EVENT(EVT_BACKEND_UPDATE, wxThreadEvent);

    // EVT_TALLY_UPDATE: Sent when tally information changes. The payload is a TallyUpdate struct.
    // The event handler must clone the payload as the event object is destroyed after processing.
    wxDEFINE_EVENT(EVT_TALLY_UPDATE, wxThreadEvent);

    // EVT_MODE_UPDATE: Sent when the backend mode (Live/Mock) changes. GetInt() is 1 for mock, 0 for live.
    wxDEFINE_EVENT(EVT_MODE_UPDATE, wxThreadEvent);

    // EVT_BACKEND_ERROR: Sent when an unhandled exception occurs in the backend. GetString() contains the error message.
    wxDEFINE_EVENT(EVT_BACKEND_ERROR, wxThreadEvent);

    enum class EventId {
        StartStop = 1,
        Preferences,
        Exit,
    };

} // namespace gui
} // namespace atem
