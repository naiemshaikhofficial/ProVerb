#include "LicenseClient.h"
#include "Ed25519Verifier.h"
#include <juce_events/juce_events.h>
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

        // 8-bit CRC lookup table with polynomial 0x07 (CRC-8-CCITT)
        static constexpr uint8_t s_crc8Table[256] = {
            0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15, 0x38, 0x3F, 0x36, 0x31, 0x24, 0x23, 0x2A, 0x2D,
            0x70, 0x77, 0x7E, 0x79, 0x6C, 0x6B, 0x62, 0x65, 0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x5A, 0x5D,
            0xE0, 0xE7, 0xEE, 0xE9, 0xFC, 0xFB, 0xF2, 0xF5, 0xD8, 0xDF, 0xD6, 0xD1, 0xC4, 0xC3, 0xCA, 0xCD,
            0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B, 0x82, 0x85, 0xA8, 0xAF, 0xA6, 0xA1, 0xB4, 0xB3, 0xBA, 0xBD,
            0xC7, 0xC0, 0xC9, 0xCE, 0xDB, 0xDC, 0xD5, 0xD2, 0xFF, 0xF8, 0xF1, 0xF6, 0xE3, 0xE4, 0xED, 0xEA,
            0xB7, 0xB0, 0xB9, 0xBE, 0xAB, 0xAC, 0xA5, 0xA2, 0x8F, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A,
            0x27, 0x20, 0x29, 0x2E, 0x3B, 0x3C, 0x35, 0x32, 0x1F, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0D, 0x0A,
            0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42, 0x6F, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7D, 0x7A,
            0x89, 0x8E, 0x87, 0x80, 0x95, 0x92, 0x9B, 0x9C, 0xB1, 0xB6, 0xBF, 0xB8, 0xAD, 0xAA, 0xA3, 0xA4,
            0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC, 0xC1, 0xC6, 0xCF, 0xC8, 0xDD, 0xDA, 0xD3, 0xD4,
            0x69, 0x6E, 0x67, 0x60, 0x75, 0x72, 0x7B, 0x7C, 0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44,
            0x19, 0x1E, 0x17, 0x10, 0x05, 0x02, 0x0B, 0x0C, 0x21, 0x26, 0x2F, 0x28, 0x3D, 0x3A, 0x33, 0x34,
            0x4E, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5C, 0x5B, 0x76, 0x71, 0x78, 0x7F, 0x6A, 0x6D, 0x64, 0x63,
            0x3E, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B, 0x06, 0x01, 0x08, 0x0F, 0x1A, 0x1D, 0x14, 0x13,
            0xAE, 0xA9, 0xA0, 0xA7, 0xB2, 0xB5, 0xBC, 0xBB, 0x96, 0x91, 0x98, 0x9F, 0x8A, 0x8D, 0x84, 0x83,
            0xDE, 0xD9, 0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB, 0xE6, 0xE1, 0xE8, 0xEF, 0xFA, 0xFD, 0xF4, 0xF3
        };

        static inline uint8_t computeCrc8(uint64_t val) noexcept
        {
            uint8_t crc = 0xAA;
            for (int i = 0; i < 7; ++i)
            {
                const uint8_t byte = static_cast<uint8_t>((val >> (i * 8)) & 0xFF);
                crc = s_crc8Table[crc ^ byte];
            }
            return crc;
        }

        static uint64_t computeTokenProof(const uint8_t* sigBytes, const juce::String& cleanLocalDev) noexcept
        {
            // 64-bit FNV-1a Hash folded with verified signature and local device binding
            uint64_t h = 0xcbf29ce484222325ULL;
            for (int i = 0; i < 64; ++i)
            {
                h ^= static_cast<uint64_t>(sigBytes[i]);
                h *= 0x100000001b3ULL;
            }

            const auto devUtf8 = cleanLocalDev.toRawUTF8();
            const auto devLen = static_cast<size_t>(cleanLocalDev.getNumBytesAsUTF8());
            for (size_t i = 0; i < devLen; ++i)
            {
                h ^= static_cast<uint64_t>(static_cast<uint8_t>(devUtf8[i]));
                h *= 0x100000001b3ULL;
            }

            // Salt with invariant product word 'smartver'
            h ^= 0x736d617274766572ULL;
            h *= 0x100000001b3ULL;

            // Invariant checksum in lower 8 bits
            const uint64_t high56 = h & 0xFFFFFFFFFFFFFF00ULL;
            const uint8_t crc = computeCrc8(high56 ^ 0xA5A5A5A5A5A5A5A5ULL);
            return high56 | static_cast<uint64_t>(crc);
        }

        // 32-byte Ed25519 public key XOR masked to prevent plain string/byte pattern extraction
        static constexpr uint8_t s_pkMask = 0x5a;
        static constexpr uint8_t s_maskedPk[32] = {
            0x87 ^ s_pkMask, 0x43 ^ s_pkMask, 0x92 ^ s_pkMask, 0xbb ^ s_pkMask,
            0x5c ^ s_pkMask, 0x67 ^ s_pkMask, 0xa3 ^ s_pkMask, 0xe2 ^ s_pkMask,
            0xd9 ^ s_pkMask, 0x1a ^ s_pkMask, 0xf3 ^ s_pkMask, 0xbf ^ s_pkMask,
            0x4d ^ s_pkMask, 0x4d ^ s_pkMask, 0xe8 ^ s_pkMask, 0x20 ^ s_pkMask,
            0x4b ^ s_pkMask, 0x39 ^ s_pkMask, 0xe8 ^ s_pkMask, 0x56 ^ s_pkMask,
            0x84 ^ s_pkMask, 0xdf ^ s_pkMask, 0x76 ^ s_pkMask, 0x90 ^ s_pkMask,
            0x0f ^ s_pkMask, 0x87 ^ s_pkMask, 0xa8 ^ s_pkMask, 0x80 ^ s_pkMask,
            0xc8 ^ s_pkMask, 0xfb ^ s_pkMask, 0xd2 ^ s_pkMask, 0xe9 ^ s_pkMask
        };

        static void getUnmaskedPublicKey(uint8_t outPk[32]) noexcept
        {
            for (size_t i = 0; i < 32; ++i)
                outPk[i] = static_cast<uint8_t>(s_maskedPk[i] ^ s_pkMask);
        }

        juce::String extractErrorMessage(const juce::var& jsonVar, const juce::String& fallback)
        {
            if (auto* obj = jsonVar.getDynamicObject())
            {
                if (obj->hasProperty("error"))
                {
                    const auto errVar = obj->getProperty("error");
                    if (errVar.isString())
                        return errVar.toString();
                    if (auto* errObj = errVar.getDynamicObject())
                    {
                        if (errObj->hasProperty("message") && errObj->getProperty("message").isString())
                            return errObj->getProperty("message").toString();
                        if (errObj->hasProperty("code") && errObj->getProperty("code").isString())
                            return errObj->getProperty("code").toString();
                    }
                }
                if (obj->hasProperty("message") && obj->getProperty("message").isString())
                    return obj->getProperty("message").toString();
            }
            return fallback;
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

    LicenseClient::~LicenseClient()
    {
        cancelPairingFlow();
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

    juce::String LicenseClient::getEffectiveApiUrl() const
    {
        const auto envUrl = juce::SystemStats::getEnvironmentVariable("PROVERB_API_URL", "").trim();
        if (envUrl.isNotEmpty())
            return envUrl;
        return ProductConfig::apiBaseUrl;
    }

    juce::String LicenseClient::getHardwareDeviceHash() const
    {
        juce::String hardwareId;

#if JUCE_WINDOWS
        // Query the canonical 64-bit registry view so 32-bit and 64-bit DAW hosts produce bit-exact identical device hashes
        hardwareId = juce::WindowsRegistry::getValue(
            "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Cryptography\\MachineGuid", "", juce::WindowsRegistry::WoW64_64bit).trim();
        if (hardwareId.isEmpty())
        {
            hardwareId = juce::WindowsRegistry::getValue(
                "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Cryptography\\MachineGuid", "").trim();
        }
#elif JUCE_MAC
        hardwareId = juce::SystemStats::getDeviceDescription().trim();
#else
        hardwareId = juce::SystemStats::getDeviceDescription().trim();
#endif

        if (hardwareId.isEmpty())
            hardwareId = juce::SystemStats::getCpuModel() + "_" + juce::SystemStats::getOperatingSystemName();

        const auto raw = hardwareId + "_ProducerToy_Entitlement";
        uint8_t hash[64] = {};
        ProducerToy::Crypto::SHA512::hash(
            reinterpret_cast<const uint8_t*>(raw.toRawUTF8()),
            static_cast<size_t>(raw.getNumBytesAsUTF8()), hash);

        // Canonical 64-character lowercase hex format
        juce::String hex;
        for (int i = 0; i < 32; ++i)
            hex << juce::String::toHexString((int) hash[i]).paddedLeft('0', 2).toLowerCase();
        return hex;
    }

    void LicenseClient::initialize()
    {
        LicenseState state;
        const auto licenseFile = getLicenseFile();

        if (licenseFile.existsAsFile() && licenseFile.getSize() > 0 && licenseFile.getSize() <= 8192)
        {
            const auto token = licenseFile.loadFileAsString().trim();
            uint64_t proof = 0ULL;
            if (verifyToken(token, state, &proof))
            {
                const juce::ScopedLock lock(stateLock);
                currentState = state;
                activeEntitlementProof.store(proof, std::memory_order_relaxed);
                isActivatedAtomic.store(true, std::memory_order_relaxed);
                return;
            }
        }

        const juce::ScopedLock lock(stateLock);
        currentState = LicenseState();
        activeEntitlementProof.store(0ULL, std::memory_order_relaxed);
        isActivatedAtomic.store(false, std::memory_order_relaxed);
    }

    LicenseState LicenseClient::getCurrentState() const
    {
        const juce::ScopedLock lock(stateLock);
        return currentState;
    }

    bool LicenseClient::verifyToken(const juce::String& token, LicenseState& outState, uint64_t* outProof) const
    {
        if (outProof != nullptr)
            *outProof = 0ULL;

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
        getUnmaskedPublicKey(publicKey);

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

        const auto cleanTokenDev = deviceHash.removeCharacters(" -").toLowerCase();
        const auto cleanLocalDev = getHardwareDeviceHash().removeCharacters(" -").toLowerCase();

        const auto now = juce::Time::getCurrentTime().toMilliseconds() / 1000;
        constexpr int64_t allowedClockSkew = 300;
        if (issuedAt <= 0 || issuedAt > now + allowedClockSkew
            || notBefore <= 0 || notBefore > now + allowedClockSkew
            || expiresAt < 0
            || (expiresAt != 0 && now > expiresAt)
            || cleanTokenDev != cleanLocalDev
            || userName.length() > 128 || maskedEmail.length() > 254
            || activationId.length() > 128)
            return false;

        const uint64_t proof = computeTokenProof(
            static_cast<const uint8_t*>(signatureBytes.getData()), cleanLocalDev);

        if (outProof != nullptr)
            *outProof = proof;

        outState.isActivated = true;
        outState.registeredName = userName;
        outState.maskedEmail = maskedEmail;
        outState.licenseTier = tier;
        outState.issuedAt = juce::Time(issuedAt * 1000);
        outState.activationId = activationId;
        outState.deviceHash = cleanLocalDev;
        outState.rawToken = token;
        return true;
    }

    bool LicenseClient::verifyEntitlementProofStatic(uint64_t proof) noexcept
    {
        if (proof == 0ULL || proof == 0xFFFFFFFFFFFFFFFFULL)
            return false;

        const uint64_t high56 = proof & 0xFFFFFFFFFFFFFF00ULL;
        if (high56 == 0ULL)
            return false;

        const uint8_t expectedCrc = computeCrc8(high56 ^ 0xA5A5A5A5A5A5A5A5ULL);
        return (static_cast<uint8_t>(proof & 0xFFULL) == expectedCrc);
    }

    float LicenseClient::computeEntitlementMultiplier(uint64_t proof) noexcept
    {
        return verifyEntitlementProofStatic(proof) ? 1.0f : 0.0f;
    }

    bool LicenseClient::installLicenseToken(const juce::String& token, juce::String& error)
    {
        error.clear();
        const auto cleanToken = token.trim();
        if (cleanToken.isEmpty() || cleanToken.length() > 8192)
        {
            error = "Invalid or empty license token.";
            return false;
        }

        LicenseState validatedState;
        if (!verifyToken(cleanToken, validatedState))
        {
            error = "This license is invalid, for another device, or was issued for a different ProVerb build.";
            return false;
        }

        const auto destination = getLicenseFile();
        if (!destination.replaceWithText(cleanToken))
        {
            error = "Could not save the license file in your user profile. Check folder permissions and try again.";
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

    bool LicenseClient::installLicenseFile(const juce::File& source, juce::String& error)
    {
        error.clear();
        if (!source.existsAsFile() || source.getSize() <= 0 || source.getSize() > 8192)
        {
            error = "Choose a valid ProVerb .ptlic license file.";
            return false;
        }

        return installLicenseToken(source.loadFileAsString(), error);
    }

    class LicenseClient::PairingWorker : public juce::Thread
    {
    public:
        PairingWorker(LicenseClient& clientIn,
                      std::function<void(bool, const juce::String&, const juce::String&, const juce::String&)> onReadyIn,
                      std::function<void(bool, const juce::String&)> onCompleteIn)
            : juce::Thread("ProVerbPairingWorker"),
              owner(clientIn),
              onReady(std::move(onReadyIn)),
              onComplete(std::move(onCompleteIn))
        {
        }

        void run() override
        {
            const auto apiUrl = owner.getEffectiveApiUrl();
            const auto devId = owner.getHardwareDeviceHash();
            const auto devName = owner.getHardwareDeviceName();

            juce::var startReq = new juce::DynamicObject();
            startReq.getDynamicObject()->setProperty("productId", ProductConfig::productId);
            startReq.getDynamicObject()->setProperty("deviceId", devId);
            startReq.getDynamicObject()->setProperty("deviceName", devName);

            juce::URL startUrl(apiUrl + "/pair/start");
            startUrl = startUrl.withPOSTData(juce::JSON::toString(startReq));

            int statusCode = 0;
            juce::String responseStr;
            {
                auto inStream = startUrl.createInputStream(
                    juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                        .withConnectionTimeoutMs(6000)
                        .withStatusCode(&statusCode)
                        .withExtraHeaders("Content-Type: application/json\r\nAccept: application/json\r\n"));

                if (inStream != nullptr)
                    responseStr = inStream->readEntireStreamAsString();
            }

            if (threadShouldExit())
                return;

            auto parsed = juce::JSON::parse(responseStr);
            auto* obj = parsed.getDynamicObject();
            if (statusCode != 200 || obj == nullptr || !obj->getProperty("success"))
            {
                juce::String err = extractErrorMessage(parsed, "");
                if (err.isEmpty())
                {
                    if (statusCode > 0 && statusCode != 200)
                        err = "Licensing server returned HTTP " + juce::String(statusCode) + ". Check internet or use Serial / Offline File.";
                    else
                        err = "Licensing server unreachable. Check connection or use Serial / Offline File.";
                }

                juce::MessageManager::callAsync([cb = onReady, err]()
                {
                    if (cb) cb(false, "", "", err);
                });
                return;
            }

            const auto code = obj->getProperty("code").toString();
            const auto claimUrl = obj->getProperty("claimUrl").toString();

            juce::MessageManager::callAsync([cb = onReady, code, claimUrl]()
            {
                if (cb) cb(true, code, claimUrl, "");
            });

            // Poll for claim completion up to 300 seconds
            const auto startTime = juce::Time::getMillisecondCounter();
            while (!threadShouldExit())
            {
                if (juce::Time::getMillisecondCounter() - startTime > 300000)
                {
                    juce::MessageManager::callAsync([cb = onComplete]()
                    {
                        if (cb) cb(false, "Pairing timed out. Please try again.");
                    });
                    return;
                }

                wait(2000);
                if (threadShouldExit())
                    return;

                juce::URL pollUrl(apiUrl + "/pair/poll?code=" + juce::URL::addEscapeChars(code, true)
                                  + "&deviceId=" + juce::URL::addEscapeChars(devId, true));

                int pollStatus = 0;
                juce::String pollResp;
                {
                    auto inStream = pollUrl.createInputStream(
                        juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                            .withConnectionTimeoutMs(4000)
                            .withStatusCode(&pollStatus)
                            .withExtraHeaders("Accept: application/json\r\n"));

                    if (inStream != nullptr)
                        pollResp = inStream->readEntireStreamAsString();
                }

                if (threadShouldExit())
                    return;

                auto pollParsed = juce::JSON::parse(pollResp);
                if (auto* pObj = pollParsed.getDynamicObject())
                {
                    const auto status = pObj->getProperty("status").toString();
                    if (status == "claimed")
                    {
                        const auto token = pObj->getProperty("token").toString();
                        juce::String installErr;
                        if (owner.installLicenseToken(token, installErr))
                        {
                            juce::MessageManager::callAsync([cb = onComplete]()
                            {
                                if (cb) cb(true, "Device successfully activated online.");
                            });
                            return;
                        }
                        else
                        {
                            juce::MessageManager::callAsync([cb = onComplete, installErr]()
                            {
                                if (cb) cb(false, "Token installation failed: " + installErr);
                            });
                            return;
                        }
                    }
                    else if (status == "expired" || status == "rejected")
                    {
                        juce::MessageManager::callAsync([cb = onComplete, status]()
                        {
                            if (cb) cb(false, "Pairing " + status + ". Please start again.");
                        });
                        return;
                    }
                }
            }
        }

    private:
        LicenseClient& owner;
        std::function<void(bool, const juce::String&, const juce::String&, const juce::String&)> onReady;
        std::function<void(bool, const juce::String&)> onComplete;
    };

    void LicenseClient::activateWithSerial(const juce::String& serialKey,
                                           std::function<void(bool, const juce::String&)> callback)
    {
        const auto cleanSerial = serialKey.trim().toUpperCase();
        if (cleanSerial.isEmpty())
        {
            if (callback) callback(false, "Please enter your ProVerb serial key.");
            return;
        }

        juce::Thread::launch([this, cleanSerial, callback]()
        {
            const auto apiUrl = getEffectiveApiUrl();
            const auto devId = getHardwareDeviceHash();
            const auto devName = getHardwareDeviceName();

            juce::var req = new juce::DynamicObject();
            req.getDynamicObject()->setProperty("productId", ProductConfig::productId);
            req.getDynamicObject()->setProperty("deviceId", devId);
            req.getDynamicObject()->setProperty("deviceName", devName);
            req.getDynamicObject()->setProperty("serialKey", cleanSerial);

            juce::URL actUrl(apiUrl + "/serial/activate");
            actUrl = actUrl.withPOSTData(juce::JSON::toString(req));

            int statusCode = 0;
            juce::String respStr;
            {
                auto inStream = actUrl.createInputStream(
                    juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                        .withConnectionTimeoutMs(8000)
                        .withStatusCode(&statusCode)
                        .withExtraHeaders("Content-Type: application/json\r\nAccept: application/json\r\n"));

                if (inStream != nullptr)
                    respStr = inStream->readEntireStreamAsString();
            }

            auto parsed = juce::JSON::parse(respStr);
            auto* obj = parsed.getDynamicObject();
            if (statusCode != 200 || obj == nullptr || !obj->getProperty("success"))
            {
                juce::String errMsg = extractErrorMessage(parsed, "");
                if (errMsg.isEmpty())
                {
                    if (statusCode > 0 && statusCode != 200)
                        errMsg = "Licensing server returned HTTP " + juce::String(statusCode) + ". Please check connection or use Offline License File.";
                    else
                        errMsg = "Could not reach the dedicated licensing server. Please check connection or use Offline License File.";
                }

                juce::MessageManager::callAsync([callback, errMsg]()
                {
                    if (callback) callback(false, errMsg);
                });
                return;
            }

            const auto token = obj->getProperty("token").toString();
            juce::String installErr;
            if (!installLicenseToken(token, installErr))
            {
                juce::MessageManager::callAsync([callback, installErr]()
                {
                    if (callback) callback(false, "Received invalid license from server: " + installErr);
                });
                return;
            }

            juce::MessageManager::callAsync([callback]()
            {
                if (callback) callback(true, "Serial key redeemed! ProVerb is activated.");
            });
        });
    }

    void LicenseClient::startOnlinePairing(
        std::function<void(bool, const juce::String&, const juce::String&, const juce::String&)> onCodeReady,
        std::function<void(bool, const juce::String&)> onComplete)
    {
        cancelPairingFlow();
        activePairingWorker = std::make_unique<PairingWorker>(*this, std::move(onCodeReady), std::move(onComplete));
        activePairingWorker->startThread(juce::Thread::Priority::normal);
    }

    void LicenseClient::cancelPairingFlow()
    {
        if (activePairingWorker != nullptr)
        {
            activePairingWorker->signalThreadShouldExit();
            activePairingWorker->stopThread(2000);
            activePairingWorker.reset();
        }
    }

    void LicenseClient::deactivate(std::function<void(bool, const juce::String&)> callback)
    {
        cancelPairingFlow();

        const auto licenseFile = getLicenseFile();
        if (licenseFile.existsAsFile())
            licenseFile.deleteFile();

        juce::String actId;
        juce::String devId = getHardwareDeviceHash();
        {
            const juce::ScopedLock lock(stateLock);
            actId = currentState.activationId;
            currentState = LicenseState();
            activeEntitlementProof.store(0ULL, std::memory_order_relaxed);
            isActivatedAtomic.store(false, std::memory_order_relaxed);
        }

        // Notify server asynchronously in background (fire-and-forget, non-blocking)
        if (actId.isNotEmpty())
        {
            const auto apiUrl = getEffectiveApiUrl();
            juce::Thread::launch([apiUrl, actId, devId]()
            {
                juce::var req = new juce::DynamicObject();
                req.getDynamicObject()->setProperty("productId", ProductConfig::productId);
                req.getDynamicObject()->setProperty("activationId", actId);
                req.getDynamicObject()->setProperty("deviceId", devId);

                juce::URL deactUrl(apiUrl + "/deactivate");
                deactUrl = deactUrl.withPOSTData(juce::JSON::toString(req));

                int code = 0;
                auto inStream = deactUrl.createInputStream(
                    juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                        .withConnectionTimeoutMs(4000)
                        .withStatusCode(&code)
                        .withExtraHeaders("Content-Type: application/json\r\nAccept: application/json\r\n"));
            });
        }

        if (callback)
            callback(true, "Local license file removed.");
    }
}
