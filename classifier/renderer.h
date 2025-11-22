#include "classifier.h"
#include "haar_cascade.h"
#include <windows.h>
#include <thread>
#include <iostream>

class Renderer {
    public:
        HaarCascade faceCascade;
        std::vector<Rect> currentFaces;
        bool faceDetectionEnabled;
        bool cascadeLoaded;

        bool load(const std::string& fileName);
        void processFrameForFaces(const std::vector<std::vector<unsigned char>>& frame);
        void draw(HDC hdc);

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
        std::vector<std::vector<float>> createIntegralImage(
            const std::vector<std::vector<unsigned char>>& image
        );
};