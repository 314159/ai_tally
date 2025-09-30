#include "gui/App.h"
#include <wx/wx.h>

// This is the platform-specific entry point for a wxWidgets application.
// It replaces the standard main() function.

#if defined(__WXMSW__)
#include <wx/msw/wx.h>
int WINAPI WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nShowCmd)
{
    wxDISABLE_DEBUG_SUPPORT();
    return wxEntry(hInstance, hPrevInstance, lpCmdLine, nShowCmd);
}
#else
int main(int argc, char** argv)
{
    return wxEntry(argc, argv);
}
#endif
