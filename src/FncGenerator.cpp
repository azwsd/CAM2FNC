#include "FncGenerator.h"
#include "CamParser.h"
#include <sstream>
#include <iomanip>
#include <map>
#include <cmath>
#include <cctype>

std::string FncGenerator::fmt(double v) {
    std::ostringstream os;
    os << std::fixed << std::setprecision(2) << v;
    return os.str();
}

std::string FncGenerator::fncProfileCode(const std::string& sectionType) {
    static const std::map<std::string, std::string> m = {
        {"I","I"},{"U","U"},{"L","L"},{"M","Q"},
        {"RO","R"},{"RU","R"},{"B","P"},{"C","C"},{"T","T"}
    };
    auto it = m.find(sectionType);
    return (it != m.end()) ? it->second : "I";
}

std::string FncGenerator::resolveFncCode(const std::string& camCat) {
    if (camCat.empty()) return "I";

    std::string cat = camCat;
    for (auto& c : cat) c = (char)toupper((unsigned char)c);

    if (cat == "RO" || cat == "RU") return "R";
    if (cat == "M") return "Q";

    switch (cat[0]) {
        case 'A': return "U";
        case 'B': return "I";
        case 'C': return "I";
        case 'D': return "L";
        case 'E': return "Q";
        case 'F': return "R";
        case 'H': return "I";
        case 'I': return "I";
        case 'M': return "Q";
        case 'P': return "U";
        case 'R': return "R";
        case 'T': return "T";
        case 'U': return "U";
        case 'Q': return "Q";
        case 'L': return "L";
        default:  return "I";
    }
}

std::string FncGenerator::fncDrillCode(const std::string& dt) {
    if (dt == "Punch") return "TS11";
    if (dt == "Tap")   return "TS41";
    return "TS31";
}

std::string FncGenerator::pcsBlock(const CamPiece& p, const FncSettings& s, bool withHeader) {
    std::string cp  = resolveFncCode(p.camCat);
    std::string mat = s.constraintMaterial.empty() ? p.material : s.constraintMaterial;
    std::string out;

    if (withHeader && s.includePrfHeader) {
        bool omitWl = (p.sectionType == "B"); 
        std::string wlPart = (!omitWl && p.weightPerMeter > 0)
                             ? " WL" + fmt(p.weightPerMeter) : "";
        if (cp == "L") {
            out += "[[PRF]]\n";
            out += "[PRF] CP:" + cp +
                " P:"  + p.profile +
                " SA"  + fmt(p.flangeWidth) +
                " TA"  + fmt(p.flangeThickness) +
                " SB"  + fmt(p.height) +
                " TB"  + fmt(p.webThickness) +
                wlPart + "\n\n";
            out += "[[MAT]]\n";
            out += "[MAT] M:" + mat + "\n\n";
        } else {
            out += "[[PRF]]\n";
            out += "[PRF] CP:" + cp +
                " P:"  + p.profile +
                " SA"  + fmt(p.height) +
                " TA"  + fmt(p.webThickness) +
                " SB"  + fmt(p.flangeWidth) +
                " TB"  + fmt(p.flangeThickness) +
                wlPart + "\n\n";
            out += "[[MAT]]\n";
            out += "[MAT] M:" + mat + "\n\n";
        }
    }

    out += "[[PCS]]\n";
    out += "[HEAD] C:" + p.projectName +
           " D:"  + p.drawingName  +
           " N:"  + p.assemblyName +
           " POS:" + p.positionName + "\n";
    out += "M:" + mat + " CP:" + cp + " P:" + p.profile + "\n";

    if (cp == "P") {                                       
        out += "LP" + fmt(p.length) +
               " SA"  + fmt(p.height) +
               " TA"  + fmt(p.webThickness) + "\n";
    } else if (cp == "R") {                                
        out += "LP" + fmt(p.length) +
               " SA"  + fmt(p.height) +
               " TA"  + fmt(p.webThickness) + "\n";
    } else if (cp == "Q") {                                
        out += "LP" + fmt(p.length) + "\n";
    } else {                                               
        out += "LP" + fmt(p.length) +
               " RAI0.00 RAF0.00 RBI0.00 RBF0.00\n";
    }

    out += "QI" + std::to_string(p.quantity) + "\n";

    // Add automatic marking if enabled
    if (s.enableAutoMark && !s.markFace.empty()) {
        out += "[MARK] " + s.markFace +
               " X" + fmt(s.markX) +
               " Y" + fmt(s.markY) +
               " ANG" + fmt(s.markAngle) +
               " N:" + p.positionName + "\n";
    }

    if (!s.removeHoles) {
        std::string dc = fncDrillCode(s.drillType);
        for (auto& h : p.holes) {
            if (h.isSlotted && s.removeSlots) continue;

            std::string face;
            double faceDim = 0;

            if (cp == "L") {
                if      (h.livNum == 1) { face = "DB"; faceDim = p.flangeWidth; }
                else if (h.livNum == 2) { face = "DA"; faceDim = p.height; }
            } else if (cp == "U" || cp == "C") {
                if      (h.livNum == 1) { face = "DC"; faceDim = p.height; }
                else if (h.livNum == 2) { face = "DA"; faceDim = p.flangeWidth; }
                else if (h.livNum == 3) { face = "DB"; faceDim = p.flangeWidth; }
            } else { 
                if      (h.livNum == 1) { face = "DC"; faceDim = p.height; }
                else if (h.livNum == 2) { face = "DB"; faceDim = p.flangeWidth; }
                else if (h.livNum == 3) { face = "DA"; faceDim = p.flangeWidth; }
                else if (h.livNum == 4) { face = "DD"; faceDim = p.height; }
            }

            if (face.empty()) continue;

            double fncY;
            if (cp == "I" && (h.livNum == 2 || h.livNum == 3)) {
                fncY = faceDim / 2.0 + h.y;
            } else if (cp == "I" && h.livNum == 1){
                fncY = faceDim + h.y;
            } else if (cp == "Q" && (h.livNum == 1 || h.livNum == 2 || h.livNum == 3)) {
                fncY = faceDim + h.y;
            } else {
                fncY = std::abs(h.y);       
            }

            out += "[HOL]   " + dc + "   " +
                   face + fmt(h.diameter) +
                   " X" + fmt(h.x) +
                   " Y" + fmt(fncY) + "\n";
        }
    }

    out += "\n";
    return out;
}

std::string FncGenerator::barBlocks(const CamFile& cam, const FncSettings& s, int& nestCounter) {
    std::map<std::string, const CamPiece*> byPos;
    for (auto& p : cam.pieces) byPos[p.positionName] = &p;

    std::string out;
    for (auto& bar : cam.bars) {
        std::string barCat;
        for (auto& item : bar.items) {
            auto it2 = byPos.find(item.position);
            if (it2 != byPos.end()) { barCat = it2->second->camCat; break; }
        }
        std::string cp = resolveFncCode(barCat);
        
        if (cp == "R") continue; // Skip bars that are pipes

        std::string mat = s.constraintMaterial.empty() ? bar.material : s.constraintMaterial;

        out += "[[BAR]]\n[HEAD]\n";
        out += "N:" + std::to_string(nestCounter) +
               " M:" + mat +
               " CP:" + cp +
               " P:" + bar.profile + "\n";
        out += "LB" + fmt(bar.length) +
               " BI" + std::to_string(bar.quantity) +
               " SP" + fmt(bar.gripStart) +
               " SL" + fmt(bar.sawWidth) +
               " SC" + fmt(bar.gripEnd) + "\n";

        for (auto& item : bar.items) {
            auto it = byPos.find(item.position);
            if (it != byPos.end()) {
                auto* p = it->second;
                out += "[PCS] C:" + p->projectName +
                       " D:"  + p->drawingName  +
                       " N:"  + p->assemblyName +
                       " POS:" + p->positionName +
                       " QT"  + std::to_string(item.quantity) + "\n";
            } else {
                out += "[PCS] C:" + item.project +
                       " D:- N:- POS:" + item.position +
                       " QT" + std::to_string(item.quantity) + "\n";
            }
        }
        out += "\n";
        ++nestCounter;
    }
    return out;
}

std::string FncGenerator::generate(const CamFile& cam, const FncSettings& s,
                                   std::function<void(const std::string&)> onWarning,
                                   int* nestCounterInOut) {
    std::string out;
    out += "; CAM to FNC  |  Source: " + cam.fileName + "\n";
    out += "; Generated by CAM2FNC Converter\n\n";

    std::string lastProfile;
    for (auto& p : cam.pieces) {
        std::string cp = resolveFncCode(p.camCat);
        
        if (cp == "R") {
            if (onWarning) {
                // Trigger the callback with the warning message
                onWarning("Pipes are not supported. Skipping piece '" + p.positionName + "' (Profile: " + p.profile + ").");
            }
            continue;
        }

        bool newHdr = (p.profile != lastProfile);
        out += pcsBlock(p, s, newHdr);
        lastProfile = p.profile;
    }

    if (s.includeBarNests && !cam.bars.empty()) {
        out += "; -- Nesting Plan ------------------------------------------\n\n";
        int nc = nestCounterInOut ? *nestCounterInOut : s.nestCounterStart;
        out += barBlocks(cam, s, nc);
        if (nestCounterInOut) *nestCounterInOut = nc;
    }

    return out;
}