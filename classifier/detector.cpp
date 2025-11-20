#include <cmath>
#include <algorithm>
#include "haar_cascade.h"
#include "feature.h"
#include "classifier.h"

/*
** Calculate Feature Value
*/
float calcFeatureValue(
    const Feature& f,
    const std::vector<std::vector<float>>& integral,
    int x,
    int y,
    float scale
) {
    int scaledX = static_cast<int>(x + f.x * scale);
    int scaledY = static_cast<int>(y + f.y * scale);
    int scaledW = static_cast<int>(f.width * scale);
    int scaledH = static_cast<int>(f.height * scale);

    auto getSum = [&](
        int x1,
        int y1,
        int x2,
        int y2
    ) -> float {
        if(x1 < 0) x1 = 0;
        if(y1 < 0) y1 = 0;
        if(x2 >= integral[0].size()) x2 = integral[0].size() - 1;
        if(y2 >= integral.size()) y2 = integral.size() - 1;

        float A = (x1 > 0 && y1 > 0) ? integral[y1-1][x1-1] : 0;
        float B = (y1 > 0) ? integral[y1-1][x2] : 0;
        float C = (x1 > 0) ? integral[y2][x1-1] : 0;
        float D = integral[y2][x2];
        
        return D - B - C + A;
    };

    float sum = 0;
    switch(f.type) {
        case Feature::TWO_HORIZONTAL: {
            int halfW = scaledW / 2;
            float leftSum = getSum(
                scaledX,
                scaledY,
                scaledX + halfW - 1,
                scaledY + scaledH - 1
            );
            float rightSum = getSum(
                scaledX + halfW,
                scaledY,
                scaledX + scaledW - 1,
                scaledY + scaledH - 1
            );
            sum = leftSum - rightSum;
            break;
        }
        case Feature::TWO_VERTICAL: {
            int halfH = scaledH / 2;
            float topSum = getSum(
                scaledX,
                scaledY,
                scaledX + scaledW - 1,
                scaledY + halfH - 1
            );
            float bottomSum = getSum(
                scaledX, 
                scaledY + halfH,
                scaledX + scaledW - 1,
                scaledY + scaledH - 1
            );
            sum = topSum - bottomSum;
            break;
        }
        case Feature::THREE_HORIZONTAL: {
            int thirdW = scaledW / 3;
            float leftSum = getSum(
                scaledX,
                scaledY,
                scaledX + thirdW - 1,
                scaledY + scaledH - 1
            );
            float middleSum = getSum(
                scaledX + thirdW,
                scaledY,
                scaledX + 2 * thirdW - 1,
                scaledY + scaledH - 1
            );
            float rightSum = getSum(
                scaledX + 2 * thirdW,
                scaledY,
                scaledX + scaledW - 1,
                scaledY + scaledH - 1
            );
            sum = leftSum - middleSum + rightSum;
            break;
        }
        case Feature::THREE_VERTICAL: {
            int thirdH = scaledH / 3;
            float topSum = getSum(
                scaledX,
                scaledY,
                scaledX + scaledW - 1,
                scaledY + thirdH - 1
            );
            float middleSum = getSum(
                scaledX,
                scaledY + thirdH,
                scaledX + scaledW - 1,
                scaledY + 2 * thirdH - 1
            );
            float bottomSum = getSum(
                scaledX,
                scaledY + 2 * thirdH,
                scaledX + scaledW - 1,
                scaledY + scaledH - 1
            );
            sum = topSum - middleSum + bottomSum;
            break;
        }
        case Feature::FOUR_SQUARE: {
            int halfW = scaledW / 2;
            int halfH = scaledH / 2;
            float topLeft = getSum(
                scaledX,
                scaledY,
                scaledX + halfW - 1,
                scaledY + halfH - 1
            );
            float topRight = getSum(
                scaledX + halfW,
                scaledY,
                scaledX + scaledW - 1,
                scaledY + halfH - 1
            );
            float bottomLeft = getSum(
                scaledX,
                scaledY + halfH,
                scaledX + halfW - 1,
                scaledY + scaledH - 1
            );
            float bottomRight = getSum(
                scaledX + halfW,
                scaledY + halfH,
                scaledX + scaledW - 1,
                scaledY + scaledH - 1
            );
            sum = 
                -topLeft + topRight + 
                bottomLeft - bottomRight;
            break;
        }
    };

    return sum;
}

/*
** Classify
*/
bool WeakClassifier::classify(
    const::std::vector<std::vector<float>>& integral,
    float scale
) const {
    float featureValue = calcFeatureValue(
        feature,
        integral,
        0,
        0,
        scale
    );
    if(feature.leftRight) {
        return featureValue * 
            feature.leftVal < feature.threshold * 
            feature.rightVal;
    } else {
        return featureValue *
            feature.leftVal > feature.threshold *
            feature.rightVal;
    }
}

bool StrongClassifier::classify(
    const std::vector<std::vector<float>>& integral,
    int x,
    int y,
    float scale
) const {
    float sum = 0;
    for(const auto& wc : weakClassifiers) {
        if(wc.classify(integral, scale)) {
            sum += wc.weight;
        }
    }
    return sum >= threshold;
}

/*
** Create Integral Data
*/
std::vector<std::vector<float>> createIntegralImage(const std::vector<std::vector<unsigned char>>& image) {
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
** Detect Faces
*/
std::vector<Rect> HaarCascade::detectFaces(
    const std::vector<std::vector<float>>& integral,
    int minSize,
    int maxSize,
    float scaleFactor
) {
    std::vector<Rect> faces;
    int width = integral[0].size();
    int height = integral.size();

    for(int windowSize = minSize; windowSize <= maxSize; windowSize = static_cast<int>(windowSize * scaleFactor)) {
        float scale = static_cast<float>(windowSize) / baseWidth;
        for(int y = 0; y <= height - windowSize; y += 2) {
            for(int x = 0; x <= width - windowSize; x += 2) {
                bool passedAllStages = true;
                for(const auto& stage : stages) {
                    if(!stage.classify(integral, x, y, scale)) {
                        passedAllStages = false;
                        break;
                    }
                }
                if(passedAllStages) {
                    faces.push_back(Rect(x, y, windowSize, windowSize));
                }
            }
        }
    }

    return faces;
}