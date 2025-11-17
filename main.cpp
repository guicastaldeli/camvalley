#include "window.h"

class Main {
    private: Window window;

    public: int run() {
        window.run();
        return 0;
    }
};

int main() {
    Main app;
    return app.run();
}