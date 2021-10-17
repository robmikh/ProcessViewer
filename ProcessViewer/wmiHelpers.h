#pragma once

template<typename T>
T GetProperty(winrt::com_ptr<IWbemClassObject> const& obj, std::wstring_view const& propertyName);

template<>
inline wil::unique_variant GetProperty(winrt::com_ptr<IWbemClassObject> const& obj, std::wstring_view const& propertyName)
{
    wil::unique_variant variant;
    winrt::check_hresult(obj->Get(propertyName.data(), 0, variant.addressof(), 0, 0));
    return variant;
}

template<>
inline winrt::com_ptr<IUnknown> GetProperty(winrt::com_ptr<IWbemClassObject> const& obj, std::wstring_view const& propertyName)
{
    auto variant = GetProperty<wil::unique_variant>(obj, propertyName);
    winrt::com_ptr<IUnknown> unknown;
    unknown.copy_from(variant.punkVal);
    return unknown;
}

template<>
inline wil::unique_bstr GetProperty(winrt::com_ptr<IWbemClassObject> const& obj, std::wstring_view const& propertyName)
{
    auto variant = GetProperty<wil::unique_variant>(obj, propertyName);
    wil::unique_bstr bstr(variant.bstrVal);
    return bstr;
}

template<>
inline uint32_t GetProperty(winrt::com_ptr<IWbemClassObject> const& obj, std::wstring_view const& propertyName)
{
    auto variant = GetProperty<wil::unique_variant>(obj, propertyName);
    return variant.uintVal;
}

struct EventSink : winrt::implements<EventSink, IWbemObjectSink>
{
    using EventSinkCallback = std::function<void(winrt::array_view<IWbemClassObject*> const&)>;

    EventSink(EventSinkCallback callback)
    {
        m_callback = callback;
    }

    IFACEMETHODIMP Indicate(long objectCount, IWbemClassObject** objArray)
    {
        try
        {
            winrt::array_view<IWbemClassObject*> objs(objArray, objArray + objectCount);
            m_callback(objs);

        }
        catch (...)
        {
            return winrt::to_hresult();
        }
        return S_OK;
    }

    IFACEMETHODIMP SetStatus(long flags, HRESULT result, BSTR strParam, IWbemClassObject* objParam)
    {
        UNREFERENCED_PARAMETER(result);
        UNREFERENCED_PARAMETER(strParam);
        UNREFERENCED_PARAMETER(objParam);
        // TODO: Do I need to handle this?
        if (flags == WBEM_STATUS_COMPLETE)
        {
        }
        else if (flags == WBEM_STATUS_PROGRESS)
        {
        }
        return S_OK;
    }

private:
    EventSinkCallback m_callback{ nullptr };
};