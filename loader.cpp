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
    bool inStages = false;
    int stageCount = 0;

    while(std::getline(file, line)) {
        std::string trimmed = Parser::trim(line);

        if(trimmed.find("<stages>") != std::string::npos) {
            inStages = true;
        }
        else if(trimmed.find("</stages>") != std::string::npos) {
            inStages = false;
        }
        else if(inStages && trimmed.find("<_>") != std::string::npos) {
            stageCount++;

            StrongClassifier dummyStage;
            dummyStage.threshold = -1.0f;
            cascade.addStage(dummyStage);
        }
    }

    file.close();
    std::wcout << L"Loaded cascade with " << cascade.stages.size() << L" stages" << std::endl;
    return !cascade.stages.empty();
}