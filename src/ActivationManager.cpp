#include "ActivationManager.h"

#include <wx/config.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <lmcons.h>
#else
#include <pwd.h>
#include <unistd.h>
#endif

namespace {

constexpr uint8_t kXorByte = 0x6Du;

constexpr std::array<uint8_t, 12> kMaterialEnc = {
    0xCE, 0x9A, 0xAF, 0x8C,
    0x30, 0xF6, 0x22, 0xEE,
    0x8B, 0x47, 0x70, 0x1D,
};

constexpr std::array<uint8_t, 32> kAlphaEnc = {
    0x0E, 0x0F, 0x08, 0x09, 0x0A, 0x0B, 0x04, 0x05,
    0x7D, 0x7E, 0x7F, 0x78, 0x79, 0x7A, 0x7B, 0x74,
    0x76, 0x77, 0x70, 0x71, 0x72, 0x6C, 0x6D, 0x6E,
    0x6F, 0x68, 0x69, 0x6A, 0x6B, 0x64, 0x65, 0x66,
};

constexpr std::array<uint8_t, 14> kStoreKeyEnc = {
    0x76, 0x54, 0x43, 0x5E, 0x41, 0x56, 0x43, 0x5E,
    0x58, 0x59, 0x7C, 0x52, 0x4E, 0x00,
};

uint32_t loadMaterial(size_t index) {
    const size_t off = index * 4;
    uint32_t v = 0;
    for (int i = 0; i < 4; ++i)
        v = (v << 8) | (kMaterialEnc[off + static_cast<size_t>(i)] ^ kXorByte);
    return v;
}

const char* alphabet() {
    static char table[33];
    static bool ready = false;
    if (!ready) {
        for (size_t i = 0; i < 32; ++i)
            table[i] = static_cast<char>(kAlphaEnc[i] ^ 0x3Cu);
        table[32] = '\0';
        ready = true;
    }
    return table;
}

std::string storageKey() {
    std::string key;
    key.reserve(12);
    for (uint8_t b : kStoreKeyEnc) {
        if (b == 0)
            break;
        key.push_back(static_cast<char>(b ^ 0x37u));
    }
    return key;
}

uint32_t mixRound(uint32_t h, uint32_t m, int rshift, int lshift) {
    h ^= m;
    h = (h << lshift) | (h >> rshift);
    return h;
}

} // namespace

uint32_t ActivationManager::hashString(const std::string& s) {
    uint32_t h = 2166136261u;
    for (unsigned char c : s) {
        h ^= c;
        h *= 16777619u;
    }
    return h;
}

uint32_t ActivationManager::computeToken(const std::string& fingerprint) {
    const uint32_t m0 = loadMaterial(0);
    const uint32_t m1 = loadMaterial(1);
    const uint32_t m2 = loadMaterial(2);

    uint32_t h = hashString(fingerprint);
    h = mixRound(h, m0, 19, 13);
    h *= m1;
    h ^= m2;
    h = mixRound(h, 0, 25, 7);
    h *= m0;
    h ^= (h >> 16);
    return h;
}

uint32_t ActivationManager::decodeKey(const std::string& key) {
    std::string clean;
    clean.reserve(key.size());
    for (char c : key) {
        if (c != '-')
            clean += static_cast<char>(toupper(static_cast<unsigned char>(c)));
    }
    if (clean.size() != 8)
        return 0;

    const char* alpha = alphabet();
    uint64_t full = 0;
    for (char c : clean) {
        const char* p = strchr(alpha, c);
        if (!p)
            return 0;
        full = (full << 5) | static_cast<uint64_t>(p - alpha);
    }

    const uint32_t token = static_cast<uint32_t>(full >> 8);
    const uint8_t ck = static_cast<uint8_t>(full & 0xFF);
    const uint8_t expected = static_cast<uint8_t>(
        (token ^ (token >> 8) ^ (token >> 16) ^ (token >> 24)) & 0xFF);
    if (ck != expected)
        return 0;

    return token;
}

std::string ActivationManager::getMachineFingerprint() {
#ifdef _WIN32
    DWORD serial = 0;
    GetVolumeInformationA("C:\\", nullptr, 0, &serial, nullptr, nullptr, nullptr, 0);

    char username[UNLEN + 1] = {};
    DWORD ulen = UNLEN + 1;
    GetUserNameA(username, &ulen);

    std::ostringstream ss;
    ss << std::hex << std::uppercase << serial << "_" << username;
    return ss.str();
#else
    char host[256] = {};
    gethostname(host, sizeof(host));
    struct passwd* pw = getpwuid(getuid());
    std::string user = pw ? pw->pw_name : "user";
    return std::string(host) + "_" + user;
#endif
}

bool ActivationManager::validateKey(const std::string& key) {
    const uint32_t decoded = decodeKey(key);
    if (decoded == 0)
        return false;
    return decoded == computeToken(getMachineFingerprint());
}

bool ActivationManager::isActivated() {
    wxConfig cfg("CAM2FNC");
    wxString saved;
    if (!cfg.Read(storageKey(), &saved) || saved.empty())
        return false;
    return validateKey(saved.ToStdString());
}

void ActivationManager::saveKey(const std::string& key) {
    wxConfig cfg("CAM2FNC");
    cfg.Write(storageKey(), wxString(key));
}

bool ActivationManager::verifyRuntime() {
    wxConfig cfg("CAM2FNC");
    wxString saved;
    if (!cfg.Read(storageKey(), &saved) || saved.empty())
        return false;

    const uint32_t token = decodeKey(saved.ToStdString());
    if (token == 0)
        return false;

    return token == computeToken(getMachineFingerprint());
}
