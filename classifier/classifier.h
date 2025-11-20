#pragma once
#include <vector>
#include <string>
#include <memory>
#include <fstream>
#include "rect.h"
#include "feature.h"

class WeakClassifier {
    public:
        Feature feature;
        float error;
        float weight;

        WeakClassifier(
            const Feature& f,
            float e,
            float w
        ) :
        feature(f),
        error(e),
        weight(w) {}

        bool classify(
            const std::vector<std::vector<float>>& integral,
            float scale
        ) const;
};

class StrongClassifier {
    public:
        std::vector<WeakClassifier> weakClassifiers;
        float threshold;

        bool classify(
            const std::vector<std::vector<float>>& integral,
            int x,
            int y,
            float scale
        ) const;
        void addClassifier(const WeakClassifier& wc) {
            weakClassifiers.push_back(wc);
        }
};