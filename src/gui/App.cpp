#include "gui/App.h"
#include "gui/MainFrame.h"
#include <boost/program_options.hpp>
#include <iostream>

wxIMPLEMENT_APP_NO_MAIN(atem::gui::App);

namespace atem {
namespace gui {

    namespace po = boost::program_options;

    bool App::OnInit()
    {
        // 1. Start with default configuration
        atem::Config config;

        // 2. Override with settings from the configuration file
        config.load_from_file("config/server_config.json");

        // 3. Override with command-line arguments
        ParseCommandLine(config);

        // 4. Create the main window, passing the final configuration.
        //    The UI can now further modify this configuration.
        auto* frame = new MainFrame("ATEM Tally Server", wxPoint(50, 50), wxSize(450, 350), config);
        frame->Show(true);
        return true;
    }

    void App::ParseCommandLine(atem::Config& config)
    {
        try {
            po::options_description desc("Allowed options");
            // clang-format off
            desc.add_options()
                ("help,h", "produce help message")
                ("atem-ip", po::value<std::string>(&config.atem_ip), "IP address of the ATEM switcher")
                ("ws-port", po::value<unsigned short>(&config.ws_port), "WebSocket server port")
                ("mock", po::bool_switch(&config.mock_enabled), "Enable mock mode, ignoring ATEM connection")
                ("mock-inputs", po::value<uint16_t>(&config.mock_inputs), "Number of inputs for mock mode")
            ;
            // clang-format on

            // We must convert wxWidgets' argv (a wxVector<wxString>) to a type that
            // Boost.Program_options can understand. The command_line_parser is more
            // flexible and can take a std::vector of strings.

            po::variables_map vm;
#if defined(__WXMSW__) && wxUSE_UNICODE
            std::vector<std::wstring> args;
            for (int i = 0; i < this->argc; ++i) {
                args.push_back(this->argv[i].ToStdWstring());
            }
            po::store(po::wcommand_line_parser(args).options(desc).run(), vm);
#else
            std::vector<std::string> args;
            for (int i = 0; i < this->argc; ++i) {
                args.push_back(this->argv[i].ToStdString());
            }
            po::store(po::command_line_parser(args).options(desc).run(), vm);
#endif
            po::notify(vm);

            if (vm.count("help")) {
                std::cout << "ATEM Tally Server\n";
                std::cout << desc << "\n";
                // Returning false from OnInit will exit the application
                wxExit();
            }

        } catch (const std::exception& e) {
            std::cerr << "Error parsing command line: " << e.what() << "\n";
        }
    }

} // namespace gui
} // namespace atem
