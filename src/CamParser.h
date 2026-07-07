#pragma once
#include "CamTypes.h"
#include <string>
#include <vector>

// Parse CAM files and extract pieces, bars, and stock data.
class CamParser {
public:
    static CamFile      parse(const std::string& filePath);
    static std::string  normaliseProfile(const std::string& raw);
    static std::string  normaliseMaterial(const std::string& raw);
    static std::string  detectSectionType(const std::string& profile);

private:
    using Lines = std::vector<std::string>;

    static CamPiece  parsePiece (const Lines& lines, int start, int end);
    static CamBar    parseBar   (const Lines& lines, int start, int end);
    static CamStock  parseStock (const Lines& lines, int start, int end);

    static void parseShape(const Lines& lines, int start, int end, CamPiece& piece);
    static void parseHoles(const Lines& lines, int start, int end, CamPiece& piece);

    static std::string trim(const std::string& s);
    static std::string afterColon(const std::string& s);
    static std::vector<std::string> splitWords(const std::string& s);
    static bool startsWith(const std::string& s, const std::string& prefix);
};
