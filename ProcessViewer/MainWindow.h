#pragma once
#include <robmikh.common/DesktopWindow.h>

struct MainWindow : robmikh::common::desktop::DesktopWindow<MainWindow>
{
	static const std::wstring ClassName;
	static void RegisterWindowClass();
	MainWindow(std::wstring const& titleString, int width, int height);
	LRESULT MessageHandler(UINT const message, WPARAM const wparam, LPARAM const lparam);

private:
	enum class Architecture
	{
		Unknown,
		x86,
		x64,
		ARM,
		ARM64
	};

	struct Process
	{
		DWORD Pid;
		std::wstring Name;
		USHORT ArchitectureValue;
	};

	static std::vector<Process> GetAllProcesses();
	static Architecture GetArchitecture(USHORT value);
	static std::wstring GetArchitectureString(Architecture arch);
	static std::optional<Process> CreateProcessFromProcessEntry(PROCESSENTRY32W const& entry);

	void CreateControls(HINSTANCE instance);
	void ResizeProcessListView();
	void OnListViewNotify(LPARAM const lparam);

private:
	HWND m_processListView = nullptr;
	std::vector<Process> m_processes;
};