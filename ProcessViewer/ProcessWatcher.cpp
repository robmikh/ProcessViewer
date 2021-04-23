#include "pch.h"
#include "ProcessWatcher.h"

namespace winrt
{
    using namespace Windows::System;
}

ProcessWatcher::ProcessWatcher(winrt::DispatcherQueue const& dispatcherQueue, ProcessAddedCallback processAdded)
{
    m_dispatcherQueue = dispatcherQueue;
    m_processAdded = processAdded;

    auto locator = winrt::create_instance<IWbemLocator>(CLSID_WbemLocator);
    winrt::com_ptr<IWbemServices> services;
    winrt::check_hresult(locator->ConnectServer(BSTR(L"ROOT\\CIMV2"), nullptr, nullptr, 0, 0, 0, 0, services.put()));

    winrt::check_hresult(CoSetProxyBlanket(
        services.get(),
        RPC_C_AUTHN_WINNT,
        RPC_C_AUTHZ_NONE,
        nullptr,
        RPC_C_AUTHN_LEVEL_CALL,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        nullptr,
        EOAC_NONE));

    auto unsecuredApartment = winrt::create_instance<IUnsecuredApartment>(CLSID_UnsecuredApartment, CLSCTX_LOCAL_SERVER);
    auto sink = winrt::make_self<EventSink>(EventSink::EventSinkCallback([&](winrt::array_view<IWbemClassObject*> const& objs)
        {
            for (auto& objRaw : objs)
            {
                winrt::com_ptr<IWbemClassObject> obj;
                obj.copy_from(objRaw);
                auto targetInstance = GetProperty<winrt::com_ptr<IUnknown>>(obj, L"TargetInstance");
                auto win32Process = targetInstance.as<IWbemClassObject>();
                auto name = GetProperty<wil::unique_bstr>(win32Process, L"Name");
                auto processId = GetProperty<uint32_t>(win32Process, L"ProcessId");
                
                if (auto processOpt = CreateProcessFromPid(processId, std::wstring(name.get(), SysStringLen(name.get()))))
                {
                    auto process = *processOpt;
                    auto processAdded = m_processAdded;
                    m_dispatcherQueue.TryEnqueue([process, processAdded]()
                        {
                            processAdded(process);
                        });
                }
            }
        }));
    winrt::com_ptr<IUnknown> stubUnknown;
    winrt::check_hresult(unsecuredApartment->CreateObjectStub(sink.get(), stubUnknown.put()));
    auto stub = stubUnknown.as<IWbemObjectSink>();
    winrt::check_hresult(services->ExecNotificationQueryAsync(
        BSTR(L"WQL"),
        BSTR(L"SELECT * FROM __InstanceCreationEvent WITHIN 1 WHERE TargetInstance ISA 'Win32_Process'"),
        WBEM_FLAG_SEND_STATUS,
        nullptr,
        stub.get()));
}

ProcessWatcher::~ProcessWatcher()
{
    winrt::check_hresult(m_services->CancelAsyncCall(m_sinkStub.get()));
}
