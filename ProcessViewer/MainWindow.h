#pragma once
#include <robmikh.common/DesktopWindow.h>
#include "Process.h"
#include "ProcessWatcher.h"

struct MainWindow : robmikh::common::desktop::DesktopWindow<MainWindow>
{
    static const std::wstring ClassName;
    static void RegisterWindowClass();
    MainWindow(std::wstring const& titleString, int width, int height);
    LRESULT MessageHandler(UINT const message, WPARAM const wparam, LPARAM const lparam);

private:
    using unique_himagelist = wil::unique_any<HIMAGELIST, decltype(&::ImageList_Destroy), ::ImageList_Destroy>;

    enum class ColumnSorting
    {
        Ascending,
        Descending
    };

    std::vector<Process>::iterator GetProcessInsertIterator(Process const& process);
    void InsertProcess(Process const& process);
    void RemoveProcessByProcessId(DWORD processId);
    void CreateMenuBar();
    void CreateControls(HINSTANCE instance);
    void ResizeProcessListView();
    void OnListViewNotify(LPARAM const lparam);
    void ResetProcessIconsCache();
    void EnsureProcessIcon(std::wstring const& exePath);

    winrt::fire_and_forget CheckBinaryArchitecture();
    
    static bool CompareProcessId(
        Process const& process1,
        Process const& process2,
        ColumnSorting const& sort);
    static bool CompareProcesses(
        Process const& process1, 
        Process const& process2,
        ColumnSorting const& sort,
        ProcessInformation const& column);

    winrt::fire_and_forget ShowAboutAsync();

private:
    HWND m_processListView = nullptr;
    unique_himagelist m_imageList;
    std::vector<ProcessInformation> m_columns;
    size_t m_selectedColumnIndex = 1;
    ColumnSorting m_columnSort = ColumnSorting::Ascending;
    std::vector<Process> m_processes;
    std::map<std::wstring, size_t> m_pathToIconIndex;
    std::vector<wil::shared_hicon> m_icons;
    wil::unique_hmenu m_menuBar;
    wil::unique_hmenu m_fileMenu;
    wil::unique_hmenu m_viewMenu;
    wil::unique_hmenu m_toolsMenu;
    wil::unique_hmenu m_helpMenu;
    bool m_viewAccessibleProcess = true;
    winrt::Windows::System::DispatcherQueue m_dispatcherQueue{ nullptr };
    std::unique_ptr<ProcessWatcher> m_processWatcher;
};