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
    const std::vector<std::vector<float>>& integral,
    int x,
    int y,
    float scale
) const {
    float featureValue = calcFeatureValue(
        feature,
        integral,
        x,
        y,
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
    // Debug: print first few calls
    static int callCount = 0;
    if (callCount < 5) {
        std::wcout << L"StrongClassifier::classify called at (" << x << "," << y << ") with " 
                   << weakClassifiers.size() << " weak classifiers" << std::endl;
        callCount++;
    }
    
    float sum = 0;
    for(const auto& wc : weakClassifiers) {
        if(wc.classify(integral, x, y, scale)) {
            sum += wc.weight;
        }
    }
    return sum >= threshold;
}

std::vector<Rect> HaarCascade::nonMaximumSuppression(
    const std::vector<Rect>& faces,
    float overlapThreshold
) {
    if(faces.empty()) {
        std::wcout << "Faces empty!" << std::endl;
        return faces;
    }

    std::vector<Rect> res;
    std::vector<bool> suppresed(faces.size(), false);
    
    std::vector<Rect> filteredFaces;
    for(const auto& face : faces) {
        if(face.width >= 30 && face.height >= 30) {
            filteredFaces.push_back(face);
        }
    }
    std::wcout << L"Filtered " << (faces.size() - filteredFaces.size()) << " very small faces" << std::endl;
    std::vector<size_t> indices(filteredFaces.size());
    for(size_t i = 0; i < filteredFaces.size(); i++) {
        indices[i] = i;
    }
    std::sort(
        indices.begin(),
        indices.end(),
        [&](
            size_t i1,
            size_t i2
        ) 
    {
        return (
            filteredFaces[i1].width * filteredFaces[i1].height >
            (filteredFaces[i2].width * filteredFaces[i2].height)
        );
    });
    for(size_t i = 0; i < filteredFaces.size(); i++) {
        if(suppresed[indices[i]]) continue;
        res.push_back(filteredFaces[indices[i]]);
        for(size_t j = i+1; j < filteredFaces.size(); j++) {
            if(suppresed[indices[j]]) continue;

            const Rect& r1 = filteredFaces[indices[i]];
            const Rect& r2 = filteredFaces[indices[j]];
            
            int x1 = std::max(r1.x, r2.x);
            int y1 = std::max(r1.y, r2.y);
            int x2 = std::min(r1.x + r1.width, r2.x + r2.width);
            int y2 = std::min(r1.y + r1.height, r2.y + r2.height);
            if(x2 > x1 && y2 > y1) {
                int intersection = (x2 - x1) * (y2 - y1);
                int area1 = r1.width * r1.height;
                int area2 = r2.width * r2.height;
                float overlap = static_cast<float>(intersection) / std::min(area1, area2);
                if(overlap > overlapThreshold) {
                    suppresed[indices[j]] = true;
                }
            }
        } 
    }

    std::wcout << L"Non-maximum suppression: " << filteredFaces.size() << L" -> " << res.size() << L" faces" << std::endl;
    return res;
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
    if(integral.empty() || integral[0].empty()) {
        std::wcout << L"HaarCascade empty integral img" << std::endl;
        return faces;
    }

    int width = integral[0].size();
    int height = integral.size();

    std::wcout << L"Detecting faces in " << width << "x" << height 
               << " image with " << stages.size() << " stages" << std::endl;

    if(maxSize > width || maxSize > height) {
        maxSize = std::min(width, height);
        std::wcout << L"Adjusted maxSize to " << maxSize << std::endl;
    }
    if(minSize > maxSize) {
        minSize = 20;
        std::wcout << L"Adjusted minSize to " << minSize << std::endl;
    }
    std::wcout << L"Scanning window sizes from " << minSize << " to " << maxSize << std::endl;

    int totalWindows = 0;
    int passedStages = 0;
    for(int windowSize = minSize; windowSize <= maxSize; windowSize = static_cast<int>(windowSize * scaleFactor)) {
        float scale = static_cast<float>(windowSize) / baseWidth;
        for(int y = 0; y <= height - windowSize; y += 3) {
            for(int x = 0; x <= width - windowSize; x += 3) {
                totalWindows++;
                bool passedAllStages = true;
                for(const auto& stage : stages) {
                    if(!stage.classify(integral, x, y, scale)) {
                        passedAllStages = false;
                        break;
                    }
                }
                if(passedAllStages) {
                    faces.push_back(Rect(x, y, windowSize, windowSize));
                    passedStages++;
                }
            }
        }
    }

    std::wcout << L"**Processed " << totalWindows << " windows, found " 
               << faces.size() << " faces before NMS" << std::endl;

    faces = nonMaximumSuppression(faces, 0.3f);

    std::wcout << L"HaarCascade: " << faces.size() << " faces after NMS" << std::endl;
    return faces;
}