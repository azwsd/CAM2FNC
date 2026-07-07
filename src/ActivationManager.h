#pragma once
#include <string>
#include <cstdint>

class ActivationManager {
public:
    static std::string getMachineFingerprint();
    static bool validateKey(const std::string& key);
    static bool isActivated();
    static void saveKey(const std::string& key);

    // Secondary gate used from export paths (not named "activation" in the binary).
    static bool verifyRuntime();

private:
    static uint32_t hashString(const std::string& s);
    static uint32_t computeToken(const std::string& fingerprint);
    static uint32_t decodeKey(const std::string& key);
};
