#include "../classifier/classifier.h"
#include "../classifier/haar_cascade.h"
#include <windows.h>
#include <thread>
#include <iostream>
#include <mutex>

class ClassifierRenderer {
    public:
        HaarCascade faceCascade;
        std::vector<Rect> currentFaces;
        std::mutex facesMutex;
        bool faceDetectionEnabled;
        bool cascadeLoaded;

        bool load(const std::string& fileName);
        void processFrameForFaces(const std::vector<std::vector<unsigned char>>& frame);
        void draw(HDC hdc, const std::vector<Rect>& faces);

        void forceEnable();
        bool isCascadeLoaded() const {
            return faceCascade.isLoaded();
        }
        bool isFaceDetectionEnabled() const {
            return faceDetectionEnabled;
        }
        HaarCascade& getFaceCascade() {
            return faceCascade;
        }
        std::vector<Rect> getCurrentFaces();
        std::vector<std::vector<float>> createIntegralImage(
            const std::vector<std::vector<unsigned char>>& image
        );
};