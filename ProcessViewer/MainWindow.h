#pragma once
#include <robmikh.common/DesktopWindow.h>
#include "Process.h"

struct MainWindow : robmikh::common::desktop::DesktopWindow<MainWindow>
{
	static const std::wstring ClassName;
	static void RegisterWindowClass();
	MainWindow(std::wstring const& titleString, int width, int height);
	LRESULT MessageHandler(UINT const message, WPARAM const wparam, LPARAM const lparam);

private:

	enum class ColumnSorting
	{
		Ascending,
		Descending
	};

	std::vector<Process>::iterator GetProcessInsertIterator(Process const& process);
	void InsertProcess(Process const& process);
	void CreateMenuBar();
	void CreateControls(HINSTANCE instance);
	void ResizeProcessListView();
	void OnListViewNotify(LPARAM const lparam);

private:
	HWND m_processListView = nullptr;
	std::vector<ProcessInformation> m_columns;
	size_t m_selectedColumnIndex = 1;
	ColumnSorting m_columnSort = ColumnSorting::Ascending;
	std::vector<Process> m_processes;
	wil::unique_hmenu m_menuBar;
	winrt::Windows::System::DispatcherQueue m_dispatcherQueue{ nullptr };
};