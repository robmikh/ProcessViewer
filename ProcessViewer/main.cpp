#include "pch.h"
#include "MainWindow.h"

#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace winrt
{
    using namespace Windows::Foundation;
    using namespace Windows::Foundation::Numerics;
    using namespace Windows::UI;
    using namespace Windows::UI::Composition;
}

namespace util
{
    using namespace robmikh::common::desktop;
}

int __stdcall WinMain(HINSTANCE, HINSTANCE, PSTR, int)
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_UNAWARE_GDISCALED);

    // Initialize COM
    winrt::init_apartment();

    auto dispatcherQueueController = util::CreateDispatcherQueueControllerForCurrentThread();
    
    // Register our window classes
    MainWindow::RegisterWindowClass();

    // Create our window
    auto window = MainWindow(L"Process Viewer", 800, 600);

    // Message pump
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return static_cast<int>(msg.wParam);
}
