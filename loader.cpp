#include "loader.h"
#include "classifier/classifier.h"
#include "classifier/haar_cascade.h"
#include <iostream>
#include <algorithm>
#include <regex>
#include "parser.h"

bool Loader::loadFile(
    const std::string& name,
    HaarCascade& cascade
) {
    std::ifstream file(name);
    if(!file.is_open()) {
        std::wcout << L"Failed to open file: " << name.c_str() << std::endl;
        return false;
    }

    std::string line;
    int baseWidth = 24, baseHeight = 24;
    file.clear();
    file.seekg(0);
    
    while(std::getline(file, line)) {
        std::string trimmed = Parser::trim(line);
        if(trimmed.find("<width>") != std::string::npos) {
            baseWidth = std::stoi(Parser::getTagContent(trimmed, "width"));
            std::wcout << L"Found base width: " << baseWidth << std::endl;
        } else if(trimmed.find("<height>") != std::string::npos) {
            baseHeight = std::stoi(Parser::getTagContent(trimmed, "height"));
            std::wcout << L"Found base height: " << baseHeight << std::endl;
        } else if(trimmed.find("<stages>") != std::string::npos) {
            break;
        }
    }
    
    cascade.baseWidth = baseWidth;
    cascade.baseHeight = baseHeight;
    file.clear();
    file.seekg(0);
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string fileContent = buffer.str();
    file.close();

    size_t stagesStart = fileContent.find("<stages>");
    size_t stagesEnd = fileContent.find("</stages>");
    
    if(stagesStart == std::string::npos || stagesEnd == std::string::npos) {
        std::wcout << L"ERROR: Could not find stages section!" << std::endl;
        return false;
    }

    std::string stagesContent = fileContent.substr(stagesStart, stagesEnd - stagesStart + 9);
    std::wcout << L"Found stages section, length: " << stagesContent.length() << L" characters" << std::endl;

    std::istringstream stagesStream(stagesContent);
    int stageCount = 0;
    std::string stageLine;
    std::getline(stagesStream, stageLine);

    while(std::getline(stagesStream, stageLine)) {
        std::string trimmed = Parser::trim(stageLine);
        
        if(trimmed.find("</stages>") != std::string::npos) {
            break;
        }
        else if(trimmed.find("<_>") != std::string::npos) {
            stageCount++;
            std::wcout << L"=== PARSING STAGE " << stageCount << L" ===" << std::endl;
            
            std::string stageContent = trimmed + "\n";
            int braceCount = 1;
            
            while(std::getline(stagesStream, stageLine) && braceCount > 0) {
                std::string lineTrimmed = Parser::trim(stageLine);
                stageContent += stageLine + "\n";
                
                if(lineTrimmed.find("<_>") != std::string::npos) {
                    braceCount++;
                } else if(lineTrimmed.find("</_>") != std::string::npos) {
                    braceCount--;
                } else if(lineTrimmed.find("</stages>") != std::string::npos) {
                    break;
                }
            }

            std::wcout << L"Stage content length: " << stageContent.length() << L" characters" << std::endl;
            
            std::istringstream stageStream(stageContent);
            StrongClassifier stage;
            int stageThreshold = 0;
            
            std::string tempFileName = "temp_stage_" + std::to_string(stageCount) + ".xml";
            std::ofstream tempFile(tempFileName);
            tempFile << stageContent;
            tempFile.close();
            
            std::ifstream fileStream(tempFileName);
            if(Parser::parseStage(fileStream, stage, stageThreshold)) {
                stage.threshold = stageThreshold;
                cascade.addStage(stage);
                std::wcout << L"SUCCESS: Stage " << stageCount << L" has " 
                          << stage.weakClassifiers.size() << L" weak classifiers" << std::endl;
            } else {
                std::wcout << L"FAILED: Could not parse stage " << stageCount << std::endl;
            }
    
            remove(tempFileName.c_str());
            
            if(stageCount >= 10) {
                std::wcout << L"DEBUG: Parsed first 10 stages" << std::endl;
                break;
            }
        }
    }

    std::wcout << L"=== FINAL LOADING RESULT ===" << std::endl;
    std::wcout << L"Successfully loaded " << cascade.stages.size() << L" stages" << std::endl;
    
    if(!cascade.stages.empty()) {
        std::wcout << L"First stage has " << cascade.stages[0].weakClassifiers.size() 
                   << L" weak classifiers" << std::endl;
        
        if(!cascade.stages[0].weakClassifiers.empty()) {
            std::wcout << L"First weak classifier details:" << std::endl;
            std::wcout << L" - threshold: " << cascade.stages[0].weakClassifiers[0].feature.threshold << std::endl;
            std::wcout << L" - leftVal: " << cascade.stages[0].weakClassifiers[0].feature.leftVal << std::endl;
            std::wcout << L" - rightVal: " << cascade.stages[0].weakClassifiers[0].feature.rightVal << std::endl;
            std::wcout << L" - type: " << cascade.stages[0].weakClassifiers[0].feature.type << std::endl;
        }
        
        size_t totalWeakClassifiers = 0;
        for (const auto& stage : cascade.stages) {
            totalWeakClassifiers += stage.weakClassifiers.size();
        }
        std::wcout << L"Total weak classifiers across all stages: " << totalWeakClassifiers << std::endl;
    } else {
        std::wcout << L"CRITICAL: No stages were loaded!" << std::endl;
        return false;
    }
    
    return !cascade.stages.empty();
}