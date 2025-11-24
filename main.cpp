#include "window_manager.h"
#include <iostream>

class Main {
    private: WindowManager windowManager;

    public: Main() {
        std::wcout << L"Main app init" << std::endl;
    }

    public: int run() {
        windowManager.run();
        return 0;
    }
};

int main() {
    try {
        Main app;
        return app.run();
    } catch(const std::exception& err) {
        std::wcout << L"Exception " << err.what() << std::endl;
        return -1;
    }
}