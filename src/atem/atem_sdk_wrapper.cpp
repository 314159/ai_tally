#include "atem_sdk_wrapper.h"
#include <iostream>

namespace atem {

// Concrete implementation of ATEMDevice
class ATEMDeviceImpl : public ATEMDevice {
public:
    ATEMDeviceImpl(IBMDSwitcher* switcher)
        : m_switcher(switcher)
    {
        m_switcher->AddRef();
    }

    ~ATEMDeviceImpl() override
    {
        if (m_switcher) {
            m_switcher->RemoveCallback(m_switcherCallback);
            m_switcher->Release();
        }
        if (m_switcherCallback)
            m_switcherCallback->Release();
    }

    bool connect() override
    {
        // Connection is already established by the discovery object
        return m_switcher != nullptr;
    }

    void disconnect() override
    {
        // The switcher object is released in the destructor
    }

    void poll() override
    {
        // The SDK uses callbacks, so polling is not required in the same way.
        // This could be used for periodic health checks if needed.
    }

    std::string get_product_name() const override
    {
        if (!m_switcher)
            return "Not Connected";

#ifdef _WIN32
        BSTR productNameBSTR = nullptr;
        if (m_switcher->GetProductName(&productNameBSTR) == S_OK) {
            std::string productName = _bstr_t(productNameBSTR, false);
            SysFreeString(productNameBSTR);
            return productName;
        }
#else
        CFStringRef productNameCFString = nullptr;
        if (m_switcher->GetProductName(&productNameCFString) == S_OK) {
            const size_t len = CFStringGetLength(productNameCFString);
            std::vector<char> buf(len + 1);
            CFStringGetCString(productNameCFString, buf.data(), len + 1, kCFStringEncodingUTF8);
            std::string productName(buf.data());
            CFRelease(productNameCFString);
            return productName;
        }
#endif
        return "Unknown";
    }

    void set_callback(ATEMSwitcherCallback* callback) override
    {
        if (!m_switcher)
            return;

        m_switcherCallback = new SwitcherCallback(callback);
        m_switcher->AddCallback(m_switcherCallback);

        // Also add callbacks for Mix Effect Blocks to get tally updates
        IBMDSwitcherMixEffectBlockIterator* meIterator = nullptr;
        if (m_switcher->CreateIterator(IID_IBMDSwitcherMixEffectBlockIterator, (void**)&meIterator) == S_OK) {
            IBMDSwitcherMixEffectBlock* meBlock = nullptr;
            while (meIterator->Next(&meBlock) == S_OK) {
                auto* meCallback = new MixEffectBlockCallback(callback, meBlock);
                meBlock->AddCallback(meCallback);
                meCallback->Release(); // ME Block holds a reference
                meBlock->Release();
            }
            meIterator->Release();
        }
    }

private:
    IBMDSwitcher* m_switcher;
    SwitcherCallback* m_switcherCallback = nullptr;
};

// Concrete implementation of ATEMDiscovery
class ATEMDiscoveryImpl : public ATEMDiscovery {
public:
    ATEMDiscoveryImpl()
    {
#ifdef _WIN32
        CoInitialize(NULL);
        HRESULT res = CoCreateInstance(CLSID_CBMDSwitcherDiscovery, NULL, CLSCTX_ALL, IID_IBMDSwitcherDiscovery, (void**)&m_discovery);
        if (FAILED(res)) {
            m_discovery = nullptr;
            std::cerr << "Error: Could not create Switcher Discovery Instance.\n";
        }
#else
        m_discovery = CreateBMDSwitcherDiscoveryInstance();
        if (!m_discovery) {
            std::cerr << "Error: Could not create Switcher Discovery Instance.\n";
        }
#endif
    }

    ~ATEMDiscoveryImpl() override
    {
        if (m_discovery)
            m_discovery->Release();
#ifdef _WIN32
        CoUninitialize();
#endif
    }

    std::unique_ptr<ATEMDevice> connect_to(const std::string& ip_address) override
    {
        if (!m_discovery)
            return nullptr;

        IBMDSwitcher* switcher = nullptr;
        BMDSwitcherConnectToFailure failureReason;

#ifdef _WIN32
        _bstr_t bstr_ip(ip_address.c_str());
        HRESULT res = m_discovery->ConnectTo(bstr_ip, &switcher, &failureReason);
#else
        CFStringRef ip_cfstring = CFStringCreateWithCString(NULL, ip_address.c_str(), kCFStringEncodingUTF8);
        HRESULT res = m_discovery->ConnectTo(ip_cfstring, &switcher, &failureReason);
        CFRelease(ip_cfstring);
#endif

        if (res == S_OK) {
            return std::make_unique<ATEMDeviceImpl>(switcher);
        } else {
            std::cerr << "Failed to connect to ATEM. Reason: " << failureReason << std::endl;
            return nullptr;
        }
    }

private:
    IBMDSwitcherDiscovery* m_discovery = nullptr;
};

std::unique_ptr<ATEMDiscovery> ATEMDiscovery::create()
{
    return std::make_unique<ATEMDiscoveryImpl>();
}

} // namespace atem

// --- Callback Implementations ---

#ifdef __APPLE__
namespace {
/**
 * @class CFUUIDRefWrapper
 * @brief An RAII wrapper for a CFUUIDRef created from bytes.
 *
 * This utility class ensures that a CFUUIDRef created via
 * CFUUIDCreateFromUUIDBytes is always released, preventing memory leaks.
 */
class CFUUIDRefWrapper {
public:
    explicit CFUUIDRefWrapper(const CFUUIDBytes& bytes)
        : m_ref(CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, bytes))
    {
    }

    ~CFUUIDRefWrapper()
    {
        if (m_ref)
            CFRelease(m_ref);
    }

    operator CFUUIDRef() const
    {
        return m_ref;
    }

private:
    CFUUIDRef m_ref;
};
} // anonymous namespace
#endif

SwitcherCallback::SwitcherCallback(atem::ATEMSwitcherCallback* owner)
    : m_owner(owner)
    , m_refCount(1) // atomic
{
}
SwitcherCallback::~SwitcherCallback()
{
}

HRESULT STDMETHODCALLTYPE SwitcherCallback::QueryInterface(REFIID iid, LPVOID* ppv)
{
    if (!ppv)
        return E_POINTER;
#ifdef _WIN32
    if (IsEqualIID(iid, IID_IUnknown) || IsEqualIID(iid, IID_IBMDSwitcherCallback))
#else
    // On macOS, IID_IUnknown and IID_IBMDSwitcherCallback are CFUUIDRef types
    // The SDK on macOS doesn't query for IUnknown, only for the specific interface.
    if (CFEqual(CFUUIDRefWrapper(iid), CFUUIDRefWrapper(IID_IBMDSwitcherCallback)))
#endif
    {
        *ppv = this;
        AddRef();
        return S_OK;
    }
    *ppv = NULL;
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE SwitcherCallback::AddRef()
{
    return m_refCount.fetch_add(1) + 1;
}
ULONG STDMETHODCALLTYPE SwitcherCallback::Release()
{
    ULONG new_ref = m_refCount.fetch_sub(1) - 1;
    if (new_ref == 0)
        delete this;
    return new_ref;
}

HRESULT STDMETHODCALLTYPE SwitcherCallback::Notify(BMDSwitcherEventType eventType, BMDSwitcherVideoMode)
{
    if (eventType == bmdSwitcherEventTypeDisconnected) {
        if (m_owner)
            m_owner->on_disconnected();
    }
    return S_OK;
}

MixEffectBlockCallback::MixEffectBlockCallback(atem::ATEMSwitcherCallback* owner, IBMDSwitcherMixEffectBlock* meBlock)
    : m_owner(owner)
    , m_meBlock(meBlock)
    , m_refCount(1) // atomic
{
    m_meBlock->AddRef();
}
MixEffectBlockCallback::~MixEffectBlockCallback()
{
    m_meBlock->Release();
}

HRESULT STDMETHODCALLTYPE MixEffectBlockCallback::QueryInterface(REFIID iid, LPVOID* ppv)
{
    if (!ppv)
        return E_POINTER;
#ifdef _WIN32
    if (IsEqualIID(iid, IID_IUnknown) || IsEqualIID(iid, IID_IBMDSwitcherMixEffectBlockCallback))
#else
    // On macOS, IIDs are CFUUIDRef types
    if (CFEqual(CFUUIDRefWrapper(iid), CFUUIDRefWrapper(IID_IBMDSwitcherMixEffectBlockCallback)))
#endif
    {
        *ppv = this;
        AddRef();
        return S_OK;
    }
    *ppv = NULL;
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE MixEffectBlockCallback::AddRef()
{
    return m_refCount.fetch_add(1) + 1;
}
ULONG STDMETHODCALLTYPE MixEffectBlockCallback::Release()
{
    ULONG new_ref = m_refCount.fetch_sub(1) - 1;
    if (new_ref == 0)
        delete this;
    return new_ref;
}

HRESULT STDMETHODCALLTYPE MixEffectBlockCallback::Notify(BMDSwitcherMixEffectBlockEventType eventType)
{
    if (eventType == bmdSwitcherMixEffectBlockEventTypeProgramInputChanged || eventType == bmdSwitcherMixEffectBlockEventTypePreviewInputChanged) {
        if (!m_owner || !m_meBlock)
            return S_OK;

        BMDSwitcherInputId programId = 0;
        BMDSwitcherInputId previewId = 0;

        // Get the latest program and preview input IDs from our Mix Effect Block
        m_meBlock->GetProgramInput(&programId);
        m_meBlock->GetPreviewInput(&previewId);

        // The SDK doesn't directly tell us which input *was* on air,
        // so a full tally implementation requires iterating all inputs.
        // However, for a simple update, we can send updates for the new sources.
        // Note: A more robust solution would track the previous state.
        if (eventType == bmdSwitcherMixEffectBlockEventTypeProgramInputChanged && programId != 0) {
            m_owner->on_tally_state_changed({ (uint16_t)programId, true, false });
        }

        if (eventType == bmdSwitcherMixEffectBlockEventTypePreviewInputChanged && previewId != 0) {
            m_owner->on_tally_state_changed({ (uint16_t)previewId, false, true });
        }
    }
    return S_OK;
}
