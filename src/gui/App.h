#pragma once

#include "config.h"
#include <wx/app.h>

namespace atem {
namespace gui {

    /**
     * @class App
     * @brief The main wxWidgets application class.
     *
     * This class is the entry point for the GUI application. It creates the main
     * application frame upon initialization.
     */
    class App final : public wxApp {
    public:
        // This method is called on application startup and is a good place
        // for initialization code.
        bool OnInit() override;

    private:
        void ParseCommandLine(atem::Config& config);
    };

} // namespace gui
} // namespace atem
