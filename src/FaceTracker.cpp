#include "FaceTracker.h"
#include <iostream>
#include <filesystem>

FaceTracker::FaceTracker() {}

FaceTracker::~FaceTracker()
{
    Stop();
}

bool FaceTracker::Init(int cameraIndex, const std::string& modelDir)
{
    // --- Open webcam ---
    m_capture.open(cameraIndex, cv::CAP_DSHOW); // DirectShow backend on Windows
    if (!m_capture.isOpened()) {
        // Fallback: try default backend
        m_capture.open(cameraIndex);
        if (!m_capture.isOpened()) {
            std::cerr << "[FaceTracker] ERROR: Could not open camera " << cameraIndex << std::endl;
            return false;
        }
    }

    // Set camera resolution
    m_capture.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    m_capture.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    m_capture.set(cv::CAP_PROP_FPS, 30);

    std::cout << "[FaceTracker] Camera opened: " 
              << (int)m_capture.get(cv::CAP_PROP_FRAME_WIDTH) << "x" 
              << (int)m_capture.get(cv::CAP_PROP_FRAME_HEIGHT) << " @ "
              << (int)m_capture.get(cv::CAP_PROP_FPS) << " FPS" << std::endl;

    // --- Load DNN face detection model ---
    std::string prototxtPath  = modelDir + "/deploy.prototxt";
    std::string caffemodelPath = modelDir + "/res10_300x300_ssd_iter_140000.caffemodel";

    if (!std::filesystem::exists(prototxtPath)) {
        std::cerr << "[FaceTracker] ERROR: Model file not found: " << prototxtPath << std::endl;
        return false;
    }
    if (!std::filesystem::exists(caffemodelPath)) {
        std::cerr << "[FaceTracker] ERROR: Model file not found: " << caffemodelPath << std::endl;
        return false;
    }

    try {
        m_net = cv::dnn::readNetFromCaffe(prototxtPath, caffemodelPath);
        m_net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        m_net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        std::cout << "[FaceTracker] DNN model loaded successfully." << std::endl;
    } catch (const cv::Exception& e) {
        std::cerr << "[FaceTracker] ERROR loading DNN model: " << e.what() << std::endl;
        return false;
    }

    return true;
}

void FaceTracker::Start()
{
    if (m_running.load()) return;
    m_running.store(true);
    m_thread = std::thread(&FaceTracker::CaptureLoop, this);
    std::cout << "[FaceTracker] Tracking thread started." << std::endl;
}

void FaceTracker::Stop()
{
    m_running.store(false);
    if (m_thread.joinable()) {
        m_thread.join();
    }
    if (m_capture.isOpened()) {
        m_capture.release();
    }
    std::cout << "[FaceTracker] Tracking thread stopped." << std::endl;
}

FaceData FaceTracker::GetFaceData() const
{
    std::lock_guard<std::mutex> lock(m_dataMutex);
    return m_faceData;
}

bool FaceTracker::GetFrameRGBA(unsigned char* outPixels, int& outWidth, int& outHeight) const
{
    std::lock_guard<std::mutex> lock(m_dataMutex);
    if (m_latestFrameRGBA.empty()) return false;

    outWidth  = m_latestFrameRGBA.cols;
    outHeight = m_latestFrameRGBA.rows;
    
    size_t dataSize = (size_t)outWidth * outHeight * 4;
    std::memcpy(outPixels, m_latestFrameRGBA.data, dataSize);
    return true;
}

void FaceTracker::CaptureLoop()
{
    cv::Mat frame, blob;

    while (m_running.load()) {
        m_capture >> frame;
        if (frame.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        int frameW = frame.cols;
        int frameH = frame.rows;

        // --- Run DNN face detection ---
        // The model expects a 300x300 BGR image, mean-subtracted
        blob = cv::dnn::blobFromImage(frame, 1.0, cv::Size(300, 300),
                                       cv::Scalar(104.0, 177.0, 123.0), false, false);
        m_net.setInput(blob);
        cv::Mat detections = m_net.forward();

        // detections shape: [1, 1, N, 7]
        // Each detection: [batchId, classId, confidence, x1, y1, x2, y2] (normalized 0-1)
        cv::Mat detectionMat(detections.size[2], detections.size[3], CV_32F, detections.ptr<float>());

        FaceData bestFace;
        float bestConfidence = 0.0f;

        for (int i = 0; i < detectionMat.rows; i++) {
            float confidence = detectionMat.at<float>(i, 2);
            if (confidence > m_confidenceThreshold && confidence > bestConfidence) {
                float x1 = detectionMat.at<float>(i, 3);
                float y1 = detectionMat.at<float>(i, 4);
                float x2 = detectionMat.at<float>(i, 5);
                float y2 = detectionMat.at<float>(i, 6);

                // Clamp to [0, 1]
                x1 = std::max(0.0f, std::min(1.0f, x1));
                y1 = std::max(0.0f, std::min(1.0f, y1));
                x2 = std::max(0.0f, std::min(1.0f, x2));
                y2 = std::max(0.0f, std::min(1.0f, y2));

                bestFace.centerX  = (x1 + x2) / 2.0f;
                bestFace.centerY  = (y1 + y2) / 2.0f;
                bestFace.width    = x2 - x1;
                bestFace.height   = y2 - y1;
                
                // Fallback 3D & Emotion data (Since OpenCV SSD is 2D only)
                bestFace.centerZ  = bestFace.width; // Depth proxy
                bestFace.pitch    = 0.0f;
                bestFace.yaw      = 0.0f;
                bestFace.roll     = 0.0f;
                
                // Simulate some emotion for testing (oscillating between chill and excited)
                bestFace.emotionScore = 0.5f + 0.5f * sinf((float)cv::getTickCount() / cv::getTickFrequency());
                
                bestFace.detected = true;
                bestFace.frameW   = frameW;
                bestFace.frameH   = frameH;
                bestConfidence    = confidence;
            }
        }

        // --- Convert frame to RGBA for Raylib ---
        // Mirror the frame horizontally so it feels like looking in a mirror
        cv::Mat mirrored;
        cv::flip(frame, mirrored, 1);

        cv::Mat rgba;
        cv::cvtColor(mirrored, rgba, cv::COLOR_BGR2RGBA);

        // --- Mirror the face X coordinate too ---
        if (bestFace.detected) {
            bestFace.centerX = 1.0f - bestFace.centerX;
        }

        // --- Update shared data (thread-safe) ---
        {
            std::lock_guard<std::mutex> lock(m_dataMutex);
            m_faceData = bestFace;
            m_latestFrameRGBA = rgba.clone();
        }
    }
}
