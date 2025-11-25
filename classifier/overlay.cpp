#include "overlay.h"

Overlay::Overlay(EVRPresenter* pPresenter) : 
    pPresenter(pPresenter) {}
    
/*
** Draw Overlay
*/
HRESULT Overlay::Draw() {
    if(!pPresenter->m_pDevice || pPresenter->m_currentFaces.empty()) {
        return S_OK;
    }

    HRESULT hr = S_OK;
    hr = pPresenter->m_pDevice->BeginScene();
    if(FAILED(hr)) return hr;
    
    pPresenter->m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    pPresenter->m_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    pPresenter->m_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    for(const auto& face : pPresenter->m_currentFaces) {
        drawRectangle(face.x, face.y, face.width, face.height);
    }

    pPresenter->m_pDevice->EndScene();
    pPresenter->m_pDevice->Present(NULL, NULL, NULL, NULL);
    return hr;
}

void Overlay::drawRectangle(int x, int y, int width, int height) {
    if(!pPresenter->m_pLine) return;
    D3DXVECTOR2 points[5] = {
        D3DXVECTOR2((float)x, (float)y),
        D3DXVECTOR2((float)(x + width), (float)y),
        D3DXVECTOR2((float)(x + width), (float)(y + height)),
        D3DXVECTOR2((float)x, (float)(y + height)),
        D3DXVECTOR2((float)x, (float)y)
    };
    pPresenter->m_pLine->SetWidth(3.0f);
    pPresenter->m_pLine->SetAntialias(TRUE);
    pPresenter->m_pLine->Begin();
    pPresenter->m_pLine->Draw(points, 5, D3DCOLOR_XRGB(0, 255, 0));
    pPresenter->m_pLine->End();
}
