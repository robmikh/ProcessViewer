#include "pch.h"
#include "MainWindow.h"
#include <CommCtrl.h>

#define ID_LISTVIEW  2000 // ?????

const std::wstring MainWindow::ClassName = L"ProcessViewer.MainWindow";

wil::unique_handle GetProcessHandleFromPid(DWORD pid)
{
    return wil::unique_handle(winrt::check_pointer(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, false, pid)));
}

void MainWindow::RegisterWindowClass()
{
    auto instance = winrt::check_pointer(GetModuleHandleW(nullptr));
    WNDCLASSEXW wcex = {};
    wcex.cbSize = sizeof(wcex);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = instance;
    wcex.hIcon = LoadIconW(instance, IDI_APPLICATION);
    wcex.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszClassName = ClassName.c_str();
    wcex.hIconSm = LoadIconW(instance, IDI_APPLICATION);
    winrt::check_bool(RegisterClassExW(&wcex));
}

MainWindow::MainWindow(std::wstring const& titleString, int width, int height)
{
    auto instance = winrt::check_pointer(GetModuleHandleW(nullptr));

    winrt::check_bool(CreateWindowExW(0, ClassName.c_str(), titleString.c_str(), WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, width, height, nullptr, nullptr, instance, this));
    WINRT_ASSERT(m_window);

    m_processes = GetAllProcesses();

    CreateControls(instance);

    ShowWindow(m_window, SW_SHOWDEFAULT);
    UpdateWindow(m_window);
}

LRESULT MainWindow::MessageHandler(UINT const message, WPARAM const wparam, LPARAM const lparam)
{
    switch (message)
    {
    case WM_SIZE:
        ResizeProcessListView();
        break;
    case WM_NOTIFY:
        OnListViewNotify(lparam);
        break;
    }
    return base_type::MessageHandler(message, wparam, lparam);
}


std::vector<MainWindow::Process> MainWindow::GetAllProcesses()
{
    std::vector<Process> result;
    wil::unique_handle snapshot(winrt::check_pointer(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)));
    PROCESSENTRY32W entry = {};
    entry.dwSize = sizeof(entry);

    if (Process32FirstW(snapshot.get(), &entry))
    {
        do
        {
            if (auto process = CreateProcessFromProcessEntry(entry))
            {
                result.push_back(*process);
            }
        } while (Process32NextW(snapshot.get(), &entry));
    }
    return result;
}

MainWindow::Architecture MainWindow::GetArchitecture(USHORT value)
{
    switch (value)
    {
    case IMAGE_FILE_MACHINE_I386:
        return Architecture::x86;
    case IMAGE_FILE_MACHINE_AMD64:
        return Architecture::x64;
    case IMAGE_FILE_MACHINE_ARMNT:
        return Architecture::ARM;
    case IMAGE_FILE_MACHINE_ARM64:
        return Architecture::ARM64;
    default:
        return Architecture::Unknown;
    }
}

std::wstring MainWindow::GetArchitectureString(Architecture arch)
{
    switch (arch)
    {
    case Architecture::x86:
        return L"x86";
    case Architecture::x64:
        return L"x64";
    case Architecture::ARM:
        return L"ARM";
    case Architecture::ARM64:
        return L"ARM64";
    default:
        return L"Unknown";
    }
}

std::optional<MainWindow::Process> MainWindow::CreateProcessFromProcessEntry(PROCESSENTRY32W const& entry)
{
    std::wstring processName(entry.szExeFile);
    auto pid = entry.th32ProcessID;
    USHORT archValue = IMAGE_FILE_MACHINE_UNKNOWN;
    try
    {
        auto handle = GetProcessHandleFromPid(pid);
        USHORT process = 0;
        USHORT machine = 0;
        winrt::check_bool(IsWow64Process2(handle.get(), &process, &machine));
        archValue = process == IMAGE_FILE_MACHINE_UNKNOWN ? machine : process;
    }
    catch (winrt::hresult_error const& error)
    {
        if (error.code() == E_INVALIDARG)
        {
            return std::nullopt;
        }
        if (error.code() != E_ACCESSDENIED)
        {
            throw;
        }
    }
    return std::optional(Process{ pid, processName, archValue });
}

void MainWindow::CreateControls(HINSTANCE instance)
{
    auto style = WS_TABSTOP | WS_CHILD | WS_BORDER | WS_VISIBLE | LVS_AUTOARRANGE | LVS_REPORT | LVS_OWNERDATA;

    m_processListView = winrt::check_pointer(CreateWindowEx(
        0, //WS_EX_CLIENTEDGE,
        WC_LISTVIEW,
        L"",
        style,
        0, 0, 0, 0,
        m_window,
        (HMENU)ID_LISTVIEW,
        instance,
        nullptr));

    // Setup columns
    {
        LV_COLUMN column = {};
        std::vector<std::wstring> columnNames = 
        {
            L"Name",
            L"PID",
            L"Status",
            L"Architecture"
        };

        column.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
        column.fmt = LVCFMT_LEFT;
        column.cx = 120;
        for (auto i = 0; i < columnNames.size(); i++)
        {
            column.pszText = columnNames[i].data();
            ListView_InsertColumn(m_processListView, i, &column);
        }
        ListView_SetExtendedListViewStyle(m_processListView, LVS_EX_AUTOSIZECOLUMNS | LVS_EX_FULLROWSELECT);
    }

    // Add items
    ListView_SetItemCount(m_processListView, m_processes.size());

    ResizeProcessListView();
}

void MainWindow::ResizeProcessListView()
{
    RECT rect = {};
    winrt::check_bool(GetClientRect(m_window, &rect));
    winrt::check_bool(MoveWindow(
        m_processListView,
        rect.left,
        rect.top,
        rect.right - rect.left,
        rect.bottom - rect.top,
        true));
}

void MainWindow::OnListViewNotify(LPARAM const lparam)
{
    auto  lpnmh = (LPNMHDR)lparam;
    auto listView = winrt::check_pointer(GetDlgItem(m_window, ID_LISTVIEW));

    switch (lpnmh->code)
    {
    case LVN_GETDISPINFOW:
    {
        auto lpdi = (LV_DISPINFO*)lparam;
        auto itemIndex = lpdi->item.iItem;
        auto subItemIndex = lpdi->item.iSubItem;
        if (subItemIndex == 0)
        {
            if (lpdi->item.mask & LVIF_TEXT)
            {
                auto& process = m_processes[itemIndex];
                wcsncpy_s(lpdi->item.pszText, lpdi->item.cchTextMax, process.Name.data(), _TRUNCATE);
            }
        }
        else if (subItemIndex > 0)
        {
            if (lpdi->item.mask & LVIF_TEXT)
            {
                auto& process = m_processes[itemIndex];
                if (subItemIndex == 1)
                {
                    std::wstringstream stream;
                    stream << process.Pid;
                    auto pidString = stream.str();
                    wcsncpy_s(lpdi->item.pszText, lpdi->item.cchTextMax, pidString.data(), _TRUNCATE);
                }
                else if (subItemIndex == 2)
                {
                    // TODO: Get status (e.g. Running, Suspended)
                }
                else if (subItemIndex == 3)
                {
                    auto& process = m_processes[itemIndex];
                    auto pid = process.Pid;
                    std::wstring archString;
                    auto arch = GetArchitecture(process.ArchitectureValue);
                    if (arch != Architecture::Unknown)
                    {
                        archString = GetArchitectureString(arch);
                    }
                    else
                    {
                        std::wstringstream stream;
                        stream << L"Unknown"; 
                        if (process.ArchitectureValue > 0)
                        {
                            stream << L": 0x" << std::hex << std::setfill(L'0') << std::setw(4) << process.ArchitectureValue;
                        }
                        archString = stream.str();
                    }
                    wcsncpy_s(lpdi->item.pszText, lpdi->item.cchTextMax, archString.data(), _TRUNCATE);
                }
            }
        }
    }
        break;
    }
}
