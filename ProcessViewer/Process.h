#pragma once

enum class ProcessInformation
{
    Pid,
    Name,
    Type,
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
    case ProcessInformation::Type:
        os << L"Type";
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

enum class ProcessType
{
    Legacy, // TODO: Better name, split packaged/unpackaged
    AppContainer,
};

inline std::wostream& operator<< (std::wostream& os, ProcessType const& type)
{
    switch (type)
    {
    case ProcessType::Legacy:
        os << L"Legacy";
        break;
    case ProcessType::AppContainer:
        os << L"AppContainer";
        break;
    }
    return os;
}

template<typename T>
inline std::wostream& operator<< (std::wostream& os, std::optional<T> const& optional)
{
    if (optional.has_value())
    {
        os << *optional;
    }
    else
    {
        os << L"Unknown";
    }
    return os;
}

struct Process
{
    DWORD Pid;
    std::wstring Name;
    std::optional<ProcessType> Type;
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

inline wil::unique_handle GetProcessToken(wil::unique_handle const& processHandle)
{
    wil::unique_handle processToken;
    winrt::check_bool(OpenProcessToken(processHandle.get(), TOKEN_QUERY, processToken.put()));
    return processToken;
}

inline std::optional<Process> CreateProcessFromPid(DWORD pid, std::wstring const& processName)
{
    USHORT archValue = IMAGE_FILE_MACHINE_UNKNOWN;
    std::optional<ProcessType> processType = std::nullopt;
    try
    {
        auto handle = GetProcessHandleFromPid(pid);
        USHORT process = 0;
        USHORT machine = 0;
        winrt::check_bool(IsWow64Process2(handle.get(), &process, &machine));
        archValue = process == IMAGE_FILE_MACHINE_UNKNOWN ? machine : process;

        auto token = GetProcessToken(handle);
        BOOL isAppContainer = FALSE;
        DWORD length = 0;
        winrt::check_bool(GetTokenInformation(token.get(), TokenIsAppContainer, &isAppContainer, sizeof(BOOL), &length));
        WINRT_VERIFY(length == sizeof(BOOL));
        if (isAppContainer)
        {
            processType = std::optional(ProcessType::AppContainer);
        }
        else
        {
            processType = std::optional(ProcessType::Legacy);
        }
    }
    catch (winrt::hresult_error const& error)
    {
        const auto code = error.code();
        if (code == E_INVALIDARG)
        {
            return std::nullopt;
        }
        if (code != E_ACCESSDENIED &&
            code != HRESULT_FROM_WIN32(ERROR_NOACCESS))
        {
            throw;
        }
    }
    return std::optional(Process{ pid, processName, processType, archValue });
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