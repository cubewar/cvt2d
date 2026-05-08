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
                        
                        // One-time debug: print all output shapes
                        static bool debugPrinted = false;
                        if (!debugPrinted) {
                            std::cout << "[FaceTracker] ONNX model has " << outs.size() << " outputs:" << std::endl;
                            for (int oi = 0; oi < (int)outs.size(); oi++) {
                                std::cout << "  Output[" << oi << "]: total=" << outs[oi].total()
                                          << " dims=" << outs[oi].dims;
                                for (int d = 0; d < outs[oi].dims; d++)
                                    std::cout << (d == 0 ? " shape=[" : ",") << outs[oi].size[d];
                                std::cout << "]" << std::endl;
                            }
                        }
                        
                        // Find the output containing 1404+ values (468 landmarks × 3 coords)
                        int lmOutIdx = -1;
                        for (int oi = 0; oi < (int)outs.size(); oi++) {
                            if (outs[oi].total() >= 1404) { lmOutIdx = oi; break; }
                        }
                        
                        if (lmOutIdx >= 0) {
                            float* data = (float*)outs[lmOutIdx].data;
                            float cropW = (float)(px2 - px1);
                            float cropH = (float)(py2 - py1);
                            
                            // Auto-detect coordinate range: check if values are in [0,1] or [0,192]
                            // Sample the first 10 landmarks to determine scale
                            float maxVal = 0.0f;
                            for (int i = 0; i < 30; i++) { // first 10 landmarks × 3
                                float v = std::abs(data[i]);
                                if (v > maxVal) maxVal = v;
                            }
                            // If max < 2.0, values are likely normalized [0,1]
                            // If max > 2.0, values are likely in pixel space [0,192]
                            float coordScale = (maxVal > 2.0f) ? 192.0f : 1.0f;
                            
                            if (!debugPrinted) {
                                std::cout << "[FaceTracker] Landmark output[" << lmOutIdx << "]:"
                                          << " maxVal=" << maxVal 
                                          << " coordScale=" << coordScale << std::endl;
                                // Print first 3 landmarks
                                for (int i = 0; i < 3; i++) {
                                    std::cout << "  Landmark[" << i << "]: x=" << data[i*3+0]
                                              << " y=" << data[i*3+1]
                                              << " z=" << data[i*3+2] << std::endl;
                                }
                                debugPrinted = true;
                            }
                            
                            // --- Extract all 468 landmarks ---
                            for (int i = 0; i < 468; i++) {
                                float lx = data[i * 3 + 0] / coordScale; // normalized to [0,1] in crop
                                float ly = data[i * 3 + 1] / coordScale;
                                float lz = data[i * 3 + 2] / coordScale;
                                
                                // Transform from crop-normalized → full frame normalized [0,1]
                                float framePixX = px1 + lx * cropW;
                                float framePixY = py1 + ly * cropH;
                                
                                bestFace.landmarks[i][0] = framePixX / (float)frameW;
                                bestFace.landmarks[i][1] = framePixY / (float)frameH;
                                bestFace.landmarks[i][2] = lz;
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
                            
                            // --- Emotion detection from landmarks ---
                            // Key landmark indices (MediaPipe 468-point mesh)
                            // Eyes: measure openness via vertical/horizontal ratio
                            const int L_EYE_TOP = 159, L_EYE_BOT = 145, L_EYE_L = 33, L_EYE_R = 133;
                            const int R_EYE_TOP = 386, R_EYE_BOT = 374, R_EYE_L = 362, R_EYE_R = 263;
                            // Mouth
                            const int MOUTH_TOP = 13, MOUTH_BOT = 14, MOUTH_L = 61, MOUTH_R = 291;
                            // Eyebrows (inner points near nose bridge)
                            const int L_BROW_INNER = 107, R_BROW_INNER = 336;
                            const int L_BROW_MID = 66, R_BROW_MID = 296;
                            
                            auto dist = [&](int a, int b) {
                                float dx = bestFace.landmarks[a][0] - bestFace.landmarks[b][0];
                                float dy = bestFace.landmarks[a][1] - bestFace.landmarks[b][1];
                                return sqrtf(dx*dx + dy*dy);
                            };
                            
                            float faceH = bestFace.height;
                            float faceW = bestFace.width;
                            if (faceH < 0.01f) faceH = 0.01f;
                            if (faceW < 0.01f) faceW = 0.01f;
                            
                            // Eye Aspect Ratio (vertical / horizontal, normalized to face size)
                            float leftEAR  = dist(L_EYE_TOP, L_EYE_BOT) / dist(L_EYE_L, L_EYE_R);
                            float rightEAR = dist(R_EYE_TOP, R_EYE_BOT) / dist(R_EYE_L, R_EYE_R);
                            float avgEAR = (leftEAR + rightEAR) / 2.0f;
                            
                            // Mouth Aspect Ratio (vertical / horizontal)
                            float mouthVert = dist(MOUTH_TOP, MOUTH_BOT);
                            float mouthHoriz = dist(MOUTH_L, MOUTH_R);
                            float MAR = mouthVert / std::max(0.001f, mouthHoriz);
                            
                            // Smile ratio: mouth corners higher than mouth center
                            // In screen coords, Y increases downward, so corners BELOW center = frown
                            float mouthCenterY = (bestFace.landmarks[MOUTH_TOP][1] + bestFace.landmarks[MOUTH_BOT][1]) / 2.0f;
                            float cornerAvgY = (bestFace.landmarks[MOUTH_L][1] + bestFace.landmarks[MOUTH_R][1]) / 2.0f;
                            float smileRatio = (mouthCenterY - cornerAvgY) / faceH; // positive = smile
                            
                            // Brow furrow: how close inner brows are to eyes (normalized to face height)
                            float leftBrowDist  = (bestFace.landmarks[L_EYE_TOP][1] - bestFace.landmarks[L_BROW_MID][1]) / faceH;
                            float rightBrowDist = (bestFace.landmarks[R_EYE_TOP][1] - bestFace.landmarks[R_BROW_MID][1]) / faceH;
                            float avgBrowDist = (leftBrowDist + rightBrowDist) / 2.0f;
                            // Inner brow pinch: horizontal distance between inner brow points
                            float browPinch = dist(L_BROW_INNER, R_BROW_INNER) / faceW;
                            
                            // --- Classify emotion ---
                            float surpriseScore = 0.0f, angryScore = 0.0f, happyScore = 0.0f;
                            
                            // Surprised: wide eyes (high EAR) + mouth open (high MAR)
                            if (avgEAR > 0.32f && MAR > 0.35f) {
                                surpriseScore = std::min(1.0f, (avgEAR - 0.32f) * 8.0f + (MAR - 0.35f) * 3.0f);
                            }
                            
                            // Angry: additive scoring from multiple signals
                            // (works across different face structures)
                            {
                                float angryPoints = 0.0f;
                                
                                // Signal 1: Brow furrow — brows closer to eyes than normal
                                if (avgBrowDist < 0.10f) {
                                    angryPoints += (0.10f - avgBrowDist) * 8.0f;
                                }
                                
                                // Signal 2: Brow pinch — inner brows pulled together
                                if (browPinch < 0.28f) {
                                    angryPoints += (0.28f - browPinch) * 3.0f;
                                }
                                
                                // Signal 3: Frown — mouth corners pulled down (negative smile)
                                if (smileRatio < -0.01f) {
                                    angryPoints += std::abs(smileRatio) * 12.0f;
                                }
                                
                                // Signal 4: Pressed/tense lips — mouth closed tight (low MAR)
                                if (MAR < 0.15f) {
                                    angryPoints += (0.15f - MAR) * 4.0f;
                                }
                                
                                // Signal 5: Squinted eyes — narrowed eyes (low EAR)
                                if (avgEAR < 0.22f) {
                                    angryPoints += (0.22f - avgEAR) * 5.0f;
                                }
                                
                                // Need at least 2 signals contributing to count as angry
                                angryScore = std::min(1.0f, angryPoints * 0.5f);
                            }
                            
                            // Happy: mouth corners raised (smile) + wide mouth
                            if (smileRatio > 0.02f) {
                                happyScore = std::min(1.0f, smileRatio * 15.0f);
                            }
                            
                            // Pick the strongest emotion
                            bestFace.emotion = Emotion::NORMAL;
                            bestFace.emotionBlend = 0.0f;
                            
                            float maxScore = 0.30f; // 30% minimum to trigger an emotion
                            if (surpriseScore > maxScore) {
                                bestFace.emotion = Emotion::SURPRISED;
                                bestFace.emotionBlend = std::min(1.0f, surpriseScore);
                                maxScore = surpriseScore;
                            }
                            if (angryScore > maxScore) {
                                bestFace.emotion = Emotion::ANGRY;
                                bestFace.emotionBlend = std::min(1.0f, angryScore);
                                maxScore = angryScore;
                            }
                            if (happyScore > maxScore) {
                                bestFace.emotion = Emotion::HAPPY;
                                bestFace.emotionBlend = std::min(1.0f, happyScore);
                            }
                            
                            // Keep emotionScore for backward compatibility (mouth open amount)
                            bestFace.emotionScore = std::min(1.0f, MAR * 2.0f);
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
