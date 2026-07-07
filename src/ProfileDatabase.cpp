#include "ProfileDatabase.h"
#include <fstream>
#include <regex>
#include <algorithm>
#include <cctype>

// ── static helpers ────────────────────────────────────────────────

// Normalise name: uppercase, replace * with X, remove spaces.
std::string ProfileDatabase::normalise(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '*')       out += 'X';
        else if (c == ' ')  continue;
        else                out += (char)toupper((unsigned char)c);
    }
    return out;
}

// Turn a category letter into an FNC code.
char ProfileDatabase::catLetter(char c) {
    switch ((char)toupper((unsigned char)c)) {
        case 'A': return 'U';   // UPN, RSC channels
        case 'B': return 'I';   // IPN I-beams
        case 'C': return 'I';   // IPE, W, M, S I-beams
        case 'D': return 'L';   // L angles
        case 'E': return 'M';   // TS rectangular/square hollow
        case 'F': return 'R';   // P pipe / round hollow
        case 'H': return 'I';   // HEA/HEB/HEM (EA/EB/EM) I-beams
        case 'I': return 'I';   // IPN/IPEA variants
        case 'M': return 'U';   // C American channels
        case 'P': return 'U';   // FC lipped channels
        case 'R': return 'U';   // RSC/SC channels
        case 'T': return 'U';   // PFC, UAP channels
        case 'U': return 'U';   // UPN, B universal beams
        default:  return 0;
    }
}

// Load settings from a PCE_CAT letter.
std::string ProfileDatabase::catToFncCode(const std::string& camCat) {
    if (camCat.empty()) return "I";
    char fnc = catLetter(camCat[0]);
    if (fnc) return std::string(1, fnc);
    // Extended PCE_CAT values not in PROFIL.DAT letter set
    char c = (char)toupper((unsigned char)camCat[0]);
    switch (c) {
        case 'K': return "R";   // round hollow alternative
        case 'N': return "L";   // angle alternative
        case 'Q': return "M";   // square/rect hollow alternative
        case 'S': return "I";   // special I-beam
        default:  return "I";
    }
}

// Load profiles from a PROFIL.DAT binary file.
int ProfileDatabase::load(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return 0;

    std::string data((std::istreambuf_iterator<char>(f)),
                      std::istreambuf_iterator<char>());

    // Extract all ASCII printable strings >= 4 chars from binary blob
    // A valid profile entry matches:  [A-U][A-Za-z0-9.*_\-+/]+
    // The first char is the category letter; the rest is the profile name.
    static const std::regex re(
        R"([A-U][A-Za-z][A-Za-z0-9.*_\-+/]{2,})",
        std::regex::optimize);

    int count = 0;
    auto begin = std::sregex_iterator(data.begin(), data.end(), re);
    auto end   = std::sregex_iterator();

    for (auto it = begin; it != end; ++it) {
        std::string entry = it->str();
        char cat  = entry[0];
        char fnc  = catLetter(cat);
        if (!fnc) continue;                      // unknown category

        std::string name = entry.substr(1);      // strip category prefix
        if (!std::any_of(name.begin(), name.end(),
                         [](char c){ return isdigit((unsigned char)c); }))
            continue;                            // must contain a digit

        std::string key = normalise(name);
        if (m_map.find(key) == m_map.end()) {    // first match wins
            m_map[key] = std::string(1, fnc);
            ++count;
        }
    }

    m_loaded = (count > 0);
    return count;
}

// Look up the FNC code for a profile name.
std::string ProfileDatabase::fncCode(const std::string& profileName) const {
    if (!m_loaded) return "";
    std::string key = normalise(profileName);
    auto it = m_map.find(key);
    if (it != m_map.end()) return it->second;
    return "";
}
