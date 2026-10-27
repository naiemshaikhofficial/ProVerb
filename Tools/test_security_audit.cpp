#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "Licensing/LicenseClient.h"
#include "Licensing/Ed25519Verifier.h"
#include <iostream>
#include <chrono>
#include <vector>

static int gPassed = 0;
static int gFailed = 0;

#define ASSERT_TRUE(condition, testName) \
    do { \
        if (condition) { \
            std::cout << "  [PASS] " << testName << "\n"; \
            gPassed++; \
        } else { \
            std::cerr << "  [FAIL] " << testName << " (Line: " << __LINE__ << ")\n"; \
            gFailed++; \
        } \
    } while (0)

#define ASSERT_FALSE(condition, testName) ASSERT_TRUE(!(condition), testName)

int main()
{
    juce::ScopedJuceInitialiser_GUI guiInit;

    std::cout << "====================================================================\n";
    std::cout << " ProVerb Comprehensive Security, Threat Model & Anti-Tamper Audit\n";
    std::cout << "====================================================================\n\n";

    auto& client = ProducerToy::Licensing::LicenseClient::getInstance();

    // -------------------------------------------------------------------------
    // TEST 1: Hardware Device Fingerprint Formatting & Privacy
    // -------------------------------------------------------------------------
    std::cout << "[TEST GROUP 1] Hardware Device Fingerprint Canonical Properties\n";
    const auto devHash = client.getHardwareDeviceHash();
    std::cout << "  Device Hash: " << devHash << "\n";
    ASSERT_TRUE (devHash.length() == 64, "Device hash length must be exactly 64 characters");
    ASSERT_TRUE (devHash.containsOnly ("0123456789abcdef"), "Device hash must contain only canonical lowercase hex characters");
    
    // Deterministic consistency check
    const auto devHash2 = client.getHardwareDeviceHash();
    ASSERT_TRUE (devHash == devHash2, "Device hash must be strictly deterministic across calls");

    // -------------------------------------------------------------------------
    // TEST 2: Production License Loading & State Invariant Verification
    // -------------------------------------------------------------------------
    std::cout << "\n[TEST GROUP 2] Production License Loading & Entitlement Proof\n";
    client.initialize();
    const bool isLegitActivated = client.isLicenseValid();
    const auto legitState = client.getCurrentState();
    const uint64_t legitProof = client.getEntitlementProof();

    std::cout << "  Installed License Valid: " << (isLegitActivated ? "YES" : "NO") << "\n";
    std::cout << "  Registered Name        : " << legitState.registeredName << "\n";
    std::cout << "  License Tier           : " << legitState.licenseTier << "\n";
    std::cout << "  Entitlement Proof Word : 0x" << juce::String::toHexString ((juce::int64) legitProof) << "\n";

    ASSERT_TRUE (isLegitActivated, "Legitimate customer license must verify successfully");
    ASSERT_TRUE (legitProof != 0ULL, "Cryptographic entitlement proof must be non-zero for active license");
    ASSERT_TRUE (ProducerToy::Licensing::LicenseClient::verifyEntitlementProofStatic (legitProof),
                 "Static entitlement proof validator must authenticate legitimate proof word");
    ASSERT_TRUE (ProducerToy::Licensing::LicenseClient::computeEntitlementMultiplier (legitProof) == 1.0f,
                 "Entitlement multiplier must evaluate to 1.0f for authentic proof word");

    const juce::String authenticToken = legitState.rawToken;
    const int separatorIdx = authenticToken.indexOfChar ('.');
    ASSERT_TRUE (separatorIdx > 0, "Authentic token must contain separator between payload and signature");
    const auto originalPayload = authenticToken.substring (0, separatorIdx);
    const auto originalSig     = authenticToken.substring (separatorIdx + 1);

    // -------------------------------------------------------------------------
    // TEST 3: Cryptographic Signature Mutation & Tamper Resistance
    // -------------------------------------------------------------------------
    std::cout << "\n[TEST GROUP 3] Asymmetric Signature Verification & Tamper Resistance\n";
    {
        // Mutate a single character in the Ed25519 signature
        juce::String tamperedSig = originalSig;
        const int flipPos = tamperedSig.length() / 2;
        tamperedSig = tamperedSig.substring (0, flipPos)
                    + (tamperedSig[flipPos] == 'A' ? 'B' : 'A')
                    + tamperedSig.substring (flipPos + 1);

        const juce::String tamperedToken = originalPayload + "." + tamperedSig;
        ProducerToy::Licensing::LicenseState testState;
        uint64_t testProof = 0ULL;
        const bool verifyRes = client.verifyToken (tamperedToken, testState, &testProof);

        ASSERT_FALSE (verifyRes, "Token with 1-char mutated signature must be cryptographically rejected");
        ASSERT_TRUE (testProof == 0ULL, "Failed token verification must output proof word 0ULL");
    }

    // -------------------------------------------------------------------------
    // TEST 4: Payload Mutation / Entitlement Tampering Resistance
    // -------------------------------------------------------------------------
    std::cout << "\n[TEST GROUP 4] Payload Entitlement Tampering Resistance\n";
    {
        // Mutate a single character in the base64url payload
        juce::String tamperedPayload = originalPayload;
        const int flipPos = tamperedPayload.length() / 2;
        tamperedPayload = tamperedPayload.substring (0, flipPos)
                        + (tamperedPayload[flipPos] == 'x' ? 'y' : 'x')
                        + tamperedPayload.substring (flipPos + 1);

        const juce::String tamperedToken = tamperedPayload + "." + originalSig;
        ProducerToy::Licensing::LicenseState testState;
        uint64_t testProof = 0ULL;
        const bool verifyRes = client.verifyToken (tamperedToken, testState, &testProof);

        ASSERT_FALSE (verifyRes, "Token with modified payload must fail Ed25519 signature verification");
        ASSERT_TRUE (testProof == 0ULL, "Rejected tampered payload must output 0ULL proof word");
    }

    // -------------------------------------------------------------------------
    // TEST 5: Malformed, Truncated & Oversized Token Fuzzing
    // -------------------------------------------------------------------------
    std::cout << "\n[TEST GROUP 5] Malformed & Oversized Token Rejection (No Crash/Throw)\n";
    {
        ProducerToy::Licensing::LicenseState dummy;
        uint64_t proof = 0ULL;

        ASSERT_FALSE (client.verifyToken ("", dummy, &proof), "Empty token rejected");
        ASSERT_FALSE (client.verifyToken ("not_a_valid_token", dummy, &proof), "Missing dot separator rejected");
        ASSERT_FALSE (client.verifyToken ("two.dots.here", dummy, &proof), "Multiple dot separators rejected");
        ASSERT_FALSE (client.verifyToken (".signature_only", dummy, &proof), "Missing payload rejected");
        ASSERT_FALSE (client.verifyToken ("payload_only.", dummy, &proof), "Missing signature rejected");
        ASSERT_FALSE (client.verifyToken ("invalid$$chars!.sig", dummy, &proof), "Non-base64url characters rejected");

        // Oversized token (>8192 bytes)
        juce::String oversizedToken (juce::String::repeatedString ("A", 8200) + "." + originalSig);
        ASSERT_FALSE (client.verifyToken (oversizedToken, dummy, &proof), "Oversized token rejected safely");
    }

    // -------------------------------------------------------------------------
    // TEST 6: Simulated 1-Byte Boolean Patch Resistance
    // -------------------------------------------------------------------------
    std::cout << "\n[TEST GROUP 6] Simulated 1-Byte Boolean Patch & Memory Tamper Resistance\n";
    {
        // Test 6a: Proof word 0ULL (uninitialized / forged)
        ASSERT_FALSE (ProducerToy::Licensing::LicenseClient::verifyEntitlementProofStatic (0ULL),
                      "Proof word 0ULL must be rejected by static proof validator");
        ASSERT_TRUE (ProducerToy::Licensing::LicenseClient::computeEntitlementMultiplier (0ULL) == 0.0f,
                     "Multiplier for 0ULL proof must be exactly 0.0f");

        // Test 6b: Typical cracker bool patch: setting variable to 1
        ASSERT_FALSE (ProducerToy::Licensing::LicenseClient::verifyEntitlementProofStatic (1ULL),
                      "Cracker boolean patch word 1ULL must fail CRC invariant verification");
        ASSERT_TRUE (ProducerToy::Licensing::LicenseClient::computeEntitlementMultiplier (1ULL) == 0.0f,
                     "Multiplier for cracker patched 1ULL must be 0.0f (wet muted)");

        // Test 6c: All bits set 0xFFFFFFFFFFFFFFFF
        ASSERT_FALSE (ProducerToy::Licensing::LicenseClient::verifyEntitlementProofStatic (0xFFFFFFFFFFFFFFFFULL),
                      "Word 0xFFFFFFFFFFFFFFFFULL must fail verification");
        ASSERT_TRUE (ProducerToy::Licensing::LicenseClient::computeEntitlementMultiplier (0xFFFFFFFFFFFFFFFFULL) == 0.0f,
                     "Multiplier for all-ones word must be 0.0f");

        // Test 6d: Simulated DSP wet path calculation with patched boolean but unverified proof
        const bool simulatedPatchedBool = true; // Cracker patched isLicensedAtomic to true!
        const uint64_t unverifiedProof   = 0ULL; // But cracker cannot forge the 64-bit cryptographic digest
        const bool isProofValid = ProducerToy::Licensing::LicenseClient::verifyEntitlementProofStatic (unverifiedProof);
        const float multiplier  = ProducerToy::Licensing::LicenseClient::computeEntitlementMultiplier (unverifiedProof);
        const bool isUnlocked   = simulatedPatchedBool && isProofValid;
        const float wetPct      = 100.0f;
        const float effectiveWet = isUnlocked ? ((wetPct / 100.0f) * multiplier) : 0.0f;

        std::cout << "  Simulated Patched Bool   : " << (simulatedPatchedBool ? "TRUE" : "FALSE") << "\n";
        std::cout << "  Cryptographic Proof Valid: " << (isProofValid ? "TRUE" : "FALSE") << "\n";
        std::cout << "  Entitlement Multiplier   : " << multiplier << "\n";
        std::cout << "  Resulting Effective Wet  : " << effectiveWet << "\n";

        ASSERT_FALSE (isUnlocked, "Patched boolean without proof MUST remain locked (isUnlocked = false)");
        ASSERT_TRUE (effectiveWet == 0.0f, "Patched boolean without proof MUST produce 0.0f wet audio (complete silence)");
    }

    // -------------------------------------------------------------------------
    // TEST 7: Audio Thread Real-Time Performance & Lock-Free Safety
    // -------------------------------------------------------------------------
    std::cout << "\n[TEST GROUP 7] Real-Time Audio Thread Performance & Zero-Lock Safety\n";
    {
        constexpr int iterations = 100000;
        const auto start = std::chrono::high_resolution_clock::now();

        float accum = 0.0f;
        for (int i = 0; i < iterations; ++i)
        {
            const bool valid = ProducerToy::Licensing::LicenseClient::verifyEntitlementProofStatic (legitProof);
            const float mult = ProducerToy::Licensing::LicenseClient::computeEntitlementMultiplier (legitProof);
            accum += (valid ? mult : 0.0f);
        }

        const auto end = std::chrono::high_resolution_clock::now();
        const auto elapsedNs = std::chrono::duration_cast<std::chrono::nanoseconds> (end - start).count();
        const double nsPerCall = static_cast<double> (elapsedNs) / static_cast<double> (iterations);

        std::cout << "  Executed " << iterations << " entitlement validations in " << (elapsedNs / 1000000.0) << " ms\n";
        std::cout << "  Average time per audio check: " << nsPerCall << " ns\n";

        ASSERT_TRUE (accum == static_cast<float> (iterations), "All benchmark checks must evaluate to 1.0f");
        ASSERT_TRUE (nsPerCall < 100.0, "Audio thread entitlement check must execute in < 100 nanoseconds (real-time safe)");
    }

    // -------------------------------------------------------------------------
    // TEST 8: Local Deactivation & Reactivation State Machine
    // -------------------------------------------------------------------------
    std::cout << "\n[TEST GROUP 8] State Machine Transitions (Deactivate & Re-activate)\n";
    {
        client.deactivate();
        ASSERT_FALSE (client.isLicenseValid(), "License must be invalid immediately after deactivation");
        ASSERT_TRUE (client.getEntitlementProof() == 0ULL, "Proof word must reset to 0ULL upon deactivation");
        ASSERT_FALSE (ProducerToy::Licensing::LicenseClient::getLicenseFile().existsAsFile(),
                      "License file must be removed from disk upon deactivation");

        // Reactivate with legitimate token
        juce::String installErr;
        const bool reinstallOk = client.installLicenseToken (authenticToken, installErr);
        ASSERT_TRUE (reinstallOk, "Re-installation of authentic token must succeed");
        ASSERT_TRUE (client.isLicenseValid(), "License must be active after re-installation");
        ASSERT_TRUE (client.getEntitlementProof() != 0ULL, "Proof word must be restored upon re-installation");
        ASSERT_TRUE (ProducerToy::Licensing::LicenseClient::verifyEntitlementProofStatic (client.getEntitlementProof()),
                     "Restored proof word must pass static validation");
    }

    std::cout << "\n====================================================================\n";
    std::cout << " AUDIT SUMMARY: " << gPassed << " PASSED, " << gFailed << " FAILED\n";
    std::cout << "====================================================================\n";

    return (gFailed == 0) ? 0 : 1;
}
