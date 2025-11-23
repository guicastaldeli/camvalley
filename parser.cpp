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
    bool inThisStage = false;

    std::wcout << L"  parseStage: Starting to parse stage..." << std::endl;

    while(std::getline(file, line)) {
        std::string trimmed = trim(line);
        
        if(trimmed.find("<_>") != std::string::npos) {
            inThisStage = true;
            std::wcout << L"  parseStage: Found stage start" << std::endl;
            continue;
        }
        
        if(!inThisStage) continue;

        if(trimmed.find("<stageThreshold>") != std::string::npos) {
            stageThreshold = std::stof(getTagContent(trimmed, "stageThreshold"));
            std::wcout << L"  parseStage: Found stageThreshold: " << stageThreshold << std::endl;
        } else if(trimmed.find("<weakClassifiers>") != std::string::npos) {
            std::wcout << L"  parseStage: Found <weakClassifiers> - calling parseTree..." << std::endl;
            if(!parseTree(file, wc)) {
                std::wcout << L"  parseStage: parseTree failed" << std::endl;
                return false;
            }
            std::wcout << L"  parseStage: parseTree completed, got " << wc.size() << L" weak classifiers" << std::endl;
        } else if(trimmed.find("</_>") != std::string::npos) {
            std::wcout << L"  parseStage: Found stage end" << std::endl;
            break;
        } else if(trimmed.find("</stages>") != std::string::npos) {
            std::wcout << L"  parseStage: Unexpected end of stages section" << std::endl;
            break;
        }
    }

    for(auto& w : wc) {
        stage.addClassifier(w);
    }
    
    std::wcout << L"  parseStage: Completed with " << wc.size() << L" weak classifiers" << std::endl;
    return !wc.empty();
}

/*
** Parse Tree
*/
bool Parser::parseTree(
    std::ifstream& file,
    std::vector<WeakClassifier>& wcs
) {
    std::string line;
    int treeCount = 0;
    
    std::wcout << L"  parseTree: Starting to parse trees..." << std::endl;

    while(std::getline(file, line)) {
        std::string trimmed = trim(line);
        
        std::wcout << L"  parseTree line: " << trimmed.c_str() << std::endl;

        if(trimmed.find("</weakClassifiers>") != std::string::npos) {
            std::wcout << L"  parseTree: End of weakClassifiers section" << std::endl;
            break;
        } 
        else if(trimmed.find("<_>") != std::string::npos) {
            treeCount++;
            std::wcout << L"  parseTree: Found weak classifier #" << treeCount << std::endl;
            
            Feature feature;
            WeakClassifier wc(feature, 0.0f, 1.0f);
            
            if(parseNode(file, wc)) {
                wcs.push_back(wc);
                std::wcout << L"  parseTree: SUCCESS - added weak classifier " << treeCount << std::endl;
            } else {
                std::wcout << L"  parseTree: FAILED - could not parse weak classifier " << treeCount << std::endl;
            }
        }
    }

    std::wcout << L"  parseTree: Completed with " << wcs.size() << L" weak classifiers" << std::endl;
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
    float threshold = 0;
    float leftVal = 0;
    float rightVal = 0;
    
    std::wcout << L"    parseNode: Starting OpenCV format parsing..." << std::endl;

    while(std::getline(file, line)) {
        std::string trimmed = trim(line);
        
        std::wcout << L"    parseNode line: " << trimmed.c_str() << std::endl;

        if(trimmed.find("<internalNodes>") != std::string::npos) {
            if(std::getline(file, line)) {
                std::string dataLine = trim(line);
                std::wcout << L"    internalNodes data: " << dataLine.c_str() << std::endl;
                size_t closePos = dataLine.find("</internalNodes>");
                if(closePos != std::string::npos) {
                    std::string nodesContent = dataLine.substr(0, closePos);
                    std::wcout << L"    Found internalNodes content: " << nodesContent.c_str() << std::endl;
                    auto parts = split(nodesContent, ' ');
                    if(parts.size() >= 4) {
                        int featureIndex = std::stoi(parts[2]);
                        threshold = std::stof(parts[3]);
                        feature = createFeatureFromIndex(featureIndex);
                        std::wcout << L"    Parsed - featureIndex: " << featureIndex 
                                  << L", threshold: " << threshold << std::endl;
                    }
                }
            }
        }
        else if(trimmed.find("<leafValues>") != std::string::npos) {
            if(std::getline(file, line)) {
                std::string dataLine = trim(line);
                std::wcout << L"    leafValues data: " << dataLine.c_str() << std::endl;
                size_t closePos = dataLine.find("</leafValues>");
                if(closePos != std::string::npos) {
                    std::string leafContent = dataLine.substr(0, closePos);
                    std::wcout << L"    Found leafValues content: " << leafContent.c_str() << std::endl;
                    
                    auto parts = split(leafContent, ' ');
                    if(parts.size() >= 2) {
                        leftVal = std::stof(parts[0]);
                        rightVal = std::stof(parts[1]);
                        std::wcout << L"    Parsed - leftVal: " << leftVal 
                                  << L", rightVal: " << rightVal << std::endl;
                    }
                }
            }
        }
        else if(trimmed.find("</_>") != std::string::npos) {
            std::wcout << L"    End of node" << std::endl;
            break;
        }
    }
    if(threshold != 0) {
        wc = WeakClassifier(feature, 0.0f, 1.0f);
        wc.feature.threshold = threshold;
        wc.feature.leftVal = leftVal;
        wc.feature.rightVal = rightVal;
        wc.feature.leftRight = true;
        
        std::wcout << L"    SUCCESS: Created weak classifier with threshold=" 
                  << threshold << L", leftVal=" << leftVal << L", rightVal=" << rightVal << std::endl;
        return true;
    }

    std::wcout << L"    FAILED: Could not parse node data" << std::endl;
    return false;
}

Feature Parser::createFeatureFromIndex(int featureIndex) {
    Feature feature;
    switch(featureIndex % 5) {
        case 0: 
            feature.type = Feature::TWO_HORIZONTAL;
            feature.x = 1; feature.y = 1; feature.width = 16; feature.height = 8;
            break;
        case 1:
            feature.type = Feature::TWO_VERTICAL;
            feature.x = 1; feature.y = 1; feature.width = 8; feature.height = 16;
            break;
        case 2:
            feature.type = Feature::THREE_HORIZONTAL;
            feature.x = 1; feature.y = 1; feature.width = 18; feature.height = 6;
            break;
        case 3:
            feature.type = Feature::THREE_VERTICAL;
            feature.x = 1; feature.y = 1; feature.width = 6; feature.height = 18;
            break;
        case 4:
            feature.type = Feature::FOUR_SQUARE;
            feature.x = 1; feature.y = 1; feature.width = 12; feature.height = 12;
            break;
    }
    
    std::wcout << L"    Created feature: type=" << feature.type 
              << L", x=" << feature.x << L", y=" << feature.y
              << L", width=" << feature.width << L", height=" << feature.height << std::endl;
    
    return feature;
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