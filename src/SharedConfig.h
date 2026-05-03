#pragma once

struct SharedConfigData {
    bool streamMode;
    float fireSizeMultiplier;
    int colorMode;       // 0: Normal(Emotion), 1: Blue, 2: Purple, 3: Red
    float suitOffsetY;
    float particleSpeedMulti;
    float particleLifeMulti;
};

class SharedConfig {
public:
    SharedConfigData* data = nullptr;
    void* hMapFile = nullptr;

    bool Init(bool isCreator);
    ~SharedConfig();
};
