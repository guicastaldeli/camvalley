#include "classifier_renderer.h"
#include "../loader.h"
#include <iostream>
#include <chrono>

bool ClassifierRenderer::load(const std::string& fileName) {
    Loader loader;
    cascadeLoaded = loader.loadFile(fileName, faceCascade);
    return cascadeLoaded && faceCascade.isLoaded();
}

/*
** Create Integral Image
*/
std::vector<std::vector<float>> ClassifierRenderer::createIntegralImage(const std::vector<std::vector<unsigned char>>& image) {
    if (image.empty() || image[0].empty()) {
        return std::vector<std::vector<float>>();
    }
    
    int height = image.size();
    int width = image[0].size();
    std::vector<std::vector<float>> integral(height, std::vector<float>(width, 0));

    integral[0][0] = image[0][0];
    for(int x = 1; x < width; x++) {
        integral[0][x] = integral[0][x-1] + image[0][x];
    }

    for(int y = 1; y < height; y++) {
        float rowSum = 0;
        for(int x = 0; x < width; x++) {
            rowSum += image[y][x];
            integral[y][x] = integral[y-1][x] + rowSum;
        }
    }

    return integral;
}

void ClassifierRenderer::forceEnable() {
    faceDetectionEnabled = true;
    cascadeLoaded = true;
}

/*
** Process Frame for Faces
*/
void ClassifierRenderer::processFrameForFaces(const std::vector<std::vector<unsigned char>>& frame) {
    static auto lastProcessTime = std::chrono::steady_clock::now();
    auto currentTime = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastProcessTime);
    if(elapsed.count() < 66) return;
    lastProcessTime = currentTime;

    if(!faceDetectionEnabled || !isCascadeLoaded()) return;
    if(frame.empty() || frame[0].empty()) return;

    auto integral = createIntegralImage(frame);
    if(integral.empty()) return;
    auto newFaces = faceCascade.detectFaces(integral);
    {
        std::lock_guard<std::mutex> lock(facesMutex);
        currentFaces = newFaces;
    }
}

/*
** Draw
*/
void ClassifierRenderer::draw(HDC hdc, const std::vector<Rect>& faces) {
    if(faces.empty()) {
        std::wcout << "Empty!!" << std::endl;
        return;
    }

    static HPEN redPen = CreatePen(PS_SOLID, 3, RGB(255, 0, 0));
    static HBRUSH nullBrush = (HBRUSH)GetStockObject(NULL_BRUSH);

    HPEN oldPen = (HPEN)SelectObject(hdc, redPen);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, nullBrush);
    int oldBkMode = SetBkMode(hdc, TRANSPARENT);

    for(const auto& face : faces) {
        Rectangle(
            hdc,
            face.x,
            face.y,
            face.x + face.width,
            face.y + face.height
        );
    }
    SetBkMode(hdc, oldBkMode);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
}

std::vector<Rect> ClassifierRenderer::getCurrentFaces() {
    std::lock_guard<std::mutex> lock(facesMutex);
    return currentFaces;
}