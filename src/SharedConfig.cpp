#include "SharedConfig.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <iostream>

bool SharedConfig::Init(bool isCreator) {
    if (isCreator) {
        hMapFile = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(SharedConfigData), "Local\\VTuberConfigMap");
        if (hMapFile == NULL) {
            std::cerr << "Could not create file mapping object.\n";
            return false;
        }
        data = (SharedConfigData*)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedConfigData));
        if (data) {
            data->streamMode = true; // Safe default for streamers
            data->fireSizeMultiplier = 1.0f;
            data->colorMode = 0;
            data->suitOffsetY = 50.0f;
            data->particleSpeedMulti = 1.0f;
            data->particleLifeMulti = 1.0f;
            data->colorR = 255.0f;
            data->colorG = 40.0f;
            data->colorB = 10.0f;
            data->coreSizeMulti = 1.0f;
            data->voiceMultiplier = 3.0f;
            data->particleOpacity = 1.0f;
        }
    } else {
        hMapFile = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, "Local\\VTuberConfigMap");
        if (hMapFile == NULL) {
            std::cerr << "Could not open file mapping object.\n";
            return false;
        }
        data = (SharedConfigData*)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedConfigData));
    }

    if (data == NULL) {
        std::cerr << "Could not map view of file.\n";
        if (hMapFile) CloseHandle(hMapFile);
        hMapFile = nullptr;
        return false;
    }
    return true;
}

SharedConfig::~SharedConfig() {
    if (data) UnmapViewOfFile(data);
    if (hMapFile) CloseHandle(hMapFile);
}
