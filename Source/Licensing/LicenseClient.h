#pragma once

#include <juce_core/juce_core.h>
#include <atomic>

namespace ProducerToy::Licensing
{
    struct ProductConfig
    {
        static constexpr const char* productId = "92cf2454-2cfc-4db5-b210-2c732457605f";
        static constexpr const char* pluginIdentifier = "com.producertoy.proverb";
        static constexpr const char* productName = "ProVerb";
        static constexpr const char* buyUrl = "https://producertoy.com/product/proverb";
        static constexpr const char* supportUrl = "https://producertoy.com/support";
        static constexpr const char* apiBaseUrl = "https://licensing.producertoy.com/api/v1/license";
        static constexpr const char* publicKeyHex = "874392bb5c67a3e2d91af3bf4d4de8204b39e85684df76900f87a880c8fbd2e9";
        static constexpr const char* signingKeyId = "proverb-ed25519-2026-10-v2";
    };

    struct LicenseState
    {
        bool isActivated { false };
        juce::String registeredName { "Unregistered User" };
        juce::String maskedEmail { "trial@producertoy.com" };
        juce::String licenseTier { "Trial Mode" };
        juce::Time issuedAt;
        juce::String activationId;
        juce::String deviceHash;
        juce::String rawToken;
    };

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
        juce::String getEffectiveApiUrl() const;

        bool verifyToken(const juce::String& token, LicenseState& outState) const;
        bool installLicenseToken(const juce::String& token, juce::String& error);
        bool installLicenseFile(const juce::File& source, juce::String& error);

        // Method 1: Serial Key Activation (Async HTTP to dedicated self-hosted server)
        void activateWithSerial(const juce::String& serialKey,
                                std::function<void(bool success, const juce::String& message)> callback);

        // Method 2: Online Account Pairing (Async HTTP to dedicated self-hosted server)
        void startOnlinePairing(
            std::function<void(bool ready, const juce::String& code, const juce::String& claimUrl, const juce::String& error)> onCodeReady,
            std::function<void(bool success, const juce::String& message)> onComplete);
        void cancelPairingFlow();

        // Deactivation (Removes local file & notifies dedicated server if reachable)
        void deactivate(std::function<void(bool success, const juce::String& message)> callback = nullptr);

        static juce::File getLicenseDirectory();
        static juce::File getLicenseFile();

    private:
        std::atomic<bool> isActivatedAtomic { false };
        mutable juce::CriticalSection stateLock;
        LicenseState currentState;

        class PairingWorker;
        std::unique_ptr<PairingWorker> activePairingWorker;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LicenseClient)
    };
}
