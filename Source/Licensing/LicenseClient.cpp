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
        hardwareId = juce::WindowsRegistry::getValue(
            "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Cryptography\\MachineGuid", "").trim();
#elif JUCE_MAC
        hardwareId = juce::SystemStats::getUniqueDeviceDescription().trim();
#else
        hardwareId = juce::SystemStats::getUniqueDeviceDescription().trim();
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
                juce::String err = obj ? obj->getProperty("error").toString() : "";
                if (err.isEmpty())
                    err = "Dedicated licensing server unreachable. Check connection or use Serial / Offline File.";

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
                juce::String errMsg = obj ? obj->getProperty("error").toString() : "";
                if (errMsg.isEmpty())
                    errMsg = "Could not reach the dedicated licensing server. Please check connection or use Offline License File.";

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
