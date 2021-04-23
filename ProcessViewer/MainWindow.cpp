#include "pch.h"
#include "MainWindow.h"
#include <CommCtrl.h>

namespace winrt
{
    using namespace Windows::System;
}

#define ID_LISTVIEW  2000 // ?????

const std::wstring MainWindow::ClassName = L"ProcessViewer.MainWindow";

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
        ProcessInformation::Status,
        ProcessInformation::Architecture
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
    case ProcessInformation::Status:
        return false;
    case ProcessInformation::Architecture:
        if (sort == ColumnSorting::Ascending)
        {
            return left.ArchitectureValue < right.ArchitectureValue;
        }
        else
        {
            return left.ArchitectureValue > right.ArchitectureValue;
        }
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
    auto newIndex = GetProcessInsertIterator(process);
    LVITEM item = {};
    item.iItem = newIndex - m_processes.begin();
    m_processes.insert(newIndex, process);
    ListView_InsertItem(m_processListView, &item);
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

DWORD GetDebugRandomNumber(size_t size)
{
    std::random_device randomDevice;
    std::mt19937 generator(randomDevice());
    std::uniform_int_distribution<> distrib(1, size);
    return distrib(generator);
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
        auto menu = (HMENU)lparam;
        int index = wparam;
        if (menu == m_menuBar.get())
        {
            auto process = Process
            {
                GetDebugRandomNumber(12000), L"DEBUG FAKE PROCESS", IMAGE_FILE_MACHINE_ARM64
            };

            InsertProcess(process);
        }
    }
        break;
    }
    return base_type::MessageHandler(message, wparam, lparam);
}

void MainWindow::CreateMenuBar()
{
    m_menuBar.reset(winrt::check_pointer(CreateMenu()));
    winrt::check_bool(AppendMenuW(m_menuBar.get(), MF_POPUP | MF_STRING, 0, L"Debug"));
    winrt::check_bool(SetMenu(m_window, m_menuBar.get()));
    MENUINFO menuInfo = {};
    menuInfo.cbSize = sizeof(menuInfo);
    menuInfo.fMask = MIM_STYLE;
    menuInfo.dwStyle = MNS_NOTIFYBYPOS;
    winrt::check_bool(SetMenuInfo(m_menuBar.get(), &menuInfo));
}

void MainWindow::CreateControls(HINSTANCE instance)
{
    auto style = WS_TABSTOP | WS_CHILD | WS_BORDER | WS_VISIBLE | LVS_AUTOARRANGE | LVS_REPORT | LVS_OWNERDATA | LVS_SHOWSELALWAYS | LVS_SINGLESEL;

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
        std::vector<std::wstring> columnNames;
        for (auto&& column : m_columns)
        {
            std::wstringstream stream;
            stream << column;
            columnNames.push_back(stream.str());
        }

        column.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
        column.fmt = LVCFMT_LEFT;
        column.cx = 120;
        for (auto i = 0; i < columnNames.size(); i++)
        {
            column.pszText = columnNames[i].data();
            ListView_InsertColumn(m_processListView, i, &column);
        }
        ListView_SetExtendedListViewStyle(m_processListView, LVS_EX_AUTOSIZECOLUMNS | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    }

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
                case ProcessInformation::Status:
                    stream << L"Unknown status";
                    break;
                case ProcessInformation::Architecture:
                    stream << process.GetArchitecture();
                    break;
                }
                auto string = stream.str();
                wcsncpy_s(lpdi->item.pszText, lpdi->item.cchTextMax, string.data(), _TRUNCATE);
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
