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
        
        std::string onnxPath = modelDir + "/MediaPipeFaceLandmarkDetector.onnx";
        if (std::filesystem::exists(onnxPath)) {
            m_landmarkNet = cv::dnn::readNetFromONNX(onnxPath);
            m_hasLandmarkNet = true;
            std::cout << "[FaceTracker] MediaPipe ONNX Landmark model loaded successfully!" << std::endl;
        } else {
            std::cout << "[FaceTracker] No MediaPipe ONNX model found at " << onnxPath << " (Falling back to simulated emotions/3D)" << std::endl;
        }
    } catch (const cv::Exception& e) {
        std::cerr << "[FaceTracker] ERROR loading models: " << e.what() << std::endl;
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
                bestFace.detected = true;
                bestFace.frameW   = frameW;
                bestFace.frameH   = frameH;
                bestConfidence    = confidence;
            }
        }

        if (bestFace.detected) {
            if (m_hasLandmarkNet) {
                // Use MediaPipe Face Landmark ONNX
                int px1 = (int)(bestFace.centerX * frameW - bestFace.width * frameW * 0.8f);
                int py1 = (int)(bestFace.centerY * frameH - bestFace.height * frameH * 0.8f);
                int px2 = (int)(bestFace.centerX * frameW + bestFace.width * frameW * 0.8f);
                int py2 = (int)(bestFace.centerY * frameH + bestFace.height * frameH * 0.8f);

                px1 = std::max(0, std::min(frameW - 1, px1));
                py1 = std::max(0, std::min(frameH - 1, py1));
                px2 = std::max(0, std::min(frameW - 1, px2));
                py2 = std::max(0, std::min(frameH - 1, py2));

                if (px2 > px1 && py2 > py1) {
                    cv::Rect roi(px1, py1, px2 - px1, py2 - py1);
                    cv::Mat faceCrop = frame(roi);
                    cv::Mat blobLandmark = cv::dnn::blobFromImage(faceCrop, 1.0/255.0, cv::Size(192, 192), cv::Scalar(0,0,0), true, false);
                    m_landmarkNet.setInput(blobLandmark);
                    
                    std::vector<cv::Mat> outs;
                    try {
                        m_landmarkNet.forward(outs, m_landmarkNet.getUnconnectedOutLayersNames());
                        
                        // Find the output containing 1404+ values (468 landmarks × 3 coords)
                        int lmOutIdx = -1;
                        for (int oi = 0; oi < (int)outs.size(); oi++) {
                            if (outs[oi].total() >= 1404) { lmOutIdx = oi; break; }
                        }
                        
                        if (lmOutIdx >= 0) {
                            float* data = (float*)outs[lmOutIdx].data;
                            float cropW = (float)(px2 - px1);
                            float cropH = (float)(py2 - py1);
                            
                            // --- Extract all 468 landmarks ---
                            for (int i = 0; i < 468; i++) {
                                float lx = data[i * 3 + 0]; // x in 192×192 crop space
                                float ly = data[i * 3 + 1]; // y in 192×192 crop space
                                float lz = data[i * 3 + 2]; // z depth
                                
                                // Transform from crop space → full frame normalized [0,1]
                                float framePixX = px1 + (lx / 192.0f) * cropW;
                                float framePixY = py1 + (ly / 192.0f) * cropH;
                                
                                bestFace.landmarks[i][0] = framePixX / (float)frameW;
                                bestFace.landmarks[i][1] = framePixY / (float)frameH;
                                bestFace.landmarks[i][2] = lz / 192.0f;
                            }
                            bestFace.landmarkCount = 468;
                            bestFace.hasLandmarks = true;
                            
                            // --- Head pose estimation using solvePnP ---
                            // MediaPipe landmark indices for key facial points
                            const int NOSE_TIP = 1, CHIN = 152;
                            const int LEFT_EYE = 33, RIGHT_EYE = 263;
                            const int LEFT_MOUTH = 61, RIGHT_MOUTH = 291;
                            
                            // 3D model points (generic face proportions, in mm)
                            std::vector<cv::Point3d> modelPts = {
                                {0.0, 0.0, 0.0},           // Nose tip
                                {0.0, -330.0, -65.0},      // Chin
                                {-225.0, 170.0, -135.0},   // Left eye corner
                                {225.0, 170.0, -135.0},    // Right eye corner
                                {-150.0, -150.0, -125.0},  // Left mouth corner
                                {150.0, -150.0, -125.0}    // Right mouth corner
                            };
                            
                            // 2D image points from detected landmarks (pixel coords)
                            const int indices[] = {NOSE_TIP, CHIN, LEFT_EYE, RIGHT_EYE, LEFT_MOUTH, RIGHT_MOUTH};
                            std::vector<cv::Point2d> imgPts;
                            for (int idx : indices) {
                                imgPts.push_back({
                                    bestFace.landmarks[idx][0] * frameW,
                                    bestFace.landmarks[idx][1] * frameH
                                });
                            }
                            
                            // Approximate camera intrinsics
                            double focal = (double)frameW;
                            cv::Mat camMat = (cv::Mat_<double>(3,3) <<
                                focal, 0, frameW / 2.0,
                                0, focal, frameH / 2.0,
                                0, 0, 1);
                            cv::Mat distCoeffs = cv::Mat::zeros(4, 1, CV_64F);
                            
                            cv::Mat rvec, tvec;
                            if (cv::solvePnP(modelPts, imgPts, camMat, distCoeffs, rvec, tvec)) {
                                cv::Mat R;
                                cv::Rodrigues(rvec, R);
                                
                                // Extract Euler angles from rotation matrix
                                double sy = sqrt(R.at<double>(0,0) * R.at<double>(0,0) +
                                                 R.at<double>(1,0) * R.at<double>(1,0));
                                if (sy > 1e-6) {
                                    bestFace.pitch = (float)(atan2(R.at<double>(2,1), R.at<double>(2,2)) * 180.0 / CV_PI);
                                    bestFace.yaw   = (float)(atan2(-R.at<double>(2,0), sy) * 180.0 / CV_PI);
                                    bestFace.roll  = (float)(atan2(R.at<double>(1,0), R.at<double>(0,0)) * 180.0 / CV_PI);
                                }
                            }
                            
                            // --- Emotion: mouth open ratio ---
                            const int UPPER_LIP = 13, LOWER_LIP = 14;
                            float mouthGap = std::abs(bestFace.landmarks[LOWER_LIP][1] - bestFace.landmarks[UPPER_LIP][1]);
                            float faceHNorm = bestFace.height;
                            bestFace.emotionScore = std::min(1.0f, (mouthGap / std::max(0.01f, faceHNorm)) * 5.0f);
                        }
                    } catch (...) {
                        // Forward failed, fallback to simulated emotion
                        bestFace.emotionScore = 0.5f + 0.5f * sinf((float)cv::getTickCount() / cv::getTickFrequency());
                    }
                }
            } else {
                // Simulate emotion for testing (no landmark model)
                bestFace.emotionScore = 0.5f + 0.5f * sinf((float)cv::getTickCount() / cv::getTickFrequency());
            }
        }

        // --- Convert frame to RGBA for Raylib ---
        // Mirror the frame horizontally so it feels like looking in a mirror
        cv::Mat mirrored;
        cv::flip(frame, mirrored, 1);

        cv::Mat rgba;
        cv::cvtColor(mirrored, rgba, cv::COLOR_BGR2RGBA);

        // --- Mirror face data to match the mirrored display ---
        if (bestFace.detected) {
            bestFace.centerX = 1.0f - bestFace.centerX;
            // Mirror landmark X coordinates and negate yaw/roll for mirrored view
            if (bestFace.hasLandmarks) {
                for (int i = 0; i < bestFace.landmarkCount; i++) {
                    bestFace.landmarks[i][0] = 1.0f - bestFace.landmarks[i][0];
                }
                bestFace.yaw  = -bestFace.yaw;
                bestFace.roll = -bestFace.roll;
            }
        }

        // --- Update shared data (thread-safe) ---
        {
            std::lock_guard<std::mutex> lock(m_dataMutex);
            m_faceData = bestFace;
            m_latestFrameRGBA = rgba.clone();
        }

        // --- Efficient sleeping ---
        // 24 FPS target is ~41ms per frame. 
        // Inference + processing might take 5-15ms, so sleeping 30ms is safe and significantly saves CPU.
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
}
