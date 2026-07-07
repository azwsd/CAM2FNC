#include "CamParser.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <map>

// Text helper functions for parsing CAM files.
std::string CamParser::trim(const std::string& s) {
    auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return {};
    auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

std::string CamParser::afterColon(const std::string& s) {
    auto p = s.find(':');
    return (p == std::string::npos) ? "" : trim(s.substr(p + 1));
}

std::vector<std::string> CamParser::splitWords(const std::string& s) {
    std::istringstream ss(s);
    std::string w;
    std::vector<std::string> v;
    while (ss >> w) v.push_back(w);
    return v;
}

bool CamParser::startsWith(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() &&
           s.compare(0, prefix.size(), prefix) == 0;
}

// Normalise profile and material strings for comparison.
std::string CamParser::normaliseProfile(const std::string& raw) {
    std::string s = trim(raw);
    for (auto& c : s) { c = (char)toupper((unsigned char)c); if (c == '*') c = 'X'; }
    s.erase(std::remove(s.begin(), s.end(), ' '), s.end());
    return s;
}

std::string CamParser::normaliseMaterial(const std::string& raw) {
    std::string s = trim(raw);
    for (auto& c : s) c = (char)toupper((unsigned char)c);
    s.erase(std::remove(s.begin(), s.end(), ' '), s.end());
    return s;
}

// When PCE_CAT is missing, guess the section type from the profile name.
std::string CamParser::detectSectionType(const std::string& profile) {
    // Fallback: guess from profile name (used only when PCE_CAT not available)
    std::string n = normaliseProfile(profile);
    if (startsWith(n,"CHS")||startsWith(n,"RO")||startsWith(n,"PIPE")||
        startsWith(n,"TUB")||startsWith(n,"CIR"))                       return "RO";
    if (startsWith(n,"RHS")||startsWith(n,"SHS")||startsWith(n,"HSQ")||
        startsWith(n,"KRQST")||startsWith(n,"MSH"))                     return "C";
    if (startsWith(n,"UPN")||startsWith(n,"UPE")||startsWith(n,"PFC")||
        startsWith(n,"UAP")||startsWith(n,"UNP")||startsWith(n,"MC"))   return "U";
    if (startsWith(n,"EA")||startsWith(n,"RSA")||startsWith(n,"ISA"))   return "L";
    if (n.size()>1 && n[0]=='L' && isdigit((unsigned char)n[1]))        return "L";
    if (startsWith(n,"FLAT")||startsWith(n,"PLT")||startsWith(n,"FB")||
        startsWith(n,"BP")||startsWith(n,"PL"))                         return "B";
    if (startsWith(n,"TEE")||startsWith(n,"T/")||startsWith(n,"WT"))    return "T";
    return "I";
}

// Map PCE_CAT letter → sectionType
static std::string catToSectionType(const std::string& cat) {
    if (cat.empty()) return "I";
    char c = (char)toupper((unsigned char)cat[0]);
    switch (c) {
        case 'B': return "I";   // standard I-beam
        case 'C': return "I";   // welded/plate girder (I-shaped)
        case 'I': return "I";   // I-beam
        case 'T': return "U";   // C-channel / rectangular tube
        case 'U': return "U";   // UPN/channel
        case 'D': return "L";   // angle
        case 'A': return "L";   // angle variant
        case 'R': return "RO";  // round hollow
        case 'Q': return "C";   // square/rectangular hollow
        case 'P': return "B";   // plate
        default:  return "I";
    }
}

// Parse the [SHAPE] block — rows of numbers that describe the cross-section.
void CamParser::parseShape(const Lines& lines, int start, int end, CamPiece& p) {
    std::vector<std::vector<double>> rows;
    for (int i = start + 1; i < end; ++i) {
        std::string ln = trim(lines[i]);
        if (ln.empty() || ln[0] == ';') continue;
        // Skip the shape-type header like "A 4 0"
        if (!ln.empty() && isalpha((unsigned char)ln[0])) continue;

        auto words = splitWords(ln);
        std::vector<double> row;
        for (auto& w : words) {
            try { row.push_back(std::stod(w)); } catch (...) {}
        }
        if (!row.empty()) rows.push_back(row);
    }

    if (rows.size() >= 1 && rows[0].size() >= 5) {
        p.height          = rows[0][0];
        p.flangeWidth     = rows[0][1];
        // [2] unused
        p.webThickness    = rows[0][3];
        p.flangeThickness = rows[0][4];
        if (rows[0].size() >= 6) p.radius = rows[0][5];
    }
    // Cuts (RAI/RAF/RBI/RBF) are always output as 0 per spec — not parsed
}

// Parse the [HOLE] block for a single piece.
// LIV1 maps to DB (web), LIV2 to DA (top flange), LIV3 to DC (bottom flange), LIV4 to DD (back web)
static std::string livToFace(int liv) {
    switch (liv) {
        case 1: return "DB";
        case 2: return "DA";
        case 3: return "DC";
        case 4: return "DD";
    }
    return "DB";
}

void CamParser::parseHoles(const Lines& lines, int start, int end, CamPiece& p) {
    int currentLiv = 1;
    for (int i = start + 1; i < end; ++i) {
        std::string ln = trim(lines[i]);
        if (ln.empty()) continue;

        // LIV marker
        if (ln.size() >= 3 &&
            toupper((unsigned char)ln[0])=='L' &&
            toupper((unsigned char)ln[1])=='I' &&
            toupper((unsigned char)ln[2])=='V') {
            try { currentLiv = std::stoi(trim(ln.substr(3))); } catch (...) {}
            continue;
        }

        char first = (char)toupper((unsigned char)ln[0]);
        if (first != 'H' && first != 'M' && first != 'B') continue;

        auto parts = splitWords(ln);
        if (parts.size() < 5) continue;

        CamHole h;
        h.livNum   = currentLiv;
        h.refType  = first;
        h.diameter = std::stod(parts[1]);
        h.x        = std::stod(parts[2]);

        bool isWeb = (currentLiv == 1 || currentLiv == 4);
        h.y = isWeb
            ? std::stod(parts[3])
            : (parts.size() >= 5 ? std::stod(parts[4]) : 0.0);

        h.isSlotted = (!parts.empty() && parts.back() == "4");
        p.holes.push_back(h);
    }
}

// Parse a _PIECE_ block from the CAM file.
CamPiece CamParser::parsePiece(const Lines& lines, int start, int end) {
    CamPiece p;
    int i = start;
    while (i < end) {
        std::string ln = trim(lines[i]);

        if (ln == "[HEAD]") {
            ++i;
            while (i < end) {
                ln = trim(lines[i]);
                if (!ln.empty() && ln[0] == '[') break;
                if (startsWith(ln,"COM_NAM:"))  p.projectName  = afterColon(ln);
                else if (startsWith(ln,"DWG_NAM:")) p.drawingName  = afterColon(ln);
                else if (startsWith(ln,"ASS_NAM:")) p.assemblyName = afterColon(ln);
                else if (startsWith(ln,"PCE_NAM:")) p.positionName = afterColon(ln);
                else if (startsWith(ln,"PCE_QTY:")) try { p.quantity = std::stoi(afterColon(ln)); } catch (...) {}
                else if (startsWith(ln,"PCE_LEN:")) try { p.length   = std::stod(afterColon(ln)); } catch (...) {}
                else if (startsWith(ln,"PCE_MAT:")) p.material = normaliseMaterial(afterColon(ln));
                else if (startsWith(ln,"PCE_PRF:")) p.profile  = normaliseProfile(afterColon(ln));
                else if (startsWith(ln,"PCE_CAT:")) p.camCat   = trim(afterColon(ln));
                ++i;
            }
            continue;
        }

        if (ln == "[SHAPE]") {
            int sEnd = i + 1;
            while (sEnd < end && trim(lines[sEnd])[0] != '[') ++sEnd;
            parseShape(lines, i, sEnd, p);
            i = sEnd;
            continue;
        }

        if (ln == "[HOLE]") {
            int hEnd = i + 1;
            while (hEnd < end) {
                std::string t = trim(lines[hEnd]);
                if (!t.empty() && t[0] == '[') break;
                ++hEnd;
            }
            parseHoles(lines, i, hEnd, p);
            i = hEnd;
            continue;
        }

        ++i;
    }

    // Prefer PCE_CAT for section type; fall back to profile-name guessing
    if (!p.camCat.empty())
        p.sectionType = catToSectionType(p.camCat);
    else
        p.sectionType = detectSectionType(p.profile);
    return p;
}

// Parse a _BAR_ block from the CAM file.
CamBar CamParser::parseBar(const Lines& lines, int start, int end) {
    CamBar bar;
    bool inItem = false;
    for (int i = start; i < end; ++i) {
        std::string ln = trim(lines[i]);
        if (ln == "[HEAD]") { inItem = false; continue; }
        if (ln == "[ITEM]") { inItem = true;  continue; }
        if (!ln.empty() && ln[0] == '[') { inItem = false; continue; }

        if (!inItem) {
            if      (startsWith(ln,"BAR_PRO:")) bar.profile     = normaliseProfile(afterColon(ln));
            else if (startsWith(ln,"BAR_MAT:")) bar.material    = normaliseMaterial(afterColon(ln));
            else if (startsWith(ln,"BAR_LEN:")) try { bar.length      = std::stod(afterColon(ln)); } catch (...) {}
            else if (startsWith(ln,"BAR_QTY:")) try { bar.quantity    = std::stoi(afterColon(ln)); } catch (...) {}
            else if (startsWith(ln,"BAR_WGH:")) try { bar.weightTotal = std::stod(afterColon(ln)); } catch (...) {}
            else if (startsWith(ln,"BAR_SC:"))  try { bar.gripStart   = std::stod(afterColon(ln)); } catch (...) {}
            else if (startsWith(ln,"BAR_SP:"))  try { bar.gripEnd     = std::stod(afterColon(ln)); } catch (...) {}
            else if (startsWith(ln,"BAR_SL:"))  try { bar.sawWidth    = std::stod(afterColon(ln)); } catch (...) {}
        } else {
            // Format: project,position,  length  qty  0
            if (ln.empty() || startsWith(ln, ";")) continue;
            auto p1 = ln.find(',');
            if (p1 == std::string::npos) continue;
            auto p2 = ln.find(',', p1 + 1);
            if (p2 == std::string::npos) continue;

            CamBarItem it;
            it.project  = trim(ln.substr(0, p1));
            it.position = trim(ln.substr(p1 + 1, p2 - p1 - 1));
            auto rest   = splitWords(trim(ln.substr(p2 + 1)));
            if (!rest.empty())    try { it.length   = std::stod(rest[0]); } catch (...) {}
            if (rest.size() >= 2) try { it.quantity = std::stoi(rest[1]); } catch (...) {}
            bar.items.push_back(it);
        }
    }
    return bar;
}

// Parse a _STOCK_ block from the CAM file.
CamStock CamParser::parseStock(const Lines& lines, int start, int end) {
    CamStock s;
    for (int i = start; i < end; ++i) {
        std::string ln = trim(lines[i]);
        if      (startsWith(ln,"STK_PRF:")) s.profile  = normaliseProfile(afterColon(ln));
        else if (startsWith(ln,"STK_LEN:")) try { s.length   = std::stod(afterColon(ln)); } catch (...) {}
        else if (startsWith(ln,"STK_MAT:")) s.material = normaliseMaterial(afterColon(ln));
        else if (startsWith(ln,"STK_WGH:")) try { s.weight   = std::stod(afterColon(ln)); } catch (...) {}
        else if (startsWith(ln,"STK_SUR:")) try { s.surface  = std::stod(afterColon(ln)); } catch (...) {}
        else if (startsWith(ln,"STK_QTY:")) try { s.quantity = std::stoi(afterColon(ln)); } catch (...) {}
        else if (startsWith(ln,"STK_RES:")) try { s.remnant  = std::stoi(afterColon(ln)); } catch (...) {}
    }
    return s;
}

// Main entry: parse an entire CAM file.
// Splits into _PIECE_, _BAR_, and _STOCK_ blocks.
CamFile CamParser::parse(const std::string& filePath) {
    CamFile cam;
    cam.filePath = filePath;
    // Extract filename from path
    auto slash = filePath.find_last_of("/\\");
    cam.fileName = (slash == std::string::npos) ? filePath : filePath.substr(slash + 1);

    std::ifstream file(filePath);
    if (!file.is_open()) return cam;

    Lines lines;
    std::string line;
    while (std::getline(file, line)) lines.push_back(line);
    file.close();

    // ── Find block boundaries ──────────────────────────────────────
    struct Block { std::string type; int s, e; };
    std::vector<Block> blocks;

    for (int i = 0; i < (int)lines.size(); ++i) {
        std::string t = trim(lines[i]);
        if (t == "_PIECE_" || t == "_BAR_" || t == "_STOCK_") {
            if (!blocks.empty()) blocks.back().e = i;
            blocks.push_back({ t, i, (int)lines.size() });
        }
    }
    if (!blocks.empty()) blocks.back().e = (int)lines.size();

    for (auto& b : blocks) {
        if      (b.type == "_PIECE_") cam.pieces.push_back(parsePiece(lines, b.s, b.e));
        else if (b.type == "_BAR_")   cam.bars.push_back(parseBar(lines, b.s, b.e));
        else if (b.type == "_STOCK_") cam.stocks.push_back(parseStock(lines, b.s, b.e));
    }

    // Extracts weight-per-meter from bars and adds it to pieces.
    std::map<std::string, double> wpm;
    for (auto& bar : cam.bars)
        if (bar.length > 0)
            wpm[bar.profile] = (bar.weightTotal / bar.length) * 1000.0;

    for (auto& p : cam.pieces)
        if (p.weightPerMeter == 0 && wpm.count(p.profile))
            p.weightPerMeter = wpm[p.profile];

    return cam;
}
