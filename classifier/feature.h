#pragma once
#include <vector>
#include <string>
#include <memory>
#include <fstream>

class Feature {
    public:
        enum Type {
            TWO_HORIZONTAL,
            TWO_VERTICAL,
            THREE_HORIZONTAL,
            THREE_VERTICAL,
            FOUR_SQUARE
        };
        Type type;

        int x;
        int y;
        int width;
        int height;
        float threshold;
        float leftVal;
        float rightVal;
        bool leftRight;
};