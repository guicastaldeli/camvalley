#pragma once
#include <vector>
#include <string>
#include <memory>
#include <fstream>
#include "classifier.h"

class HaarCascade {
    public:
        std::vector<StrongClassifier> stages;
        int baseWidth;
        int baseHeight;
        bool loaded;

        HaarCascade() : 
            baseWidth(24), 
            baseHeight(24) {}

        std::vector<Rect> detectFaces(
            const std::vector<std::vector<float>>& integral,
            int minSize = 24,
            int maxSize = 400,
            float scaleFactor = 1.25f
        );
        void addStage(const StrongClassifier& stage) {
            stages.push_back(stage);
        }
        
        bool isLoaded() const {
            return loaded && !stages.empty();
        }
        void clear() {
            stages.clear();
            loaded = false;
        }
};