#pragma once

enum class ProcessInformation
{
    Pid,
    Name,
    Type,
    Architecture,
    IntegrityLevel,
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
    case ProcessInformation::IntegrityLevel:
        os << L"Integrity Level";
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

enum class IntegrityLevel : uint32_t
{
    Untrusted = SECURITY_MANDATORY_UNTRUSTED_RID,
    Low = SECURITY_MANDATORY_LOW_RID,
    Medium = SECURITY_MANDATORY_MEDIUM_RID,
    MediumPlus = SECURITY_MANDATORY_MEDIUM_PLUS_RID,
    High = SECURITY_MANDATORY_HIGH_RID,
    System = SECURITY_MANDATORY_SYSTEM_RID,
    ProtectedProcess = SECURITY_MANDATORY_PROTECTED_PROCESS_RID
};

inline std::wostream& operator<< (std::wostream& os, IntegrityLevel const& ilevel)
{
    switch (ilevel)
    {
    case IntegrityLevel::Untrusted:
        return os << L"Untrusted";
    case IntegrityLevel::Low:
        return os << L"Low";
    case IntegrityLevel::Medium:
        return os << L"Medium";
    case IntegrityLevel::MediumPlus:
        return os << L"MediumPlus";
    case IntegrityLevel::High:
        return os << L"High";
    case IntegrityLevel::System:
        return os << L"System";
    case IntegrityLevel::ProtectedProcess:
        return os << L"ProtectedProcess";
    default:
        std::abort();
    }
}

// https://support.microsoft.com/en-us/help/243330/well-known-security-identifiers-in-windows-operating-systems
inline std::wstring IntegrityLevelToSidString(IntegrityLevel value)
{
    switch (value)
    {
    case IntegrityLevel::Untrusted:
        return L"S-1-16-0";
    case IntegrityLevel::Low:
        return L"S-1-16-4096";
    case IntegrityLevel::Medium:
        return L"S-1-16-8192";
    case IntegrityLevel::MediumPlus:
        return L"S-1-16-8448";
    case IntegrityLevel::High:
        return L"S-1-16-12288";
    case IntegrityLevel::System:
        return L"S-1-16-16384";
    case IntegrityLevel::ProtectedProcess:
        return L"S-1-16-20480";
    }
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
    std::wstring ExecutablePath;
    std::optional<ProcessType> Type;
    USHORT ArchitectureValue;
    std::optional<IntegrityLevel> IntegrityLevel;

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

template <typename T>
inline void check_bool_with_expected_error(T result, winrt::hresult expected)
{
    if (!result)
    {
        // Get the last error
        auto lastError = HRESULT_FROM_WIN32(GetLastError());
        if (lastError != expected)
        {
            winrt::check_hresult(lastError);
        }
    }
}

inline std::optional<IntegrityLevel> GetIntegrityLevelFromProcessToken(wil::unique_handle const& processToken)
{
    // Get the size of the data we'll get back
    DWORD informationLength = 0;
    check_bool_with_expected_error(GetTokenInformation(processToken.get(), TokenIntegrityLevel, nullptr, 0, &informationLength), HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER));

    // Allocate the memory for the integrity level data
    wil::unique_hlocal infoData(LocalAlloc(0, informationLength));
    auto info = (PTOKEN_MANDATORY_LABEL)infoData.get();

    // Get the data for our integrity level
    winrt::check_bool(GetTokenInformation(processToken.get(), TokenIntegrityLevel, info, informationLength, &informationLength));

    // Get the integrity level from the info we got back
    auto authorityCount = (DWORD)(UCHAR)*winrt::check_pointer(GetSidSubAuthorityCount(info->Label.Sid));
    auto integrityLevel = *winrt::check_pointer(GetSidSubAuthority(info->Label.Sid, authorityCount - 1));

    switch (integrityLevel)
    {
    case SECURITY_MANDATORY_UNTRUSTED_RID:
    case SECURITY_MANDATORY_LOW_RID:
    case SECURITY_MANDATORY_MEDIUM_RID:
    case SECURITY_MANDATORY_HIGH_RID:
    case SECURITY_MANDATORY_SYSTEM_RID:
    case SECURITY_MANDATORY_PROTECTED_PROCESS_RID:
        return std::optional(static_cast<IntegrityLevel>(integrityLevel));
    default:
        return std::nullopt;
    }
}

inline std::wstring GetExectuablePathFromProcess(wil::unique_handle const& processHandle)
{
    std::wstring exePath(MAX_PATH, L'\0');
    DWORD length = static_cast<DWORD>(exePath.size());
    winrt::check_bool(QueryFullProcessImageNameW(processHandle.get(), 0, exePath.data(), &length));
    exePath.resize(length, L'\0');
    return exePath;
}

inline std::optional<Process> CreateProcessFromPid(DWORD pid, std::wstring const& processName)
{
    USHORT archValue = IMAGE_FILE_MACHINE_UNKNOWN;
    std::optional<ProcessType> processType = std::nullopt;
    std::optional<IntegrityLevel> ilevel = std::nullopt;
    std::wstring exeName;
    try
    {
        auto handle = GetProcessHandleFromPid(pid);
        USHORT process = 0;
        USHORT machine = 0;
        winrt::check_bool(IsWow64Process2(handle.get(), &process, &machine));
        archValue = process == IMAGE_FILE_MACHINE_UNKNOWN ? machine : process;

        exeName = GetExectuablePathFromProcess(handle);

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

        ilevel = GetIntegrityLevelFromProcessToken(token);
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
    return std::optional(Process{ pid, processName, exeName, processType, archValue, ilevel });
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