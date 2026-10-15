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
        static constexpr const char* publicKeyHex = "b6ca702800efcd1da84da72603cfcfb61df5f3d55532ae54459e569b8647aad6";
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
        ~LicenseClient() = default;

        void initialize();
        bool isLicenseValid() const noexcept { return isActivatedAtomic.load(std::memory_order_relaxed); }
        LicenseState getCurrentState() const;

        juce::String getHardwareDeviceHash() const;
        juce::String getHardwareDeviceName() const;
        bool verifyToken(const juce::String& token, LicenseState& outState) const;
        bool installLicenseFile(const juce::File& source, juce::String& error);

        void deactivate();

        static juce::File getLicenseDirectory();
        static juce::File getLicenseFile();

    private:
        std::atomic<bool> isActivatedAtomic { false };
        mutable juce::CriticalSection stateLock;
        LicenseState currentState;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LicenseClient)
    };
}
