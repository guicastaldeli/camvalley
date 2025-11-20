#pragma once
#include "classifier/classifier.h"
#include "classifier/haar_cascade.h"
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <map>

class Loader {
    public:
        static bool loadFile(
            const std::string& name,
            HaarCascade& cascade
        );
};