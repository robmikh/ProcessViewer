#pragma once
#include "Process.h"

class ProcessWatcher
{
public:
    using ProcessAddedCallback = std::function<void(Process)>;

    ProcessWatcher(winrt::Windows::System::DispatcherQueue const& dispatcherQueue, ProcessAddedCallback processAdded);
    ~ProcessWatcher();

private:
    winrt::com_ptr<IWbemServices> m_services;
    winrt::com_ptr<IUnsecuredApartment> m_unsecuredApartment;
    winrt::com_ptr<EventSink> m_sink;
    winrt::com_ptr<IWbemObjectSink> m_sinkStub;

    ProcessAddedCallback m_processAdded;
    winrt::Windows::System::DispatcherQueue m_dispatcherQueue{ nullptr };
};