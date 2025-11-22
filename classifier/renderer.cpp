#include "renderer.h"
#include "../loader.h"
#include <iostream>

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
        std::wcout << L"  - cascadeLoaded: " << cascadeLoaded << std::endl;
        std::wcout << L"  - faceCascade.isLoaded(): " << faceCascade.isLoaded() << std::endl;
        std::wcout << L"  - stages count: " << faceCascade.stages.size() << std::endl;
    }
    
    return cascadeLoaded && faceCascade.isLoaded();
}

/*
** Create Integral Image
*/
std::vector<std::vector<float>> Renderer::createIntegralImage(const std::vector<std::vector<unsigned char>>& image) {
    int height = image.size();
    int width = image[0].size();
    std::vector<std::vector<float>> integral(
        height,
        std::vector<float>(width, 0)
    );

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
** Process Frame 
*/
void Renderer::processFrameForFaces(const std::vector<std::vector<unsigned char>>& frame) {
    std::wcout << L"=== Renderer::processFrameForFaces ===" << std::endl;
    std::wcout << L"faceDetectionEnabled: " << faceDetectionEnabled << std::endl;
    std::wcout << L"cascadeLoaded: " << cascadeLoaded << std::endl;
    std::wcout << L"faceCascade.isLoaded(): " << faceCascade.isLoaded() << std::endl;
    std::wcout << L"faceCascade.stages.size(): " << faceCascade.stages.size() << std::endl;
    
    if(!faceDetectionEnabled || !isCascadeLoaded()) {
        std::wcout << L"Renderer: Face detection not enabled or cascade not loaded" << std::endl;
        std::wcout << L"  - faceDetectionEnabled: " << faceDetectionEnabled << std::endl;
        std::wcout << L"  - isCascadeLoaded(): " << isCascadeLoaded() << std::endl;
        std::wcout << L"  - cascadeLoaded: " << cascadeLoaded << std::endl;
        std::wcout << L"  - faceCascade.isLoaded(): " << faceCascade.isLoaded() << std::endl;
        return;
    }
    
    if(frame.empty() || frame[0].empty()) {
        std::wcout << L"Renderer: Empty frame received" << std::endl;
        return;
    }
    
    std::wcout << L"Renderer: Processing frame " << frame[0].size() << L"x" << frame.size() << std::endl;
    
    // Create integral image
    std::wcout << L"Renderer: Creating integral image..." << std::endl;
    auto integral = createIntegralImage(frame);
    std::wcout << L"Renderer: Integral image created: " << integral[0].size() << L"x" << integral.size() << std::endl;
    
    // Detect faces
    std::wcout << L"Renderer: Starting face detection..." << std::endl;
    currentFaces = faceCascade.detectFaces(integral);
    std::wcout << L"Renderer: Detection complete. Found " << currentFaces.size() << L" faces" << std::endl;
    
    // Print face locations for debugging
    for(size_t i = 0; i < currentFaces.size(); i++) {
        const auto& face = currentFaces[i];
        std::wcout << L"Renderer: Face " << i << L" at (" << face.x << L"," << face.y 
                   << L") size " << face.width << L"x" << face.height << std::endl;
    }
}

/*
** Draw
*/
void Renderer::draw(HDC hdc) {
    if(currentFaces.empty()) {
        std::wcout << "Empty faces" << std::endl;
        return;
    }

    HDC memDC = CreateCompatibleDC(hdc);
    RECT clientRect;
    GetClientRect(WindowFromDC(hdc), &clientRect);
    HBITMAP memBitmap = CreateCompatibleBitmap(
        hdc,
        clientRect.right,
        clientRect.bottom
    );
    HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

    for(const auto& face : currentFaces) {
        RECT rect = {
            face.x,
            face.y,
            face.x + face.width,
            face.y + face.height
        };

        HPEN redPen = CreatePen(PS_SOLID, 3, RGB(0, 10, 255));
        HPEN oldPen = (HPEN)SelectObject(hdc, redPen);
        HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));

        Rectangle(
            hdc,
            rect.left,
            rect.top,
            rect.right,
            rect.bottom
        );
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
        DeleteObject(redPen);
    }
    
    BitBlt(
        hdc,
        0,
        0,
        clientRect.right,
        clientRect.bottom,
        memDC,
        0,
        0,
        SRCCOPY
    );
    SelectObject(memDC, oldBitmap);
    DeleteObject(memBitmap);
    DeleteDC(memDC);
}