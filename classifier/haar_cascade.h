#pragma once
#include <vector>
#include <string>
#include <memory>
#include <fstream>
#include <iostream>
#include "classifier.h"

class HaarCascade {
    public:
        std::vector<StrongClassifier> stages;
        int baseWidth;
        int baseHeight;
        bool loaded;

        HaarCascade() : 
            baseWidth(24), 
            baseHeight(24),
            loaded(false) {}

        std::vector<Rect> detectFaces(
            const std::vector<std::vector<float>>& integral,
            int minSize = 24,
            int maxSize = 400,
            float scaleFactor = 1.25f
        );
        std::vector<Rect> nonMaximumSuppression(
            const std::vector<Rect>& faces,
            float overlapThreshold = 0.3f
        );
        void addStage(const StrongClassifier& stage) {
            stages.push_back(stage);
            loaded = !stages.empty();
        }
        
        bool isLoaded() const {
            bool result = loaded && !stages.empty();
            return result;
        }
        void clear() {
            stages.clear();
            loaded = false;
        }
};