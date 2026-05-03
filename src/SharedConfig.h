#pragma once

struct SharedConfigData {
    bool streamMode;
    float fireSizeMultiplier;
    int colorMode;
    float suitOffsetY;
    float particleSpeedMulti;
    float particleLifeMulti;
    // Main base color
    float colorR, colorG, colorB;
    // Per-phase color offsets (absolute RGB, not offsets)
    float innerR, innerG, innerB;
    float midR,   midG,   midB;
    float outerR, outerG, outerB;
    // Other controls
    float coreSizeMulti;
    float voiceMultiplier;
    float particleOpacity;
    float innerRatio;
    float midRatio;
    float outerRatio;
    float bgColorR;
    float bgColorG;
    float bgColorB;
    float spreadAngle;
    float seedShape;
};

class SharedConfig {
public:
    SharedConfigData* data = nullptr;
    void* hMapFile = nullptr;

    bool Init(bool isCreator);
    ~SharedConfig();
};

void LaunchControlPanel();
