#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include "Licensing/LicenseClient.h"
#include <iostream>

int main()
{
    juce::ScopedJuceInitialiser_GUI guiInit;

    std::cout << "========================================================\n";
    std::cout << " ProVerb C++ LicenseClient Production Verification\n";
    std::cout << "========================================================\n\n";

    auto& client = ProducerToy::Licensing::LicenseClient::getInstance();

    // 1. Hardware Device Hash format check
    const auto devHash = client.getHardwareDeviceHash();
    std::cout << "[1/6] Device Hash canonical check:\n";
    std::cout << "      Hash: " << devHash << "\n";
    std::cout << "      Length: " << devHash.length() << " (Expected: 64)\n";
    if (devHash.length() != 64)
    {
        std::cerr << "FAILED: Device hash must be 64 lowercase hex characters.\n";
        return 1;
    }
    std::cout << "      Status: OK\n\n";

    // 2. Initialize from APPDATA license file
    std::cout << "[2/6] Initializing from installed %APPDATA% license file...\n";
    auto licFile = ProducerToy::Licensing::LicenseClient::getLicenseFile();
    std::cout << "      File path: " << licFile.getFullPathName() << "\n";
    std::cout << "      Exists: " << (licFile.existsAsFile() ? "YES" : "NO") << "\n";
    std::cout << "      Size: " << licFile.getSize() << " bytes\n";
    auto token = licFile.loadFileAsString().trim();
    std::cout << "      Token length: " << token.length() << "\n";
    ProducerToy::Licensing::LicenseState debugState;
    bool debugVerify = client.verifyToken(token, debugState);
    std::cout << "      Direct verifyToken: " << (debugVerify ? "TRUE" : "FALSE") << "\n";

    client.initialize();
    const bool valid = client.isLicenseValid();
    const auto state = client.getCurrentState();
    std::cout << "      isLicenseValid: " << (valid ? "TRUE" : "FALSE") << "\n";
    std::cout << "      Registered Name: " << state.registeredName << "\n";
    std::cout << "      Masked Email: " << state.maskedEmail << "\n";
    std::cout << "      Tier: " << state.licenseTier << "\n";
    std::cout << "      Activation ID: " << state.activationId << "\n";

    if (!valid)
    {
        std::cerr << "FAILED: Expected installed license to be valid.\n";
        return 2;
    }
    std::cout << "      Status: OK\n\n";

    // 3. Test Invalid Token Rejection
    std::cout << "[3/6] Testing rejection of malformed / forged license tokens...\n";
    juce::String err;
    bool badRes = client.installLicenseToken("invalid.token.here", err);
    if (badRes)
    {
        std::cerr << "FAILED: Malformed token was accepted unexpectedly!\n";
        return 3;
    }
    std::cout << "      Malformed token correctly rejected with error: " << err << "\n";
    std::cout << "      Status: OK\n\n";

    // 4. Test Local Deactivation
    std::cout << "[4/6] Testing local deactivation & DSP lock state transition...\n";
    client.deactivate();
    if (client.isLicenseValid())
    {
        std::cerr << "FAILED: License remained valid after deactivation!\n";
        return 4;
    }
    auto licenseFile = ProducerToy::Licensing::LicenseClient::getLicenseFile();
    if (licenseFile.existsAsFile())
    {
        std::cerr << "FAILED: License file still exists after deactivation!\n";
        return 4;
    }
    std::cout << "      License file successfully removed, isLicenseValid is now FALSE.\n";
    std::cout << "      Status: OK\n\n";

    // 5. Test Re-installation with Valid Token
    std::cout << "[5/6] Testing re-installation of valid license token...\n";
    if (!client.installLicenseToken(state.rawToken, err))
    {
        std::cerr << "FAILED: Re-installation of valid token failed: " << err << "\n";
        return 5;
    }
    if (!client.isLicenseValid())
    {
        std::cerr << "FAILED: License not valid after re-installation!\n";
        return 5;
    }
    std::cout << "      Re-installed successfully! isLicenseValid is TRUE.\n";
    std::cout << "      Status: OK\n\n";

    // 6. Effective API URL check
    std::cout << "[6/6] Verifying dedicated licensing API endpoint configuration...\n";
    const auto apiUrl = client.getEffectiveApiUrl();
    std::cout << "      Effective API URL: " << apiUrl << "\n";
    if (apiUrl.containsIgnoreCase("vercel") || apiUrl.containsIgnoreCase("supabase"))
    {
        std::cerr << "FAILED: Licensing API URL must NOT point to Vercel or Supabase!\n";
        return 6;
    }
    std::cout << "      Zero Vercel/Supabase check: PASSED (Non-Vercel/Non-Supabase)\n\n";

    std::cout << "========================================================\n";
    std::cout << " ALL C++ LICENSING CHECKS PASSED SUCCESSFULLY!\n";
    std::cout << "========================================================\n";
    return 0;
}
