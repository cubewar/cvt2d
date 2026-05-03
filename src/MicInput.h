#pragma once

/**
 * Captures microphone input using Windows Multimedia API (waveIn)
 * and calculates the smoothed peak volume amplitude.
 */
class MicInput {
public:
    MicInput();
    ~MicInput();

    bool Start();
    void Stop();
    
    // Returns smoothed volume amplitude (0.0 to 1.0)
    float GetVolume() const;

    // Internal callback use only
    void ProcessBuffer(void* hdr);

private:
    void* m_hWaveIn;
    void* m_waveHdr[2];
    short* m_buffers[2];
    bool m_running;
    float m_smoothedVolume;
};
