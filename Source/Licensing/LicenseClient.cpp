#include "LicenseClient.h"
#include "Ed25519Verifier.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace ProducerToy::Licensing
{
    static juce::MemoryBlock decodeBase64Url(const juce::String& s)
    {
        juce::String b64 = s;
        b64 = b64.replaceCharacter('-', '+').replaceCharacter('_', '/');
        while ((b64.length() % 4) != 0)
            b64 += "=";

        juce::MemoryOutputStream os;
        juce::Base64::convertFromBase64(os, b64);
        return os.getMemoryBlock();
    }

    static void hexToBytes(const char* hex, uint8_t* out, size_t outLen)
    {
        auto nibble = [](char c) -> uint8_t {
            if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
            if (c >= 'a' && c <= 'f') return static_cast<uint8_t>(c - 'a' + 10);
            if (c >= 'A' && c <= 'F') return static_cast<uint8_t>(c - 'A' + 10);
            return 0;
        };

        for (size_t i = 0; i < outLen; ++i)
        {
            out[i] = (nibble(hex[i * 2]) << 4) | nibble(hex[i * 2 + 1]);
        }
    }

    // ==============================================================================
    // Background Worker for Desktop Pairing & Polling Flow
    // ==============================================================================
    class LicenseClient::PairingWorker : public juce::Thread
    {
    public:
        PairingWorker(LicenseClient& ownerClient,
                      std::function<void(bool, const juce::String&, const juce::String&)> onCode,
                      std::function<void(bool, const juce::String&)> onDone)
            : juce::Thread("ProducerToy_LicensePairingThread"),
              client(ownerClient),
              codeCallback(std::move(onCode)),
              doneCallback(std::move(onDone))
        {
        }

        ~PairingWorker() override
        {
            stopThread(4000);
        }

        void run() override
        {
            // 1. Send pairing challenge creation request to server
            juce::var reqObj = new juce::DynamicObject();
            reqObj.getDynamicObject()->setProperty("productId", ProductConfig::productId);
            reqObj.getDynamicObject()->setProperty("deviceHash", client.getHardwareDeviceHash());
            reqObj.getDynamicObject()->setProperty("deviceName", client.getHardwareDeviceName());
            reqObj.getDynamicObject()->setProperty("os", juce::SystemStats::getOperatingSystemName());

            juce::String postJson = juce::JSON::toString(reqObj);

            juce::URL challengeUrl(juce::String(ProductConfig::apiBaseUrl) + "/challenge");
            challengeUrl = challengeUrl.withPOSTData(postJson);

            int statusCode = 0;
            auto inStream = challengeUrl.createInputStream(juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                                                            .withConnectionTimeoutMs(8000)
                                                            .withResponseHeaders(nullptr)
                                                            .withStatusCode(&statusCode)
                                                            .withExtraHeaders("Content-Type: application/json\r\nAccept: application/json\r\nUser-Agent: ProducerToy-ProVerb/1.0\r\n"));

            if (threadShouldExit()) return;

            if (inStream == nullptr || statusCode < 200 || statusCode >= 300)
            {
                notifyCode(false, {}, "Could not connect to Producer Toy licensing server. Please check your internet connection.");
                return;
            }

            juce::String responseText = inStream->readEntireStreamAsString();
            juce::var parsed = juce::JSON::parse(responseText);

            if (!parsed.isObject() || !parsed.getProperty("success", false))
            {
                juce::String err = parsed.getProperty("error", "Error creating activation challenge").toString();
                notifyCode(false, {}, err);
                return;
            }

            juce::String challengeId = parsed.getProperty("challengeId", "").toString();
            juce::String pairingCode = parsed.getProperty("pairingCode", "").toString();
            juce::String activateUrl = parsed.getProperty("activateUrl", "").toString();

            if (challengeId.isEmpty() || pairingCode.isEmpty())
            {
                notifyCode(false, {}, "Malformed server pairing response");
                return;
            }

            // Successfully received pairing code -> notify UI and launch default browser
            notifyCode(true, pairingCode, {});
            if (activateUrl.isNotEmpty())
            {
                juce::URL(activateUrl).launchInDefaultBrowser();
            }

            // 2. Poll server for user claim authorization
            int pollAttempts = 0;
            const int maxPollAttempts = 150; // ~7.5 minutes total (3s intervals)

            while (!threadShouldExit() && pollAttempts < maxPollAttempts)
            {
                wait(3000);
                if (threadShouldExit()) return;
                pollAttempts++;

                juce::URL pollUrl(juce::String(ProductConfig::apiBaseUrl) + "/poll");
                pollUrl = pollUrl.withParameter("challengeId", challengeId)
                                 .withParameter("deviceHash", client.getHardwareDeviceHash());

                int pollStatus = 0;
                auto pollStream = pollUrl.createInputStream(juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                                                              .withConnectionTimeoutMs(5000)
                                                              .withExtraHeaders("User-Agent: ProducerToy-ProVerb/1.0\r\n")
                                                              .withStatusCode(&pollStatus));

                if (pollStream != nullptr && pollStatus == 200)
                {
                    juce::String pollResponse = pollStream->readEntireStreamAsString();
                    juce::var pollJson = juce::JSON::parse(pollResponse);

                    if (pollJson.isObject())
                    {
                        juce::String status = pollJson.getProperty("status", "").toString();

                        if (status == "claimed")
                        {
                            juce::String token = pollJson.getProperty("entitlementToken", "").toString();
                            LicenseState state;
                            if (client.verifyToken(token, state))
                            {
                                // Save verified token safely to AppData
                                auto licFile = LicenseClient::getLicenseFile();
                                licFile.create();
                                licFile.replaceWithText(token);

                                {
                                    const juce::ScopedLock sl(client.stateLock);
                                    client.currentState = state;
                                    client.isActivatedAtomic.store(true, std::memory_order_relaxed);
                                }

                                notifyDone(true, {});
                                return;
                            }
                            else
                            {
                                notifyDone(false, "Server issued an invalid or untrusted cryptographic entitlement token.");
                                return;
                            }
                        }
                        else if (status == "expired")
                        {
                            notifyDone(false, "Pairing code has expired. Please click Activate again.");
                            return;
                        }
                    }
                }
            }

            if (!threadShouldExit())
            {
                notifyDone(false, "Pairing timed out. You can click Activate whenever you are ready.");
            }
        }

    private:
        LicenseClient& client;
        std::function<void(bool, const juce::String&, const juce::String&)> codeCallback;
        std::function<void(bool, const juce::String&)> doneCallback;

        void notifyCode(bool ok, const juce::String& code, const juce::String& err)
        {
            if (codeCallback)
            {
                juce::MessageManager::callAsync([cb = codeCallback, ok, code, err]() {
                    cb(ok, code, err);
                });
            }
        }

        void notifyDone(bool ok, const juce::String& err)
        {
            if (doneCallback)
            {
                juce::MessageManager::callAsync([cb = doneCallback, ok, err]() {
                    cb(ok, err);
                });
            }
        }
    };

    // ==============================================================================
    // LicenseClient Implementation
    // ==============================================================================
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
        auto appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
        auto dir = appData.getChildFile("ProducerToy").getChildFile("Licenses");
        if (!dir.exists())
            dir.createDirectory();
        return dir;
    }

    juce::File LicenseClient::getLicenseFile()
    {
        return getLicenseDirectory().getChildFile("proverb.ptlic");
    }

    juce::String LicenseClient::getHardwareDeviceName() const
    {
        juce::String name = juce::SystemStats::getComputerName();
        if (name.isEmpty()) name = "Studio Audio Machine";
        return name;
    }

    juce::String LicenseClient::getHardwareDeviceHash() const
    {
        juce::String hardwareId;

#if JUCE_WINDOWS
        hardwareId = juce::WindowsRegistry::getValue("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Cryptography\\MachineGuid", "").trim();
#endif

        if (hardwareId.isEmpty())
        {
            hardwareId = juce::SystemStats::getCpuModel() + "_" + juce::SystemStats::getOperatingSystemName();
        }

        juce::String raw = hardwareId + "_ProducerToy_Entitlement";
        uint8_t hash[64] = { 0 };
        ProducerToy::Crypto::SHA512::hash(reinterpret_cast<const uint8_t*>(raw.toRawUTF8()),
                                          static_cast<size_t>(raw.getNumBytesAsUTF8()), hash);
        return juce::String::toHexString(hash, 32).toLowerCase();
    }

    void LicenseClient::initialize()
    {
        auto licFile = getLicenseFile();
        if (licFile.existsAsFile())
        {
            juce::String token = licFile.loadFileAsString().trim();
            LicenseState state;
            if (verifyToken(token, state))
            {
                const juce::ScopedLock sl(stateLock);
                currentState = state;
                isActivatedAtomic.store(true, std::memory_order_relaxed);
                return;
            }
        }

        // Unregistered / Trial state
        const juce::ScopedLock sl(stateLock);
        currentState = LicenseState();
        isActivatedAtomic.store(false, std::memory_order_relaxed);
    }

    LicenseState LicenseClient::getCurrentState() const
    {
        const juce::ScopedLock sl(stateLock);
        return currentState;
    }

    // Standard RFC 6234 / RFC 2104 HMAC-SHA256 Implementation
    static juce::String computeHmacSha256(const juce::String& keyStr, const juce::String& dataStr)
    {
        class SHA256Helper {
        public:
            SHA256Helper() { reset(); }
            void reset() {
                state[0] = 0x6a09e667; state[1] = 0xbb67ae85;
                state[2] = 0x3c6ef372; state[3] = 0xa54ff53a;
                state[4] = 0x510e527f; state[5] = 0x9b05688c;
                state[6] = 0x1f83d9ab; state[7] = 0x5be0cd19;
                count = 0; bufferLen = 0;
            }
            void update(const void* data, size_t len) {
                const uint8_t* p = reinterpret_cast<const uint8_t*>(data);
                for (size_t i = 0; i < len; ++i) {
                    buffer[bufferLen++] = p[i];
                    if (bufferLen == 64) { processBlock(buffer); bufferLen = 0; }
                }
                count += len * 8;
            }
            void finalize(uint8_t out[32]) {
                buffer[bufferLen++] = 0x80;
                if (bufferLen > 56) {
                    while (bufferLen < 64) buffer[bufferLen++] = 0;
                    processBlock(buffer);
                    bufferLen = 0;
                }
                while (bufferLen < 56) buffer[bufferLen++] = 0;
                for (int i = 7; i >= 0; --i) buffer[bufferLen++] = static_cast<uint8_t>((count >> (i * 8)) & 0xFF);
                processBlock(buffer);
                for (int i = 0; i < 8; ++i) {
                    out[i * 4 + 0] = static_cast<uint8_t>((state[i] >> 24) & 0xFF);
                    out[i * 4 + 1] = static_cast<uint8_t>((state[i] >> 16) & 0xFF);
                    out[i * 4 + 2] = static_cast<uint8_t>((state[i] >> 8) & 0xFF);
                    out[i * 4 + 3] = static_cast<uint8_t>(state[i] & 0xFF);
                }
            }
        private:
            uint32_t state[8]; uint64_t count; uint8_t buffer[64]; size_t bufferLen;
            static inline uint32_t rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }
            void processBlock(const uint8_t block[64]) {
                static const uint32_t K[64] = {
                    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
                    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
                    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
                    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
                    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
                    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
                    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
                    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
                };
                uint32_t W[64];
                for (int t = 0; t < 16; ++t) {
                    W[t] = (static_cast<uint32_t>(block[t * 4]) << 24) |
                           (static_cast<uint32_t>(block[t * 4 + 1]) << 16) |
                           (static_cast<uint32_t>(block[t * 4 + 2]) << 8) |
                           (static_cast<uint32_t>(block[t * 4 + 3]));
                }
                for (int t = 16; t < 64; ++t) {
                    uint32_t s0 = rotr(W[t - 15], 7) ^ rotr(W[t - 15], 18) ^ (W[t - 15] >> 3);
                    uint32_t s1 = rotr(W[t - 2], 17) ^ rotr(W[t - 2], 19) ^ (W[t - 2] >> 10);
                    W[t] = W[t - 16] + s0 + W[t - 7] + s1;
                }
                uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
                uint32_t e = state[4], f = state[5], g = state[6], h = state[7];
                for (int t = 0; t < 64; ++t) {
                    uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
                    uint32_t ch = (e & f) ^ ((~e) & g);
                    uint32_t temp1 = h + S1 + ch + K[t] + W[t];
                    uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
                    uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
                    uint32_t temp2 = S0 + maj;
                    h = g; g = f; f = e; e = d + temp1;
                    d = c; c = b; b = a; a = temp1 + temp2;
                }
                state[0] += a; state[1] += b; state[2] += c; state[3] += d;
                state[4] += e; state[5] += f; state[6] += g; state[7] += h;
            }
        };

        auto keyUtf8 = keyStr.toRawUTF8();
        size_t keyLen = static_cast<size_t>(keyStr.getNumBytesAsUTF8());
        auto dataUtf8 = dataStr.toRawUTF8();
        size_t dataLen = static_cast<size_t>(dataStr.getNumBytesAsUTF8());

        uint8_t k[64] = { 0 };
        if (keyLen > 64) {
            SHA256Helper sha;
            sha.update(keyUtf8, keyLen);
            sha.finalize(k);
        } else {
            memcpy(k, keyUtf8, keyLen);
        }

        uint8_t ipad[64], opad[64];
        for (int i = 0; i < 64; ++i) {
            ipad[i] = k[i] ^ 0x36;
            opad[i] = k[i] ^ 0x5C;
        }

        uint8_t innerHash[32];
        SHA256Helper innerSha;
        innerSha.update(ipad, 64);
        innerSha.update(dataUtf8, dataLen);
        innerSha.finalize(innerHash);

        uint8_t finalHash[32];
        SHA256Helper outerSha;
        outerSha.update(opad, 64);
        outerSha.update(innerHash, 32);
        outerSha.finalize(finalHash);

        // Base64URL encode
        static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
        juce::String out;
        int val = 0, valb = -6;
        for (size_t i = 0; i < 32; ++i) {
            val = (val << 8) + finalHash[i];
            valb += 8;
            while (valb >= 0) {
                out += b64[(val >> valb) & 0x3F];
                valb -= 6;
            }
        }
        if (valb > -6) {
            out += b64[((val << 8) >> (valb + 8)) & 0x3F];
        }
        return out;
    }

    bool LicenseClient::verifyToken(const juce::String& token, LicenseState& outState) const
    {
        int dotIndex = token.indexOfChar('.');
        if (dotIndex <= 0)
            return false;

        juce::String encPayload = token.substring(0, dotIndex);
        juce::String encSig     = token.substring(dotIndex + 1);

        // 1. Verify Cryptographic Signature
        juce::String expectedSig = computeHmacSha256(ProductConfig::masterSecret, encPayload);
        bool validSig = (encSig == expectedSig);

        if (!validSig)
        {
            // Fallback: Verify with secondary server secret in case server used JWT_SECRET
            juce::String fallbackSig = computeHmacSha256("producer-toy-secret-key-2026", encPayload);
            validSig = (encSig == fallbackSig);
        }

        if (!validSig)
        {
            // Fallback: Also verify Ed25519 if present
            auto sigBytes = decodeBase64Url(encSig);
            if (sigBytes.getSize() == 64)
            {
                uint8_t pkBytes[32] = { 0 };
                hexToBytes(ProductConfig::publicKeyHex, pkBytes, 32);
                const auto* msgData = reinterpret_cast<const uint8_t*>(encPayload.toRawUTF8());
                size_t msgLen = static_cast<size_t>(encPayload.getNumBytesAsUTF8());
                validSig = ProducerToy::Crypto::Ed25519::verify(
                    static_cast<const uint8_t*>(sigBytes.getData()), msgData, msgLen, pkBytes);
            }
        }

        if (!validSig)
            return false;

        // Parse JSON Payload
        auto payloadBytes = decodeBase64Url(encPayload);
        juce::String jsonStr = payloadBytes.toString();
        juce::var parsed = juce::JSON::parse(jsonStr);

        if (!parsed.isObject())
            return false;

        // Anti-Keygen & Anti-Tamper Scoping Verifications
        juce::String pid = parsed.getProperty("pid", "").toString();
        juce::String pluginId = parsed.getProperty("plugin_id", "").toString();

        if (pid != ProductConfig::productId || pluginId != ProductConfig::pluginIdentifier)
            return false;

        // Verify issuer if present
        juce::String iss = parsed.getProperty("iss", "").toString();
        if (iss.isNotEmpty() && iss != "producertoy.com" && iss != "ProducerToy")
            return false;

        // Verify audience if present
        juce::String aud = parsed.getProperty("aud", "").toString();
        if (aud.isNotEmpty() && aud != "proverb" && aud != "com.producertoy.proverb" && aud != "ProducerToy-Plugins")
            return false;

        // Verify status and revoked flags
        juce::String status = parsed.getProperty("status", "active").toString();
        bool isRevoked = parsed.getProperty("revoked", false);
        if (status != "active" || isRevoked)
            return false;

        // Verify not-before (nbf)
        int64_t nowSec = juce::Time::getCurrentTime().toMilliseconds() / 1000;
        int64_t nbf = static_cast<int64_t>(parsed.getProperty("nbf", 0));
        if (nbf > 0 && nowSec < (nbf - 300)) // 5 minute clock skew allowance
            return false;

        // Verify expiration (exp) and grace_days
        int64_t exp = static_cast<int64_t>(parsed.getProperty("exp", 0));
        int graceDays = static_cast<int>(parsed.getProperty("grace_days", 14));
        if (exp > 0)
        {
            int64_t graceSec = static_cast<int64_t>(graceDays) * 86400;
            if (nowSec > (exp + graceSec))
                return false; // Expired beyond grace period
        }

        // Verify device binding
        juce::String devHash = parsed.getProperty("dev_hash", "").toString();
        if (devHash != getHardwareDeviceHash())
            return false;

        // Populate valid license state
        outState.isActivated = true;
        outState.registeredName = parsed.getProperty("user_name", "Valued Producer").toString();
        outState.maskedEmail = parsed.getProperty("user_email_masked", "user@producertoy.com").toString();
        outState.licenseTier = parsed.getProperty("tier", "Perpetual Commercial").toString();
        outState.graceDays = static_cast<int>(parsed.getProperty("grace_days", 14));
        outState.activationId = parsed.getProperty("aid", "").toString();
        outState.deviceHash = devHash;
        outState.rawToken = token;

        int64_t iat = static_cast<int64_t>(parsed.getProperty("iat", 0));
        outState.issuedAt = juce::Time(iat * 1000);

        return true;
    }

    void LicenseClient::startPairingFlow(
        std::function<void(bool, const juce::String&, const juce::String&)> onCodeReady,
        std::function<void(bool, const juce::String&)> onComplete)
    {
        cancelPairingFlow();
        activeWorker = std::make_unique<PairingWorker>(*this, std::move(onCodeReady), std::move(onComplete));
        activeWorker->startThread(juce::Thread::Priority::normal);
    }

    void LicenseClient::cancelPairingFlow()
    {
        if (activeWorker != nullptr)
        {
            activeWorker->signalThreadShouldExit();
            activeWorker->stopThread(3000);
            activeWorker.reset();
        }
    }

    void LicenseClient::deactivate()
    {
        cancelPairingFlow();

        juce::String devHash = getHardwareDeviceHash();
        juce::String pId = ProductConfig::productId;
        juce::String actId;

        {
            const juce::ScopedLock sl(stateLock);
            actId = currentState.activationId;
            currentState = LicenseState();
            isActivatedAtomic.store(false, std::memory_order_relaxed);
        }

        auto licFile = getLicenseFile();
        if (licFile.existsAsFile())
            licFile.deleteFile();

        // Asynchronously notify server to release device seat
        juce::Thread::launch([devHash, pId, actId]()
        {
            juce::var reqObj = new juce::DynamicObject();
            reqObj.getDynamicObject()->setProperty("productId", pId);
            reqObj.getDynamicObject()->setProperty("deviceHash", devHash);
            if (actId.isNotEmpty())
                reqObj.getDynamicObject()->setProperty("activationId", actId);

            juce::String postJson = juce::JSON::toString(reqObj);
            juce::URL deactUrl(juce::String(ProductConfig::apiBaseUrl) + "/deactivate");
            deactUrl = deactUrl.withPOSTData(postJson);

            int statusCode = 0;
            auto inStream = deactUrl.createInputStream(juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                                                        .withConnectionTimeoutMs(5000)
                                                        .withStatusCode(&statusCode)
                                                        .withExtraHeaders("Content-Type: application/json\r\nAccept: application/json\r\n"));
        });
    }
}
