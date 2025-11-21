#include "renderer.h"
#include "../loader.h"

bool Renderer::load(const std::string& fileName) {
    Loader loader;
    return loader.loadFile(fileName, faceCascade);
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

/*
** Process Frame 
*/
void Renderer::processFrameForFaces(const std::vector<std::vector<unsigned char>>& frame) {
    if(!faceDetectionEnabled || !faceCascade.isLoaded()) return;
    auto integral = createIntegralImage(frame);
    currentFaces = faceCascade.detectFaces(integral);
}

/*
** Draw
*/
void Renderer::draw(HDC hdc) {
    for(const auto& face : currentFaces) {
        RECT rect = {
            face.x,
            face.y,
            face.x + face.width,
            face.y + face.height
        };

        HPEN redPen = CreatePen(PS_SOLID, 3, RGB(255, 0, 0));
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
}