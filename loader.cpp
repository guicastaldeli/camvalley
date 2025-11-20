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
    int width = 24;
    int height = 24;

    while(std::getline(file, line)) {
        std::string trimmed = Parser::trim(line);

        if(trimmed.find("<width>") != std::string::npos) {
            width = std::stoi(Parser::getTagContent(trimmed, "width"));
        } else if(trimmed.find("<height>") != std::string::npos) {
            height = std::stoi(Parser::getTagContent(trimmed, "height"));
        } else if(trimmed.find("<stage>") != std::string::npos) {
            StrongClassifier stage;
            int stageThreshold = 0;
            if(Parser::parseStage(file, stage, stageThreshold)) {
                stage.threshold = stageThreshold;
                cascade.addStage(stage);
            }
        }
    }

    file.close();
    std::wcout << L"Loaded cascade with " << cascade.stages.size() << L" stages" << std::endl;
    return !cascade.stages.empty();
}