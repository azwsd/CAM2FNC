#pragma once
#include <string>
#include <vector>
#include <set>
#include <ctime>

// Generates a nest number based on the current date and time.
// Format: MMDDHH001 (month, day, hour, sequence starting at 1)
inline int GetDefaultNestNumber() {
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    // Format: MMDDHH001 (month, day, hour, sequence starting at 1)
    return (t->tm_mon + 1) * 10000000 +      // month * 10,000,000
           t->tm_mday * 100000 +              // day * 100,000
           t->tm_hour * 1000 +                // hour * 1,000
           1;                                 // start at 001
}

// A single hole from a [HOLE] block inside a piece.
// LIV number 1-4 maps to different faces in the FNC output.
struct CamHole {
    int         livNum  = 0;   // 1-4 as in CAM file
    std::string fncFace;       // DA/DB/DC — resolved in FncGenerator per profile type
    char        refType = 'M'; // H=top  M=centreline  B=bottom
    double diameter = 0;
    double x        = 0;
    double y        = 0;
    bool   isSlotted= false;
};

// A single piece from a _PIECE_ block.
struct CamPiece {
    std::string projectName;
    std::string drawingName;
    std::string assemblyName;
    std::string positionName;
    std::string material;
    std::string profile;
    std::string camCat;       // PCE_CAT value straight from CAM file (B/C/T/R/D/U/I/A…)
    std::string sectionType;  // derived: I U L M RO RU B C T

    int    quantity         = 1;
    double length           = 0;
    double height           = 0;
    double flangeWidth      = 0;
    double flangeThickness  = 0;
    double webThickness     = 0;
    double radius           = 0;
    double webStartCut      = 0;
    double webEndCut        = 0;
    double flangeStartCut   = 0;
    double flangeEndCut     = 0;
    double weightPerMeter   = 0;

    std::vector<CamHole> holes;
};

// One item inside a bar (from an [ITEM] line).
struct CamBarItem {
    std::string project;
    std::string position;
    double length   = 0;
    int    quantity = 1;
};

// A single bar from a _BAR_ block.
struct CamBar {
    std::string profile;
    std::string material;
    double length      = 0;
    double weightTotal = 0;
    double gripStart   = 0;   // BAR_SC
    double gripEnd     = 0;   // BAR_SP
    double sawWidth    = 0;   // BAR_SL
    int    quantity    = 1;

    std::vector<CamBarItem> items;
};

// A stock piece from a _STOCK_ block.
// Kept as raw data, not exported to FNC.
struct CamStock {
    std::string profile;
    std::string material;
    double length   = 0;
    double weight   = 0;
    double surface  = 0;
    int    quantity = 0;
    int    remnant  = 0;
};

// A fully parsed CAM file with all pieces, bars, and stock.
struct CamFile {
    std::string filePath;
    std::string fileName;

    std::vector<CamPiece>  pieces;
    std::vector<CamBar>    bars;
    std::vector<CamStock>  stocks;

    std::string projectName() const {
        return pieces.empty() ? "" : pieces.front().projectName;
    }
    std::string profileSummary() const {
        std::set<std::string> uniqueProfiles;

        // Collect profiles from all pieces
        for (const auto& p : pieces) {
            if (!p.profile.empty()) uniqueProfiles.insert(p.profile);
        }

        // Collect profiles from all bars (nesting results)
        for (const auto& b : bars) {
            if (!b.profile.empty()) uniqueProfiles.insert(b.profile);
        }

        if (uniqueProfiles.empty()) return "";

        // Join the unique profiles with a comma and space
        std::string result;
        for (auto it = uniqueProfiles.begin(); it != uniqueProfiles.end(); ++it) {
            if (it != uniqueProfiles.begin()) {
                result += ", ";
            }
            result += *it;
        }
        return result;
    }
    int totalHoles() const {
        int n = 0;
        for (auto& p : pieces) n += (int)p.holes.size();
        return n;
    }
};

// All settings for exporting to FNC format.
// These get saved between runs.
struct FncSettings {
    std::string drillType          = "Drill";   // Drill | Punch | Tap
    bool        removeHoles        = false;
    bool        removeSlots        = true;
    bool        includePrfHeader   = true;
    bool        includeBarNests    = true;
    bool        exportByProfile    = false;
    bool        combinedOutput     = false;
    std::string constraintMaterial;
    int         nestCounterStart   = GetDefaultNestNumber();

    // Automatic marking settings
    bool        enableAutoMark     = false;
    std::string markFace;        // Face for marking (DA, DB, DC, etc.)
    double      markX             = 0;   // X coordinate
    double      markY             = 0;   // Y coordinate
    double      markAngle         = 0;   // Angle
};
