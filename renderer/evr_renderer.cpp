#include "evr_renderer.h"
#include "../controller/capture_controller.h"
#include "../window_manager.h"
#include <Windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <shlwapi.h>
#include <evr.h>
#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <atomic>
#include <iostream>

EVRRenderer::EVRRenderer(CaptureController* pCaptureController, WindowManager* pWindowManager) :
    pCaptureController(*pCaptureController),
    pWindowManager(*pWindowManager) {}

bool EVRRenderer::setEVR() {
    HRESULT hr = S_OK;
    IMFActivate* pSinkActivate = NULL;
    IMFTopology* pTopology = NULL;
    IMFTopologyNode* pSourceNode = NULL;
    IMFTopologyNode* pOutputNode = NULL;
    IMFStreamDescriptor* pSourceSD = NULL;
    IMFPresentationDescriptor* pPD = NULL;
    IMFMediaTypeHandler* pHandler = NULL;
    IMFMediaEvent* pEvent = NULL;
    DWORD streamCount = 0;
    BOOL fSelected = FALSE;
    DWORD videoStreamIndex = (DWORD)-1;
    MediaEventType eventType;
    std::wcout << L"Setting EVR pipeline" << std::endl;

    std::wcout << L"Setting EVR pipeline with window: " << pWindowManager.hwndVideo << std::endl;
    hr = MFCreateVideoRendererActivate(pWindowManager.hwndVideo, &pSinkActivate);
    if(FAILED(hr)) {
        std::wcout << L"Failed to create video renderer activate: " << hr << std::endl;
        goto done;
    }

    hr = MFCreateTopology(&pTopology);
    if(FAILED(hr)) {
        std::wcout << L"Failed to create topology: " << hr << std::endl;
        goto done;
    }

    hr = pCaptureController.pVideoSource->CreatePresentationDescriptor(&pPD);
    if(FAILED(hr)) {
        std::wcout << L"Failed to create presentatin descriptor: " << hr << std::endl;
        goto done;
    }

    hr = pPD->GetStreamDescriptorCount(&streamCount);
    if(FAILED(hr) || streamCount == 0) {
        std::wcout << L"No streams available: " << hr << std::endl;
        goto done;
    }
    for(DWORD i = 0; i < streamCount; i++) {
        hr = pPD->GetStreamDescriptorByIndex(i, &fSelected, &pSourceSD);
        if(SUCCEEDED(hr) && pSourceSD) {
            hr = pSourceSD->GetMediaTypeHandler(&pHandler);
            if(SUCCEEDED(hr)) {
                GUID majorType = GUID_NULL;
                hr = pHandler->GetMajorType(&majorType);
                if(SUCCEEDED(hr) && majorType == MFMediaType_Video) {
                    videoStreamIndex = i;
                    std::wcout << L"Found video stream at index: " << i << std::endl;
                    hr = pCaptureController.configFormat(pHandler);
                    break;
                }
            }
            if(pHandler) {
                pHandler->Release();
                pHandler = NULL;
            }
            if(pSourceSD) {
                pSourceSD->Release();
                pSourceSD = NULL;
            }
        }
    }
    if(videoStreamIndex == (DWORD)-1) {
        std::wcout << L"No video stream found!" << std::endl;
        hr = E_FAIL;
        goto done;
    }
    if(!pSourceSD) {
        hr = pPD->GetStreamDescriptorByIndex(videoStreamIndex, &fSelected, &pSourceSD);
        if(FAILED(hr)) {
            std::wcout << L"Failed to get video stream descriptor: " << hr << std::endl;
            goto done;
        }
    }

    hr = MFCreateTopologyNode(
        MF_TOPOLOGY_SOURCESTREAM_NODE, 
        &pSourceNode
    );
    if(FAILED(hr)) {
        std::wcout << L"Failed to create source node: " << hr << std::endl;
        goto done;
    }
    hr = pSourceNode->SetUnknown(MF_TOPONODE_SOURCE, pCaptureController.pVideoSource);
    if(FAILED(hr)) {
        std::wcout << L"Failed to set source: " << hr << std::endl;
        goto done;
    }
    hr = pSourceNode->SetUnknown(MF_TOPONODE_PRESENTATION_DESCRIPTOR, pPD);
    if(FAILED(hr)) {
        std::wcout << L"Failed to set presentation descriptor: " << hr << std::endl;
        goto done;
    }
    hr = pSourceNode->SetUnknown(MF_TOPONODE_STREAM_DESCRIPTOR, pSourceSD);
    if(FAILED(hr)) {
        std::wcout << L"Failed to set stream descriptor: " << hr << std::endl;
        goto done;
    }

    hr = MFCreateTopologyNode(
        MF_TOPOLOGY_OUTPUT_NODE,
        &pOutputNode
    );
    if(FAILED(hr)) {
        std::wcout << L"Failed to create output node: " << hr << std::endl;
        goto done;
    }
    hr = pOutputNode->SetObject(pSinkActivate);
    if(FAILED(hr)) {
        std::wcout << L"Failed to set sink activate: " << hr << std::endl;
        goto done;
    }
    hr = pOutputNode->SetUINT32(MF_TOPONODE_STREAMID, 0);
    if(FAILED(hr)) {
        std::wcout << L"Failed to set stream ID: " << hr << std::endl;
        goto done;
    }
    hr = pOutputNode->SetUINT32(MF_TOPONODE_NOSHUTDOWN_ON_REMOVE, FALSE);
    if(FAILED(hr)) {
        std::wcout << L"Failed to set no shutdown: " << hr << std::endl;
        goto done;
    }

    hr = pTopology->AddNode(pSourceNode);
    if(FAILED(hr)) {
        std::wcout << L"Failed to add source node: " << hr << std::endl;
        goto done;
    }
    hr = pTopology->AddNode(pOutputNode);
    if(FAILED(hr)) {
        std::wcout << L"Failed to add output node: " << hr << std::endl;
        goto done;
    }
    hr = pSourceNode->ConnectOutput(0, pOutputNode, 0);
    if(FAILED(hr)) {
        std::wcout << L"Failed to connect nodes: " << hr << std::endl;
        goto done;
    }

    hr = MFCreateMediaSession(NULL, &pCaptureController.pSession);
    if(FAILED(hr)) {
        std::wcout << L"Failed to create media session: " << hr << std::endl;
        goto done;
    }
    hr = pCaptureController.pSession->SetTopology(
        MFSESSION_SETTOPOLOGY_IMMEDIATE, 
        pTopology
    );
    if(FAILED(hr)) {
        std::wcout << L"Failed to set topology: " << hr << std::endl;
        goto done;
    }

    hr = pCaptureController.pSession->GetEvent(0, &pEvent);
    if(SUCCEEDED(hr)) {
        hr = pEvent->GetType(&eventType);
        if(SUCCEEDED(hr)) {
            std::wcout << L"First media event type: " << eventType << std::endl;
        }
        pEvent->Release();
    }

    PROPVARIANT varStart;
    PropVariantInit(&varStart);
    varStart.vt = VT_EMPTY;
    hr = pCaptureController.pSession->Start(&GUID_NULL, &varStart);
    if(FAILED(hr)) {
        std::cout << L"Failed to start sesison: " << hr << std::endl;
        hr = pCaptureController.pSession->Start(NULL, NULL);
        if(FAILED(hr)) {
            std::wcout << L"Alternative start failed: " << hr << std::endl;
            goto done;
        }
    }

    pCaptureController.isRunning = true;
    std::wcout << L"EVR setup complete!" << std::endl;
    done:
        if(pSourceNode) pSourceNode->Release();
        if(pOutputNode) pOutputNode->Release();
        if(pTopology) pTopology->Release();
        if(pSinkActivate) pSinkActivate->Release();
        if(pSourceSD) pSourceSD->Release();
        if(pPD) pPD->Release();
        if(pHandler) pHandler->Release();
        if(pEvent) pEvent->Release();

        return SUCCEEDED(hr);
}