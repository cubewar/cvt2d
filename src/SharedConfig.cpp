#include "SharedConfig.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <iostream>

#pragma comment(lib, "shell32.lib")

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
            data->suitOffsetY = -30.0f;
            data->particleSpeedMulti = 1.0f;
            data->particleLifeMulti = 1.0f;
            data->colorR = 255.0f;
            data->colorG = 40.0f;
            data->colorB = 10.0f;
            data->coreSizeMulti = 1.0f;
            data->voiceMultiplier = 3.0f;
            data->particleOpacity = 0.85f;
            // Spatial thresholds: distRatio 0..innerRatio = core, innerRatio..midRatio = mid, beyond = outer
            data->innerRatio = 0.20f;
            data->midRatio = 0.55f;
            data->outerRatio = 0.12f;
            // Phase colors — vibrant classic fire
            data->innerR = 255.0f; data->innerG = 240.0f; data->innerB = 180.0f;  // Warm white-yellow
            data->midR   = 255.0f; data->midG   = 140.0f; data->midB   = 20.0f;   // Orange
            data->outerR = 220.0f; data->outerG =  30.0f;  data->outerB = 5.0f;   // Deep red
            // Background
            data->bgColorR = 0.0f; data->bgColorG = 0.0f; data->bgColorB = 0.0f;
            data->seedShape = 2.5f;
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

void LaunchControlPanel() {
    ShellExecuteA(NULL, "open", "VTuberControlPanel.exe", NULL, NULL, SW_SHOWDEFAULT);
}
