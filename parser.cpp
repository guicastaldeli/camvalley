#include "classifier/classifier.h"
#include "classifier/haar_cascade.h"
#include "loader.h"
#include "parser.h"
#include <iostream>
#include <algorithm>
#include <regex>

/*
** Parse Stage
*/
bool Parser::parseStage(
    std::ifstream& file,
    StrongClassifier& stage,
    int& stageThreshold
) {
    std::string line;
    std::vector<WeakClassifier> wc;

    while(std::getline(file, line)) {
        std::string trimmed = trim(line);
        if(trimmed.find("<stageThreshold>") != std::string::npos) {
            stageThreshold = std::stof(getTagContent(trimmed, "stageThreshold"));
        } else if(trimmed.find("<trees>") != std::string::npos) {
            if(!parseTree(file, wc)) {
                return false;
            }
        } else if(trimmed.find("</stage>") != std::string::npos) {
            break;
        }
    }

    for(auto& w : wc) stage.addClassifier(w);
    return true;
}

/*
** Parse Tree
*/
bool Parser::parseTree(
    std::ifstream& file,
    std::vector<WeakClassifier>& wcs
) {
    std::string line;
    while(std::getline(file, line)) {
        std::string trimmed = trim(line);
        if(trimmed.find("</trees>") != std::string::npos) {
            break;
        } else if(trimmed.find("<_>") != std::string::npos) {
            Feature dfFeature;
            WeakClassifier wc(dfFeature, 0.0f, 0.0f);
            if(parseNode(file, wc)) {
                wcs.push_back(wc);
            }
        }
    }

    return true;
}

/*
** Parse Node
*/
bool Parser::parseNode(
    std::ifstream& file,
    WeakClassifier& wc
) {
    std::string line;
    Feature feature;
    float threshold = 0;;
    float leftVal = 0;
    float rightVal = 0;
    bool hasLeftVal = false;
    bool hasRightVal = false;
    std::vector<std::string> rectsData;

    while(std::getline(file, line)) {
        std::string trimmed = trim(line);
        if(trimmed.find("<threshold>") != std::string::npos) {
            threshold = std::stof(getTagContent(trimmed, "threshold"));
        } else if(trimmed.find("<left_val>") != std::string::npos) {
            leftVal = std::stof(getTagContent(trimmed, "left_val"));
            hasLeftVal = true;
        } else if(trimmed.find("<right_val>") != std::string::npos) {
            rightVal = std::stof(getTagContent(trimmed, "right_val"));
            hasRightVal = true;
        } else if(trimmed.find("<rects>") != std::string::npos) {
            while(std::getline(file, line)) {
                std::string rectLine = trim(line);
                if(rectLine.find("</rects>") != std::string::npos) break;
                if(rectLine.find("_") != std::string::npos) {
                    rectsData.push_back(rectLine);
                }
            }
        } else if(trimmed.find("</_>") != std::string::npos) {
            break;
        }
    }
    if(!rectsData.empty()) {
        feature = parseFeature(rectsData);
        wc = WeakClassifier(feature, 0.3f, 1.0f);
        wc.feature.threshold = threshold;
        wc.feature.leftVal = hasLeftVal ? leftVal : -1.0f;
        wc.feature.rightVal = hasRightVal ? rightVal : 1.0f;
        return true;
    }

    return false;
}

/*
** Parse Feature
*/
Feature Parser::parseFeature(const std::vector<std::string>& rectsData) {
    Feature feature;
    feature.type = Feature::TWO_HORIZONTAL;
    std::vector<std::tuple<int, int, int, int, float>> rects;

    for(const auto& rectStr : rectsData) {
        std::string content = getTagContent(rectStr, "_");
        auto parts = split(content, ' ');
        if(parts.size() >= 4) {
            int x = std::stoi(parts[0]);
            int y = std::stoi(parts[1]);
            int width = std::stoi(parts[2]);
            int height = std::stoi(parts[3]);
            float weight = 1.0f;
            if(parts.size() > 4) weight = std::stof(parts[4]);
            rects.push_back(std::make_tuple(
                x,
                y,
                width,
                height,
                weight
            ));
        }
    }

    if(rects.size() == 2) {
        auto [x1, y1, w1, h1, weight1] = rects[0];
        auto [x2, y2, w2, h2, weight2] = rects[1];

        if(y1 == y2 && h1 == h2) {
            feature.type = Feature::TWO_HORIZONTAL;
        } else if(x1 == x2 && w1 == w2) {
            feature.type = Feature::TWO_VERTICAL;
        }
    } else if(rects.size() == 3) {
        feature.type = Feature::THREE_HORIZONTAL;
    }
    if(!rects.empty()) {
        auto [x, y, w, h, weight] = rects[0];
        feature.x = x;
        feature.y = y;
        feature.width = w;
        feature.height = h;
    }

    return feature;
}

std::string Parser::trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\n\r");
    size_t end = str.find_last_not_of(" \t\n\r");
    if(start == std::string::npos) return "";
    return str.substr(start, end - start + 1);
}

std::vector<std::string> Parser::split(
    const std::string& str, 
    char delimiter
) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(str);
    while(std::getline(tokenStream, token, delimiter)) {
        if(!token.empty()) {
            tokens.push_back(token);
        }
    }
    return tokens;
}

/*
** Get Tag Content
*/
std::string Parser::getTagContent(
    const std::string& line,
    const std::string& tag
) {
    std::string openTag = "<" + tag + ">";
    std::string closeTag = "</" + tag + ">";

    size_t start = line.find(openTag);
    if(start == std::string::npos) return "";
    start += openTag.length();

    size_t end = line.find(closeTag, start);
    if(end == std::string::npos) return "";

    return line.substr(start, end - start);
}