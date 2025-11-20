#pragma once
#include <vector>
#include <string>
#include <memory>
#include <fstream>
#include "classifier.h"

class HaarCascade {
    private:
        std::vector<StrongClassifier> stages;
        int baseWidth;
        int baseHeight;

    public:
        HaarCascade() : 
            baseWidth(24), 
            baseHeight(24) {}

        bool loadFromFile(const std::string& fileName);
        std::vector<Rect> detectFaces(
            const std::vector<std::vector<float>>& integral,
            int minSize = 24,
            int maxSize = 400,
            float scaleFactor = 1.25f
        );
        void addStage(const StrongClassifier& stage) {
            stages.push_back(stage);
        }
};