#include "LicenseClient.h"
#include "Ed25519Verifier.h"
#include <limits>

namespace ProducerToy::Licensing
{
    namespace
    {
        bool isBase64Url(const juce::String& value)
        {
            if (value.isEmpty())
                return false;

            for (int i = 0; i < value.length(); ++i)
            {
                const auto c = value[i];
                const bool allowed = (c >= 'A' && c <= 'Z')
                                  || (c >= 'a' && c <= 'z')
                                  || (c >= '0' && c <= '9')
                                  || c == '-' || c == '_';
                if (!allowed)
                    return false;
            }
            return true;
        }

        juce::MemoryBlock decodeBase64Url(const juce::String& value)
        {
            juce::String base64 = value;
            base64 = base64.replaceCharacter('-', '+').replaceCharacter('_', '/');
            while ((base64.length() % 4) != 0)
                base64 += "=";

            juce::MemoryOutputStream stream;
            if (!juce::Base64::convertFromBase64(stream, base64))
                return {};
            return stream.getMemoryBlock();
        }

        bool requiredString(const juce::DynamicObject& object, const char* key, juce::String& result)
        {
            const juce::Identifier id(key);
            if (!object.hasProperty(id))
                return false;

            const auto value = object.getProperty(id);
            if (!value.isString())
                return false;

            result = value.toString();
            return result.isNotEmpty();
        }

        bool requiredInteger(const juce::DynamicObject& object, const char* key, int64_t& result)
        {
            const juce::Identifier id(key);
            if (!object.hasProperty(id))
                return false;

            const auto value = object.getProperty(id);
            if (!value.isInt() && !value.isInt64())
                return false;

            result = static_cast<int64_t>(value);
            return true;
        }

        bool requiredInt(const juce::DynamicObject& object, const char* key, int& result)
        {
            int64_t number = 0;
            if (!requiredInteger(object, key, number)
                || number < std::numeric_limits<int>::min()
                || number > std::numeric_limits<int>::max())
                return false;

            result = static_cast<int>(number);
            return true;
        }

        void hexToBytes(const char* hex, uint8_t* output, size_t outputLength)
        {
            const auto nibble = [](char c) -> uint8_t
            {
                if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
                if (c >= 'a' && c <= 'f') return static_cast<uint8_t>(c - 'a' + 10);
                if (c >= 'A' && c <= 'F') return static_cast<uint8_t>(c - 'A' + 10);
                return 0;
            };

            for (size_t i = 0; i < outputLength; ++i)
                output[i] = static_cast<uint8_t>((nibble(hex[i * 2]) << 4) | nibble(hex[i * 2 + 1]));
        }
    }

    LicenseClient& LicenseClient::getInstance()
    {
        static LicenseClient instance;
        return instance;
    }

    LicenseClient::LicenseClient()
    {
        initialize();
    }

    juce::File LicenseClient::getLicenseDirectory()
    {
        auto directory = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                             .getChildFile("ProducerToy")
                             .getChildFile("Licenses");
        if (!directory.exists())
            directory.createDirectory();
        return directory;
    }

    juce::File LicenseClient::getLicenseFile()
    {
        return getLicenseDirectory().getChildFile("proverb.ptlic");
    }

    juce::String LicenseClient::getHardwareDeviceName() const
    {
        auto name = juce::SystemStats::getComputerName();
        return name.isEmpty() ? "Studio Audio Machine" : name;
    }

    juce::String LicenseClient::getHardwareDeviceHash() const
    {
        juce::String hardwareId;

#if JUCE_WINDOWS
        hardwareId = juce::WindowsRegistry::getValue(
            "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Cryptography\\MachineGuid", "").trim();
#endif

        if (hardwareId.isEmpty())
            hardwareId = juce::SystemStats::getCpuModel() + "_" + juce::SystemStats::getOperatingSystemName();

        const auto raw = hardwareId + "_ProducerToy_Entitlement";
        uint8_t hash[64] = {};
        ProducerToy::Crypto::SHA512::hash(
            reinterpret_cast<const uint8_t*>(raw.toRawUTF8()),
            static_cast<size_t>(raw.getNumBytesAsUTF8()), hash);

        // Keep JUCE's spaced byte-hex format stable; the issuer tool accepts this exact value.
        return juce::String::toHexString(hash, 32).toLowerCase();
    }

    void LicenseClient::initialize()
    {
        LicenseState state;
        const auto licenseFile = getLicenseFile();

        if (licenseFile.existsAsFile() && licenseFile.getSize() > 0 && licenseFile.getSize() <= 8192)
        {
            const auto token = licenseFile.loadFileAsString().trim();
            if (verifyToken(token, state))
            {
                const juce::ScopedLock lock(stateLock);
                currentState = state;
                isActivatedAtomic.store(true, std::memory_order_relaxed);
                return;
            }
        }

        const juce::ScopedLock lock(stateLock);
        currentState = LicenseState();
        isActivatedAtomic.store(false, std::memory_order_relaxed);
    }

    LicenseState LicenseClient::getCurrentState() const
    {
        const juce::ScopedLock lock(stateLock);
        return currentState;
    }

    bool LicenseClient::verifyToken(const juce::String& token, LicenseState& outState) const
    {
        outState = LicenseState();
        if (token.isEmpty() || token.length() > 8192)
            return false;

        const int separator = token.indexOfChar('.');
        if (separator <= 0 || separator != token.lastIndexOfChar('.'))
            return false;

        const auto encodedPayload = token.substring(0, separator);
        const auto encodedSignature = token.substring(separator + 1);
        if (!isBase64Url(encodedPayload) || !isBase64Url(encodedSignature))
            return false;

        const auto signatureBytes = decodeBase64Url(encodedSignature);
        if (signatureBytes.getSize() != 64)
            return false;

        uint8_t publicKey[32] = {};
        hexToBytes(ProductConfig::publicKeyHex, publicKey, sizeof(publicKey));

        const auto* message = reinterpret_cast<const uint8_t*>(encodedPayload.toRawUTF8());
        const auto messageLength = static_cast<size_t>(encodedPayload.getNumBytesAsUTF8());
        if (!ProducerToy::Crypto::Ed25519::verify(
                static_cast<const uint8_t*>(signatureBytes.getData()), message, messageLength, publicKey))
            return false;

        const auto payloadBytes = decodeBase64Url(encodedPayload);
        if (payloadBytes.getSize() == 0 || payloadBytes.getSize() > 4096)
            return false;

        const auto parsed = juce::JSON::parse(payloadBytes.toString());
        const auto* object = parsed.getDynamicObject();
        if (object == nullptr)
            return false;

        int version = 0;
        int64_t issuedAt = 0, notBefore = 0, expiresAt = 0;
        juce::String issuer, audience, productId, pluginId, keyId, status, tier;
        juce::String deviceHash, activationId, userName, maskedEmail;

        if (!requiredInt(*object, "v", version) || version != 2
            || !requiredString(*object, "iss", issuer) || issuer != "producertoy.com"
            || !requiredString(*object, "aud", audience) || audience != ProductConfig::pluginIdentifier
            || !requiredString(*object, "pid", productId) || productId != ProductConfig::productId
            || !requiredString(*object, "plugin_id", pluginId) || pluginId != ProductConfig::pluginIdentifier
            || !requiredString(*object, "kid", keyId) || keyId != ProductConfig::signingKeyId
            || !requiredString(*object, "status", status) || status != "active"
            || !requiredString(*object, "tier", tier) || tier != "perpetual"
            || !requiredString(*object, "dev_hash", deviceHash)
            || !requiredString(*object, "aid", activationId)
            || !requiredString(*object, "user_name", userName)
            || !requiredString(*object, "user_email_masked", maskedEmail)
            || !requiredInteger(*object, "iat", issuedAt)
            || !requiredInteger(*object, "nbf", notBefore)
            || !requiredInteger(*object, "exp", expiresAt))
            return false;

        const auto now = juce::Time::getCurrentTime().toMilliseconds() / 1000;
        constexpr int64_t allowedClockSkew = 300;
        if (issuedAt <= 0 || issuedAt > now + allowedClockSkew
            || notBefore <= 0 || notBefore > now + allowedClockSkew
            || expiresAt < 0
            || (expiresAt != 0 && now > expiresAt)
            || deviceHash != getHardwareDeviceHash()
            || userName.length() > 128 || maskedEmail.length() > 254
            || activationId.length() > 128)
            return false;

        outState.isActivated = true;
        outState.registeredName = userName;
        outState.maskedEmail = maskedEmail;
        outState.licenseTier = tier;
        outState.issuedAt = juce::Time(issuedAt * 1000);
        outState.activationId = activationId;
        outState.deviceHash = deviceHash;
        outState.rawToken = token;
        return true;
    }

    bool LicenseClient::installLicenseFile(const juce::File& source, juce::String& error)
    {
        error.clear();
        if (!source.existsAsFile() || source.getSize() <= 0 || source.getSize() > 8192)
        {
            error = "Choose a valid ProVerb .ptlic license file.";
            return false;
        }

        const auto token = source.loadFileAsString().trim();
        LicenseState validatedState;
        if (!verifyToken(token, validatedState))
        {
            error = "This license is invalid, for another device, or was issued for a different ProVerb build.";
            return false;
        }

        const auto destination = getLicenseFile();
        if (!destination.replaceWithText(token))
        {
            error = "Windows could not save the license in your user profile. Check folder permissions and try again.";
            return false;
        }

        initialize();
        if (!isLicenseValid())
        {
            destination.deleteFile();
            error = "The saved license could not be reloaded. Please contact Producer Toy support.";
            return false;
        }

        return true;
    }

    void LicenseClient::deactivate()
    {
        const auto licenseFile = getLicenseFile();
        if (licenseFile.existsAsFile())
            licenseFile.deleteFile();

        const juce::ScopedLock lock(stateLock);
        currentState = LicenseState();
        isActivatedAtomic.store(false, std::memory_order_relaxed);
    }
}
