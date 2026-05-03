#pragma once

struct SharedConfigData {
    bool streamMode;
    float fireSizeMultiplier;
    int colorMode;       // 0: Normal(Emotion), 1: Blue, 2: Purple, 3: Red (Kept for backwards compatibility or override)
    float suitOffsetY;
    float particleSpeedMulti;
    float particleLifeMulti;
    float colorR;
    float colorG;
    float colorB;
    float coreSizeMulti;
    float voiceMultiplier;
    float particleOpacity;
    float innerRatio;
    float midRatio;
    float outerRatio;
};

class SharedConfig {
public:
    SharedConfigData* data = nullptr;
    void* hMapFile = nullptr;

    bool Init(bool isCreator);
    ~SharedConfig();
};

void LaunchControlPanel();
