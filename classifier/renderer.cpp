#include "renderer.h"
#include "../loader.h"
#include <iostream>
#include <chrono>

bool Renderer::load(const std::string& fileName) {
    Loader loader;
    cascadeLoaded = loader.loadFile(fileName, faceCascade);
    
    std::wcout << L"Renderer::load: cascadeLoaded = " << cascadeLoaded << std::endl;
    std::wcout << L"Renderer::load: faceCascade.isLoaded() = " << faceCascade.isLoaded() << std::endl;
    std::wcout << L"Renderer::load: faceCascade.stages.size() = " << faceCascade.stages.size() << std::endl;
    
    if (cascadeLoaded && faceCascade.isLoaded()) {
        std::wcout << L"Renderer: Cascade loaded successfully with " 
                   << faceCascade.stages.size() << L" stages" << std::endl;
    } else {
        std::wcout << L"Renderer: Failed to load cascade or cascade not properly initialized" << std::endl;
    }
    
    return cascadeLoaded && faceCascade.isLoaded();
}

/*
** Create Integral Image
*/
std::vector<std::vector<float>> Renderer::createIntegralImage(const std::vector<std::vector<unsigned char>>& image) {
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

void Renderer::forceEnable() {
    faceDetectionEnabled = true;
    cascadeLoaded = true;
}

/*
** Process Frame for Faces
*/
void Renderer::processFrameForFaces(const std::vector<std::vector<unsigned char>>& frame) {
    static auto lastProcessTime = std::chrono::steady_clock::now();
    auto currentTime = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastProcessTime);
    if(elapsed.count() < 66) return;
    lastProcessTime = currentTime;

    static int frameCount = 0;
    frameCount++;
    if(!faceDetectionEnabled || !isCascadeLoaded()) return;
    if(frame.empty() || frame[0].empty()) return;

    auto integral = createIntegralImage(frame);
    if(integral.empty()) return;
    currentFaces = faceCascade.detectFaces(integral);
}

/*
** Draw
*/
void Renderer::draw(HDC hdc) {
    if(currentFaces.empty()) {
        std::wcout << "Empty!!" << std::endl;
        return;
    }

    static HPEN redPen = CreatePen(PS_SOLID, 3, RGB(0, 0, 255));
    static HBRUSH nullBrush = (HBRUSH)GetStockObject(NULL_BRUSH);

    HPEN oldPen = (HPEN)SelectObject(hdc, redPen);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, nullBrush);

    for(const auto& face : currentFaces) {
        Rectangle(
            hdc,
            face.x,
            face.y,
            face.x + face.width,
            face.y + face.height
        );
    }
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
}
