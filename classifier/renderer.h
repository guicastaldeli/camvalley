#include "classifier.h"
#include "haar_cascade.h"
#include <windows.h>

class Renderer {
    public:
        HaarCascade faceCascade;
        std::vector<Rect> currentFaces;
        bool faceDetectionEnabled;

        bool load(const std::string& fileName);
        void processFrameForFaces(const std::vector<std::vector<unsigned char>>& frame);
        void draw(HDC hdc);
        std::vector<std::vector<float>> createIntegralImage(const std::vector<std::vector<unsigned char>>& image);
        void toggleFaceDetection() {
            faceDetectionEnabled = !faceDetectionEnabled;
        }
};