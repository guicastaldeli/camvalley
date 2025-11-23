#pragma once
#include "classifier/classifier.h"
#include "classifier/haar_cascade.h"
#include "classifier/feature.h"
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <map>

class Parser {
    public:
        static bool parseStage(
            std::ifstream& file,
            StrongClassifier& stage,
            int& stageThreshold
        );
        static bool parseTree(
            std::ifstream& file,
            std::vector<WeakClassifier>& wc
        );
        static bool parseNode(
            std::ifstream& file,
            WeakClassifier& wc
        );
        static Feature parseFeature(const std::vector<std::string>& rectsData);
        static std::string trim(const std::string& str);
        static std::vector<std::string> split(
            const std::string& str,
            char delimiter
        );
        static bool readUntil(
            std::ifstream& file,
            const std::string& target
        );
        static std::string getTagContent(
            const std::string& line,
            const std::string& tag
        );
        static Feature createFeatureFromIndex(int featureIndex);
};