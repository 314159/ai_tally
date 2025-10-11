#pragma once

#include "tally_state.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

#if defined(_WIN32)
#include "BMDSwitcherAPI.idl"
#include <comdef.h>
#elif defined(__APPLE__)
#include "BMDSwitcherAPI.h"
#endif

// Forward-declare SDK types to avoid including SDK headers in public interface
class IBMDSwitcher;
class IBMDSwitcherCallback;
class IBMDSwitcherMixEffectBlockCallback;
class IBMDSwitcherDiscovery;
class IBMDSwitcherMixEffectBlock;

namespace atem {

// --- Interfaces ---

/**
 * @class ATEMSwitcherCallback
 * @brief Abstract base class for receiving callbacks from the ATEM switcher.
 */
class ATEMSwitcherCallback {
public:
    virtual ~ATEMSwitcherCallback() noexcept = default;
    virtual void on_tally_state_changed(const TallyUpdate& update) = 0;
    virtual void on_disconnected() = 0;
};

/**
 * @class ATEMDevice
 * @brief An abstract representation of a discovered ATEM device.
 */
class ATEMDevice {
public:
    virtual ~ATEMDevice() noexcept = default;
    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    virtual void poll() = 0;
    virtual std::string get_product_name() const = 0;
    virtual uint16_t get_input_count() const = 0;
    virtual void set_callback(ATEMSwitcherCallback* callback) = 0;
};

/**
 * @class ATEMDiscovery
 * @brief An abstract class for discovering ATEM devices on the network.
 */
class ATEMDiscovery {
public:
    virtual ~ATEMDiscovery() noexcept = default;
    static std::unique_ptr<ATEMDiscovery> create();
    virtual std::unique_ptr<ATEMDevice> connect_to(const std::string& ip_address) = 0;
};

} // namespace atem

/**
 * @class SwitcherCallback
 * @brief The concrete implementation of the SDK's callback interfaces.
 * This class will handle the low-level events and translate them for our application.
 */
class SwitcherCallback
#ifdef _WIN32
    : public IBMDSwitcherCallback
#else
    : public IBMDSwitcherCallback
#endif
{
public:
    SwitcherCallback(atem::ATEMSwitcherCallback* owner);
    ~SwitcherCallback() override;

    // IUnknown methods
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, LPVOID* ppv) override;
    ULONG STDMETHODCALLTYPE AddRef() override; // Cannot be noexcept due to Windows headers
    ULONG STDMETHODCALLTYPE Release() override; // Cannot be noexcept due to Windows headers

    // IBMDSwitcherCallback methods
    HRESULT STDMETHODCALLTYPE Notify(BMDSwitcherEventType eventType, BMDSwitcherVideoMode coreVideoMode) override;

private:
    atem::ATEMSwitcherCallback* m_owner;
    std::atomic<int32_t> m_refCount;
};

class MixEffectBlockCallback : public IBMDSwitcherMixEffectBlockCallback {
public:
    MixEffectBlockCallback(atem::ATEMSwitcherCallback* owner, IBMDSwitcherMixEffectBlock* meBlock);
    ~MixEffectBlockCallback() override;

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, LPVOID* ppv) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;

    HRESULT STDMETHODCALLTYPE Notify(BMDSwitcherMixEffectBlockEventType eventType) override;

private:
    atem::ATEMSwitcherCallback* m_owner;
    IBMDSwitcherMixEffectBlock* m_meBlock;
    std::atomic<int32_t> m_refCount;
};
