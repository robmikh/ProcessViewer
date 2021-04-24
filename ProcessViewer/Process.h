#pragma once

enum class ProcessInformation
{
    Pid,
    Name,
    Status,
    Architecture,
};

inline std::wostream& operator<< (std::wostream& os, ProcessInformation const& info)
{
    switch (info)
    {
    case ProcessInformation::Pid:
        os << L"Pid";
        break;
    case ProcessInformation::Name:
        os << L"Name";
        break;
    case ProcessInformation::Status:
        os << L"Status";
        break;
    case ProcessInformation::Architecture:
        os << L"Architecture";
        break;
    }
    return os;
}

enum class Architecture
{
    Unknown,
    x86,
    x64,
    ARM,
    ARM64
};

inline Architecture MachineValueToArchitecture(uint16_t value)
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

inline std::wostream& operator<< (std::wostream& os, Architecture const& arch)
{
    switch (arch)
    {
    case Architecture::x86:
        os << L"x86";
        break;
    case Architecture::x64:
        os << L"x64";
        break;
    case Architecture::ARM:
        os << L"ARM";
        break;
    case Architecture::ARM64:
        os << L"ARM64";
        break;
    default:
        os << L"Unknown";
        break;
    }
    return os;
}

struct Process
{
    DWORD Pid;
    std::wstring Name;
    USHORT ArchitectureValue;

    Architecture GetArchitecture()
    {
        return MachineValueToArchitecture(ArchitectureValue);
    }
};

inline wil::unique_handle GetProcessHandleFromPid(DWORD pid)
{
    return wil::unique_handle(winrt::check_pointer(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, false, pid)));
}

inline std::optional<Process> CreateProcessFromPid(DWORD pid, std::wstring const& processName)
{
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

inline std::optional<Process> CreateProcessFromProcessEntry(PROCESSENTRY32W const& entry)
{
    std::wstring processName(entry.szExeFile);
    auto pid = entry.th32ProcessID;
    return CreateProcessFromPid(pid, processName);
}

inline std::vector<Process> GetAllProcesses(bool keepInaccessible = true)
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
                if (keepInaccessible || process->ArchitectureValue != IMAGE_FILE_MACHINE_UNKNOWN)
                {
                    result.push_back(*process);
                }
            }
        } while (Process32NextW(snapshot.get(), &entry));
    }
    return result;
}