#pragma once
#include "CamTypes.h"
#include <string>
#include <map>
#include <functional> // Added for the warning callback

class FncGenerator {
public:
    
    static std::string generate(const CamFile& cam, const FncSettings& s, std::function<void(const std::string&)> onWarning = nullptr, int* nestCounterInOut = nullptr);

private:
    static std::string fncProfileCode(const std::string& sectionType);
    static std::string resolveFncCode(const std::string& camCat);
    static std::string fncDrillCode(const std::string& drillType);
    static std::string fmt(double v);
    static std::string pcsBlock(const CamPiece& p, const FncSettings& s, bool withHeader);
    static std::string barBlocks(const CamFile& cam, const FncSettings& s, int& nestCounter);
};