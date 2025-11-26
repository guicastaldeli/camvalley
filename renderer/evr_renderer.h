class CaptureController;
class WindowManager;
class EVRRenderer {
    public:
        CaptureController& pCaptureController;
        WindowManager& pWindowManager;
        bool setEVR();

        EVRRenderer(
            CaptureController* pCaptureController,
            WindowManager* pWindowManager
        );
};