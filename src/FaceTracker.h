#pragma once

#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include <string>
#include <mutex>
#include <thread>
#include <atomic>

/**
 * FaceTracker — OpenCV DNN-based face detection running in a separate thread.
 * 
 * Captures webcam frames and runs a Caffe SSD face detector to produce
 * face position (x, y) and size (width, height) which serve as input
 * for the fire particle emitter.
 * 
 * The face size acts as a depth proxy: closer = bigger face = bigger fire.
 */
struct FaceData {
    float centerX   = 0.0f;  // Normalized [0, 1] — face center X in camera frame
    float centerY   = 0.0f;  // Normalized [0, 1] — face center Y in camera frame
    float width     = 0.0f;  // Normalized [0, 1] — face bounding box width
    float height    = 0.0f;  // Normalized [0, 1] — face bounding box height
    bool  detected  = false;
    int   frameW    = 640;   // Camera frame width in pixels
    int   frameH    = 480;   // Camera frame height in pixels
};

class FaceTracker {
public:
    FaceTracker();
    ~FaceTracker();

    /**
     * Initialize the webcam and face detection model.
     * @param cameraIndex  Webcam device index (default 0)
     * @param modelDir     Path to directory containing deploy.prototxt and caffemodel
     * @return true if initialization succeeded
     */
    bool Init(int cameraIndex = 0, const std::string& modelDir = "assets/models");

    /**
     * Start the background capture/detection thread.
     */
    void Start();

    /**
     * Stop the background thread and release the camera.
     */
    void Stop();

    /**
     * Get the latest face detection result (thread-safe).
     */
    FaceData GetFaceData() const;

    /**
     * Get the latest camera frame as raw pixel data for Raylib texture upload.
     * Returns true if a new frame was available, and copies RGBA data into `outPixels`.
     * @param outPixels   Pre-allocated buffer (frameW * frameH * 4 bytes)
     * @param outWidth    Output frame width
     * @param outHeight   Output frame height
     */
    bool GetFrameRGBA(unsigned char* outPixels, int& outWidth, int& outHeight) const;

    /**
     * Returns true if the tracker is running.
     */
    bool IsRunning() const { return m_running.load(); }

private:
    void CaptureLoop();

    cv::VideoCapture     m_capture;
    cv::dnn::Net         m_net;
    
    mutable std::mutex   m_dataMutex;
    FaceData             m_faceData;
    cv::Mat              m_latestFrame;       // BGR frame from camera
    cv::Mat              m_latestFrameRGBA;   // Converted for Raylib

    std::thread          m_thread;
    std::atomic<bool>    m_running{false};

    float m_confidenceThreshold = 0.5f;
};
