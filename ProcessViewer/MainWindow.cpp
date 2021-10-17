#include "pch.h"
#include "MainWindow.h"
#include <shellapi.h>

namespace winrt
{
    using namespace Windows::Foundation;
    using namespace Windows::Storage::Pickers;
    using namespace Windows::System;
    using namespace Windows::UI::Popups;
}

namespace winmd
{
    using namespace winmd::reader;
}

#define ID_LISTVIEW  2000 // ?????

const std::wstring MainWindow::ClassName = L"ProcessViewer.MainWindow";

std::string GetStringFromPath(winrt::hstring const& path);

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
    m_dispatcherQueue = winrt::DispatcherQueue::GetForCurrentThread();

    winrt::check_bool(CreateWindowExW(0, ClassName.c_str(), titleString.c_str(), WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, width, height, nullptr, nullptr, instance, this));
    WINRT_ASSERT(m_window);

    m_columns =
    {
        ProcessInformation::Name,
        ProcessInformation::Pid,
        ProcessInformation::Type,
        ProcessInformation::Architecture,
        ProcessInformation::IntegrityLevel,
    };
    m_processes = GetAllProcesses();

    CreateMenuBar();
    CreateControls(instance);

    ShowWindow(m_window, SW_SHOWDEFAULT);
    UpdateWindow(m_window);

    m_processWatcher = std::make_unique<ProcessWatcher>(m_dispatcherQueue, 
        ProcessWatcher::ProcessAddedCallback([&](Process process)
        {
            InsertProcess(process);
        }),
        ProcessWatcher::ProcessRemovedCallback([&](DWORD processId)
        {
            RemoveProcessByProcessId(processId);
        }));
}

bool MainWindow::CompareProcesses(
    Process const& left, 
    Process const& right,
    ColumnSorting const& sort,
    ProcessInformation const& column)
{
    switch (column)
    {
    case ProcessInformation::Pid:
        if (sort == ColumnSorting::Ascending)
        {
            return left.Pid < right.Pid;
        }
        else
        {
            return left.Pid > right.Pid;
        }
    case ProcessInformation::Name:
    {
        auto value = _wcsicmp(left.Name.c_str(), right.Name.c_str());
        if (sort == ColumnSorting::Ascending)
        {
            return value < 0;
        }
        else
        {
            return value > 0;
        }
    }
    case ProcessInformation::Type:
        if (sort == ColumnSorting::Ascending)
        {
            if (!left.Type.has_value())
            {
                return true;
            }
            else if (!right.Type.has_value())
            {
                return false;
            }
            else
            {
                return *left.Type < *right.Type;
            }
        }
        else
        {
            if (!left.Type.has_value())
            {
                return false;
            }
            else if (!right.Type.has_value())
            {
                return true;
            }
            else
            {
                return *left.Type > *right.Type;
            }
        }
    case ProcessInformation::Architecture:
        if (sort == ColumnSorting::Ascending)
        {
            return left.ArchitectureValue < right.ArchitectureValue;
        }
        else
        {
            return left.ArchitectureValue > right.ArchitectureValue;
        }
    case ProcessInformation::IntegrityLevel:
        if (sort == ColumnSorting::Ascending)
        {
            if (!left.IntegrityLevel.has_value())
            {
                return true;
            }
            else if (!right.IntegrityLevel.has_value())
            {
                return false;
            }
            else
            {
                return *left.IntegrityLevel < *right.IntegrityLevel;
            }
        }
        else
        {
            if (!left.IntegrityLevel.has_value())
            {
                return false;
            }
            else if (!right.IntegrityLevel.has_value())
            {
                return true;
            }
            else
            {
                return *left.IntegrityLevel > *right.IntegrityLevel;
            }
        }
    default:
        std::abort();
    }
}

std::vector<Process>::iterator MainWindow::GetProcessInsertIterator(Process const& process)
{
    auto sort = m_columnSort;
    auto& column = m_columns[m_selectedColumnIndex];
    auto newIndex = std::lower_bound(m_processes.begin(), m_processes.end(), process, [sort, column](Process const& process1, Process const& process2)
        {
            return CompareProcesses(process1, process2, sort, column);
        });
    return newIndex;
}

void MainWindow::InsertProcess(Process const& process)
{
    if (m_viewAccessibleProcess || process.ArchitectureValue != IMAGE_FILE_MACHINE_UNKNOWN)
    {
        auto newIndex = GetProcessInsertIterator(process);
        LVITEMW item = {};
        item.iItem = static_cast<int>(newIndex - m_processes.begin());
        //item.iImage = I_IMAGECALLBACK;
        m_processes.insert(newIndex, process);
        ListView_InsertItem(m_processListView, &item);
        EnsureProcessIcon(process.ExecutablePath);
    }
}

void MainWindow::RemoveProcessByProcessId(DWORD processId)
{
    auto it = std::find_if(m_processes.begin(), m_processes.end(), [processId](Process const& process)
        {
            return process.Pid == processId;
        });

    if (it != m_processes.end())
    {
        auto index = it - m_processes.begin();
        m_processes.erase(it);
        ListView_DeleteItem(m_processListView, index);
    }
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
    case WM_MENUCOMMAND:
    {
        auto menu = reinterpret_cast<HMENU>(lparam);
        //int index = static_cast<int>(wparam);
        if (menu == m_fileMenu.get())
        {
            // TODO: Check the index, but currently there's only one item
            PostQuitMessage(0);
        }
        else if (menu == m_viewMenu.get())
        {
            // TODO: Check the index, but currently there's only one item
            m_viewAccessibleProcess = !m_viewAccessibleProcess;
            auto flag = m_viewAccessibleProcess ? MF_CHECKED : MF_UNCHECKED;
            CheckMenuItem(m_viewMenu.get(), 0, flag);
            auto sort = m_columnSort;
            auto& column = m_columns[m_selectedColumnIndex];
            m_processes = GetAllProcesses(m_viewAccessibleProcess); std::sort(m_processes.begin(), m_processes.end(), [sort, column](Process const& process1, Process const& process2)
                {
                    return CompareProcesses(process1, process2, sort, column);
                });
            ListView_SetItemCount(m_processListView, m_processes.size());
            ListView_RedrawItems(m_processListView, 0, m_processes.size() - 1);
            ListView_Scroll(m_processListView, 0, 0);
            ListView_SetItemState(m_processListView, -1, 0, LVIS_SELECTED);
            ListView_SetItemState(m_processListView, -1, 0, LVIS_FOCUSED);
        }
        else if (menu == m_toolsMenu.get())
        {
            // TODO: Check the index, but currently there's only one item
            CheckBinaryArchitecture();
        }
        else if (menu == m_helpMenu.get())
        {
            // TODO: Check the index, but currently there's only one item
            auto ignored = ShowAboutAsync();
        }
    }
        break;
    }
    return base_type::MessageHandler(message, wparam, lparam);
}

void MainWindow::CreateMenuBar()
{
    m_menuBar.reset(winrt::check_pointer(CreateMenu()));
    m_fileMenu.reset(winrt::check_pointer(CreatePopupMenu()));
    m_viewMenu.reset(winrt::check_pointer(CreatePopupMenu()));
    m_toolsMenu.reset(winrt::check_pointer(CreatePopupMenu()));
    m_helpMenu.reset(winrt::check_pointer(CreatePopupMenu()));
    winrt::check_bool(AppendMenuW(m_menuBar.get(), MF_POPUP, reinterpret_cast<UINT_PTR>(m_fileMenu.get()), L"File"));
    winrt::check_bool(AppendMenuW(m_fileMenu.get(), MF_STRING, 0, L"Exit"));
    winrt::check_bool(AppendMenuW(m_menuBar.get(), MF_POPUP, reinterpret_cast<UINT_PTR>(m_viewMenu.get()), L"View"));
    winrt::check_bool(AppendMenuW(m_viewMenu.get(), MF_STRING | MF_CHECKED, 0, L"View inaccessible processes"));
    winrt::check_bool(AppendMenuW(m_menuBar.get(), MF_POPUP, reinterpret_cast<UINT_PTR>(m_toolsMenu.get()), L"Tools"));
    winrt::check_bool(AppendMenuW(m_toolsMenu.get(), MF_STRING, 0, L"Check binary architecture"));
    winrt::check_bool(AppendMenuW(m_menuBar.get(), MF_POPUP, reinterpret_cast<UINT_PTR>(m_helpMenu.get()), L"Help"));
    winrt::check_bool(AppendMenuW(m_helpMenu.get(), MF_STRING, 0, L"About"));
    winrt::check_bool(SetMenu(m_window, m_menuBar.get()));
    MENUINFO menuInfo = {};
    menuInfo.cbSize = sizeof(menuInfo);
    menuInfo.fMask = MIM_STYLE;
    menuInfo.dwStyle = MNS_NOTIFYBYPOS;
    winrt::check_bool(SetMenuInfo(m_menuBar.get(), &menuInfo));
    winrt::check_bool(SetMenuInfo(m_viewMenu.get(), &menuInfo));
    winrt::check_bool(SetMenuInfo(m_fileMenu.get(), &menuInfo));
    winrt::check_bool(SetMenuInfo(m_toolsMenu.get(), &menuInfo));
    winrt::check_bool(SetMenuInfo(m_helpMenu.get(), &menuInfo));
}

void MainWindow::CreateControls(HINSTANCE instance)
{
    auto style = WS_TABSTOP | WS_CHILD | WS_BORDER | WS_VISIBLE | LVS_AUTOARRANGE | LVS_REPORT | LVS_OWNERDATA | LVS_SHOWSELALWAYS | LVS_SINGLESEL;

    m_processListView = winrt::check_pointer(CreateWindowExW(
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
        LVCOLUMNW listViewColumn = {};
        std::vector<std::wstring> columnNames;
        for (auto&& column : m_columns)
        {
            std::wstringstream stream;
            stream << column;
            columnNames.push_back(stream.str());
        }

        listViewColumn.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
        listViewColumn.fmt = LVCFMT_LEFT;
        listViewColumn.cx = 120;
        for (auto i = 0; i < columnNames.size(); i++)
        {
            listViewColumn.pszText = columnNames[i].data();
            ListView_InsertColumn(m_processListView, i, &listViewColumn);
        }
        ListView_SetExtendedListViewStyle(m_processListView, LVS_EX_AUTOSIZECOLUMNS | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    }

    // Setup icon list
    ResetProcessIconsCache();

    // Add items
    ListView_SetItemCount(m_processListView, m_processes.size());

    ResizeProcessListView();
}

void MainWindow::ResizeProcessListView()
{
    if (m_processListView)
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
}

void MainWindow::OnListViewNotify(LPARAM const lparam)
{
    auto  lpnmh = reinterpret_cast<LPNMHDR>(lparam);
    //auto listView = winrt::check_pointer(GetDlgItem(m_window, ID_LISTVIEW));

    switch (lpnmh->code)
    {
    case LVN_GETDISPINFOW:
    {
        auto itemDisplayInfo = reinterpret_cast<NMLVDISPINFOW*>(lparam);
        auto itemIndex = itemDisplayInfo->item.iItem;
        auto subItemIndex = itemDisplayInfo->item.iSubItem;
        if (subItemIndex == 0)
        {
            if (itemDisplayInfo->item.mask & LVIF_TEXT)
            {
                auto& process = m_processes[itemIndex];
                wcsncpy_s(itemDisplayInfo->item.pszText, itemDisplayInfo->item.cchTextMax, process.Name.data(), _TRUNCATE);
            }
            if (itemDisplayInfo->item.mask & LVIF_IMAGE)
            {
                auto& process = m_processes[itemIndex];
                auto search = m_pathToIconIndex.find(process.ExecutablePath);
                int iconIndex = 0;
                if (search != m_pathToIconIndex.end())
                {
                    iconIndex = static_cast<int>(search->second);
                }
                itemDisplayInfo->item.iImage = iconIndex;
            }
        }
        else if (subItemIndex > 0)
        {
            if (itemDisplayInfo->item.mask & LVIF_TEXT)
            {
                auto& process = m_processes[itemIndex];
                auto& column = m_columns[subItemIndex];
                std::wstringstream stream;
                switch (column)
                {
                case ProcessInformation::Pid:
                    stream << process.Pid;
                    break;
                case ProcessInformation::Name:
                    stream << process.Name;
                    break;
                case ProcessInformation::Type:
                    stream << process.Type;
                    break;
                case ProcessInformation::Architecture:
                    stream << process.GetArchitecture();
                    break;
                case ProcessInformation::IntegrityLevel:
                    stream << process.IntegrityLevel;
                }
                auto string = stream.str();
                wcsncpy_s(itemDisplayInfo->item.pszText, itemDisplayInfo->item.cchTextMax, string.data(), _TRUNCATE);
            }
        }
    }
        break;
    case LVN_COLUMNCLICK:
    {
        auto messageInfo = (LPNMLISTVIEW)lparam;
        auto columnIndex = messageInfo->iSubItem;
        if (columnIndex != m_selectedColumnIndex)
        {
            m_columnSort = ColumnSorting::Ascending;
        }
        else
        {
            m_columnSort = m_columnSort == ColumnSorting::Ascending ? ColumnSorting::Descending : ColumnSorting::Ascending;
        }
        auto sort = m_columnSort;
        m_selectedColumnIndex = columnIndex;
        auto& column = m_columns[m_selectedColumnIndex];

        std::sort(m_processes.begin(), m_processes.end(), [sort, column](Process const& process1, Process const& process2)
            {
                return CompareProcesses(process1, process2, sort, column);
            });
        ListView_RedrawItems(m_processListView, 0, m_processes.size() - 1);
        ListView_Scroll(m_processListView, 0, 0);
        ListView_SetItemState(m_processListView, -1, 0, LVIS_SELECTED);
        ListView_SetItemState(m_processListView, -1, 0, LVIS_FOCUSED);
    }
        break;
    }
}

void MainWindow::ResetProcessIconsCache()
{
    m_imageList.reset(winrt::check_pointer(ImageList_Create(
        GetSystemMetrics(SM_CXICON) / 2,
        GetSystemMetrics(SM_CYICON) / 2,
        ILC_MASK | ILC_COLOR32, 1, 1)));
    m_icons.clear();
    // Create a default icon
    SHSTOCKICONINFO iconInfo = {};
    iconInfo.cbSize = sizeof(iconInfo);
    winrt::check_hresult(SHGetStockIconInfo(SIID_APPLICATION, SHGSI_ICON, &iconInfo));
    wil::shared_hicon defaultIcon(winrt::check_pointer(iconInfo.hIcon));
    m_icons.push_back(defaultIcon);
    ImageList_AddIcon(m_imageList.get(), defaultIcon.get());
    for (auto&& process : m_processes)
    {
        EnsureProcessIcon(process.ExecutablePath);
    }
    ListView_SetImageList(m_processListView, m_imageList.get(), LVSIL_SMALL);
}

void MainWindow::EnsureProcessIcon(std::wstring const& exePath)
{
    if (!exePath.empty())
    {
        auto search = m_pathToIconIndex.find(exePath);
        if (search == m_pathToIconIndex.end())
        {
            auto instance = winrt::check_pointer(GetModuleHandleW(nullptr));
            wil::shared_hicon exeIcon(ExtractIconW(instance, exePath.c_str(), 0));
            if (exeIcon.is_valid())
            {
                auto index = m_icons.size();
                m_icons.push_back(exeIcon);
                m_pathToIconIndex.insert({ exePath, index });
                ImageList_AddIcon(m_imageList.get(), exeIcon.get());
            }
            else
            {
                auto error = GetLastError();
                if (!(error == ERROR_RESOURCE_TYPE_NOT_FOUND || error == ERROR_RESOURCE_DATA_NOT_FOUND))
                {
                    winrt::throw_last_error();
                }
            }
        }
    }
}

winrt::fire_and_forget MainWindow::CheckBinaryArchitecture()
{
    auto picker = winrt::FileOpenPicker();
    InitializeObjectWithWindowHandle(picker);
    picker.SuggestedStartLocation(winrt::PickerLocationId::ComputerFolder);
    picker.FileTypeFilter().Append(L".dll");
    picker.FileTypeFilter().Append(L".exe");
    auto file = co_await picker.PickSingleFileAsync();

    if (file != nullptr)
    {
        co_await m_dispatcherQueue;

        auto name = file.Name();
        auto path = GetStringFromPath(file.Path());
        winmd::file_view view(path);

        uint16_t machine = IMAGE_FILE_MACHINE_UNKNOWN;
        try
        {
            // adapted from https://github.com/microsoft/winmd/blob/ab1436427ede293ddad944d3688e83b3fba3a173/src/impl/winmd_reader/database.h#L229
            auto& dos = view.as<winmd::impl::image_dos_header>();
            if (dos.e_signature != 0x5A4D) // IMAGE_DOS_SIGNATURE
            {
                winmd::impl::throw_invalid("Invalid DOS signature");
            }

            auto& pe = view.as<winmd::impl::image_nt_headers32>(dos.e_lfanew);
            if (pe.FileHeader.NumberOfSections == 0 || pe.FileHeader.NumberOfSections > 100)
            {
                winmd::impl::throw_invalid("Invalid PE section count");
            }

            machine = pe.FileHeader.Machine;
        }
        catch (std::invalid_argument const& error)
        {
            MessageBoxA(m_window, error.what(), "Process Viewer", MB_OK | MB_ICONERROR);
            co_return;
        }
        auto architecture = MachineValueToArchitecture(machine);

        std::wstringstream stream;
        stream << name.c_str() << L" targets " << architecture;
        auto message = stream.str();
        MessageBoxW(m_window, message.c_str(), L"Process Viewer", MB_OK);
    }
    co_return;
}

winrt::fire_and_forget MainWindow::ShowAboutAsync()
{
    auto dialog = winrt::MessageDialog(L"ProcessViewer is an open source application written by Robert Mikhayelyan", L"About");
    auto commands = dialog.Commands();
    commands.Append(winrt::UICommand(L"Open GitHub", [](auto&) -> winrt::fire_and_forget
        {
            co_await winrt::Launcher::LaunchUriAsync(winrt::Uri(L"https://github.com/robmikh/ProcessViewer"));
        }));
    commands.Append(winrt::UICommand(L"Close"));

    dialog.DefaultCommandIndex(1);
    dialog.CancelCommandIndex(1);

    InitializeObjectWithWindowHandle(dialog);
    co_await dialog.ShowAsync();
}

std::string GetStringFromPath(winrt::hstring const& path)
{
    std::wstring input(path);
    auto const input_length = static_cast<uint32_t>(input.length() + 1);
    int buffer_length = WideCharToMultiByte(CP_UTF8, 0, input.data(), input_length, 0, 0, nullptr, nullptr);
    std::string output(buffer_length, '\0');
    winrt::check_bool(WideCharToMultiByte(CP_UTF8, 0, input.data(), input_length, output.data(), buffer_length, nullptr, nullptr) > 0);
    return output;
}