#include "pch.h"
#include "ProcessWatcher.h"

namespace winrt
{
    using namespace Windows::System;
}

ProcessWatcher::ProcessWatcher(winrt::DispatcherQueue const& dispatcherQueue, ProcessAddedCallback processAdded, ProcessRemovedCallback processRemoved)
{
    m_dispatcherQueue = dispatcherQueue;
    m_processAdded = processAdded;
    m_processRemoved = processRemoved;

    auto locator = winrt::create_instance<IWbemLocator>(CLSID_WbemLocator);
    winrt::check_hresult(locator->ConnectServer(BSTR(L"ROOT\\CIMV2"), nullptr, nullptr, 0, 0, 0, 0, m_services.put()));

    winrt::check_hresult(CoSetProxyBlanket(
        m_services.get(),
        RPC_C_AUTHN_WINNT,
        RPC_C_AUTHZ_NONE,
        nullptr,
        RPC_C_AUTHN_LEVEL_CALL,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        nullptr,
        EOAC_NONE));

    m_unsecuredApartment = winrt::create_instance<IUnsecuredApartment>(CLSID_UnsecuredApartment, CLSCTX_LOCAL_SERVER);
    m_sink = winrt::make_self<EventSink>(EventSink::EventSinkCallback([&](winrt::array_view<IWbemClassObject*> const& objs)
        {
            for (auto& objRaw : objs)
            {
                winrt::com_ptr<IWbemClassObject> obj;
                obj.copy_from(objRaw);
                auto targetInstance = GetProperty<winrt::com_ptr<IUnknown>>(obj, L"TargetInstance");
                auto win32Process = targetInstance.as<IWbemClassObject>();
                auto name = GetProperty<wil::unique_bstr>(win32Process, L"Name");
                auto processId = GetProperty<uint32_t>(win32Process, L"ProcessId");
                
                auto classNameBstr = GetProperty<wil::unique_bstr>(obj, L"__CLASS");
                auto className = std::wstring(classNameBstr.get(), SysStringLen(classNameBstr.get()));
                if (className == L"__InstanceCreationEvent")
                {
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
                else if (className == L"__InstanceDeletionEvent")
                {
                    auto processRemoved = m_processRemoved;
                    m_dispatcherQueue.TryEnqueue([processId, processRemoved]()
                        {
                            processRemoved(processId);
                        });
                }
            }
        }));
    winrt::com_ptr<IUnknown> stubUnknown;
    winrt::check_hresult(m_unsecuredApartment->CreateObjectStub(m_sink.get(), stubUnknown.put()));
    m_sinkStub = stubUnknown.as<IWbemObjectSink>();
    winrt::check_hresult(m_services->ExecNotificationQueryAsync(
        BSTR(L"WQL"),
        BSTR(L"SELECT * FROM __InstanceOperationEvent WITHIN 1 WHERE TargetInstance ISA 'Win32_Process'"),
        WBEM_FLAG_SEND_STATUS,
        nullptr,
        m_sinkStub.get()));
}

ProcessWatcher::~ProcessWatcher()
{
    winrt::check_hresult(m_services->CancelAsyncCall(m_sinkStub.get()));
}
