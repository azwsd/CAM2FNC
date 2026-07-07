#pragma once
#include <string>
#include <unordered_map>

// Database of Steel Projects PROFIL.DAT profiles.
// Maps profile names to FNC codes used in export.
class ProfileDatabase {
public:
    ProfileDatabase() = default;

    // Load from PROFIL.DAT binary.  Returns number of entries loaded.
    int  load(const std::string& path);

    bool isLoaded() const { return m_loaded; }

    // Look up FNC code for a profile name (case-insensitive, * normalised).
    // Returns empty string if not found.
    std::string fncCode(const std::string& profileName) const;

    // Map a PCE_CAT letter (from CAM file) to FNC code — used as fallback.
    static std::string catToFncCode(const std::string& camCat);

private:
    bool m_loaded = false;
    std::unordered_map<std::string, std::string> m_map; // normalised_name → fnc_code

    static std::string normalise(const std::string& s);
    static char        catLetter(char c);
};
