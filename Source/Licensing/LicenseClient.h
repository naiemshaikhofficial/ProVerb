#pragma once

#include <juce_core/juce_core.h>
#include <atomic>
#include <functional>
#include <memory>

namespace ProducerToy::Licensing
{
    // ==============================================================================
    // Centralized Product Metadata & Marketplace Scoping
    // (Reusable by any Producer Toy plugin)
    // ==============================================================================
    struct ProductConfig
    {
        static constexpr const char* productId        = "92cf2454-2cfc-4db5-b210-2c732457605f";
        static constexpr const char* pluginIdentifier = "com.producertoy.proverb";
        static constexpr const char* productName      = "ProVerb";
        static constexpr const char* brandName        = "Producer Toy";
        static constexpr const char* buyUrl           = "https://producertoy.com/product/proverb";
        static constexpr const char* activateBaseUrl  = "https://producertoy.com/library?claim=";
        static constexpr const char* libraryUrl       = "https://producertoy.com/library";
        static constexpr const char* supportUrl       = "https://producertoy.com/support";
        static constexpr const char* apiBaseUrl       = "https://producertoy.com/api/license";

        // Embedded Marketplace Cryptographic Key
        static constexpr const char* masterSecret     = "3qwpro3wqu3u3uodnnxcv82384903898";
        static constexpr const char* publicKeyHex     = "1b478fce182f0b9b418a0ea80d53a2c26d7c1ad288e13a39b2835fc8ae6a70e9";
    };

    struct LicenseState
    {
        bool isActivated { false };
        juce::String registeredName { "Unregistered User" };
        juce::String maskedEmail { "trial@producertoy.com" };
        juce::String licenseTier { "Trial Mode" };
        juce::Time issuedAt;
        int graceDays { 14 };
        juce::String activationId;
        juce::String deviceHash;
        juce::String rawToken;
    };

    // ==============================================================================
    // Robust, Non-Blocking Audio-Safe Licensing Engine
    // ==============================================================================
    class LicenseClient
    {
    public:
        static LicenseClient& getInstance();

        LicenseClient();
        ~LicenseClient();

        void initialize();

        bool isLicenseValid() const noexcept { return isActivatedAtomic.load(std::memory_order_relaxed); }
        LicenseState getCurrentState() const;

        juce::String getHardwareDeviceHash() const;
        juce::String getHardwareDeviceName() const;

        bool verifyToken(const juce::String& token, LicenseState& outState) const;

        // Desktop Single-Use Pairing Flow (Async background thread)
        void startPairingFlow(
            std::function<void(bool success, const juce::String& code, const juce::String& error)> onCodeReady,
            std::function<void(bool success, const juce::String& error)> onComplete);

        void cancelPairingFlow();
        void deactivate();

        // Safe storage locations
        static juce::File getLicenseDirectory();
        static juce::File getLicenseFile();

    private:
        std::atomic<bool> isActivatedAtomic { false };
        mutable juce::CriticalSection stateLock;
        LicenseState currentState;

        class PairingWorker;
        std::unique_ptr<PairingWorker> activeWorker;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LicenseClient)
    };
}
