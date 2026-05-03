#include "MicInput.h"
#include <windows.h>
#include <mmsystem.h>
#include <cmath>
#include <algorithm>

#pragma comment(lib, "winmm.lib")

static void CALLBACK globalWaveInProc(HWAVEIN hwi, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2) {
    if (uMsg == WIM_DATA) {
        MicInput* mic = (MicInput*)dwInstance;
        mic->ProcessBuffer((void*)dwParam1);
        waveInAddBuffer(hwi, (WAVEHDR*)dwParam1, sizeof(WAVEHDR));
    }
}

MicInput::MicInput() : m_hWaveIn(nullptr), m_running(false), m_smoothedVolume(0.0f) {
    m_buffers[0] = new short[4096];
    m_buffers[1] = new short[4096];
    m_waveHdr[0] = new WAVEHDR();
    m_waveHdr[1] = new WAVEHDR();
}

MicInput::~MicInput() {
    Stop();
    delete[] m_buffers[0];
    delete[] m_buffers[1];
    delete (WAVEHDR*)m_waveHdr[0];
    delete (WAVEHDR*)m_waveHdr[1];
}

void MicInput::ProcessBuffer(void* hdr_ptr) {
    if (m_running) {
        WAVEHDR* hdr = (WAVEHDR*)hdr_ptr;
        short* samples = (short*)hdr->lpData;
        int numSamples = hdr->dwBytesRecorded / sizeof(short);
        float maxAmp = 0.0f;
        for (int i = 0; i < numSamples; i++) {
            float amp = std::abs(samples[i]) / 32768.0f;
            if (amp > maxAmp) maxAmp = amp;
        }
        // Smooth the volume (quick rise, slow fall)
        if (maxAmp > m_smoothedVolume) {
            m_smoothedVolume = m_smoothedVolume * 0.2f + maxAmp * 0.8f;
        } else {
            m_smoothedVolume = m_smoothedVolume * 0.8f + maxAmp * 0.2f;
        }
    }
}

bool MicInput::Start() {
    WAVEFORMATEX wfx;
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = 1;
    wfx.nSamplesPerSec = 44100;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = (wfx.nChannels * wfx.wBitsPerSample) / 8;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
    wfx.cbSize = 0;

    HWAVEIN hWaveIn;
    if (waveInOpen(&hWaveIn, WAVE_MAPPER, &wfx, (DWORD_PTR)globalWaveInProc, (DWORD_PTR)this, CALLBACK_FUNCTION) != MMSYSERR_NOERROR) {
        return false;
    }
    m_hWaveIn = hWaveIn;

    for (int i = 0; i < 2; i++) {
        WAVEHDR* hdr = (WAVEHDR*)m_waveHdr[i];
        ZeroMemory(hdr, sizeof(WAVEHDR));
        hdr->lpData = (LPSTR)m_buffers[i];
        hdr->dwBufferLength = 4096 * sizeof(short);
        waveInPrepareHeader(hWaveIn, hdr, sizeof(WAVEHDR));
        waveInAddBuffer(hWaveIn, hdr, sizeof(WAVEHDR));
    }

    m_running = true;
    waveInStart(hWaveIn);
    return true;
}

void MicInput::Stop() {
    m_running = false;
    if (m_hWaveIn) {
        HWAVEIN hWaveIn = (HWAVEIN)m_hWaveIn;
        waveInStop(hWaveIn);
        waveInReset(hWaveIn);
        for (int i = 0; i < 2; i++) {
            waveInUnprepareHeader(hWaveIn, (WAVEHDR*)m_waveHdr[i], sizeof(WAVEHDR));
        }
        waveInClose(hWaveIn);
        m_hWaveIn = nullptr;
    }
}

float MicInput::GetVolume() const {
    return m_smoothedVolume;
}
