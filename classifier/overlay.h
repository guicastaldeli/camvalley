#include <d3dx9.h>
#include <iostream>
#include <shlwapi.h>
#include <mfapi.h>
#include <mfidl.h>
#include "../evr_presenter/evr_presenter.h"

class Overlay {
    public:
        EVRPresenter* pPresenter;
        Overlay(EVRPresenter* pPresenter);

        HRESULT Draw();
        void drawRectangle(int x, int y, int width, int height);
};