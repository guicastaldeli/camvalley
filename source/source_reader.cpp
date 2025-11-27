#include "source_reader.h"
#include <iostream>
#include <mfapi.h>
#include <mfidl.h>
#include "../controller/capture_controller.h"

bool SourceReader::createSourceReader(CaptureController* instance, IMFMediaSource* pCaptureSource) {
    if(!pCaptureSource) {
        std::wcout << L"No capture source available" << std::endl;
        return false;
    }

    IMFAttributes* pAttrs = nullptr;
    HRESULT hr = MFCreateAttributes(&pAttrs, 3);
    if(FAILED(hr)) {
        std::wcout << L"Failed to create attributes" << std::endl;
        return false;
    }

    hr = pAttrs->SetUINT32(
        MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING,
        TRUE
    );
    if(FAILED(hr)) {
        std::wcout << L"Failed to enable video" << std::endl;
        pAttrs->Release();
        return false;
    }

    hr = pAttrs->SetUnknown(
        MF_SOURCE_READER_ASYNC_CALLBACK,
        instance
    );
    if(FAILED(hr)) {
        std::wcout << L"Failed to set callback" << std::endl;
        pAttrs->Release();
        return false;
    }

    hr = MFCreateSourceReaderFromMediaSource(
        pCaptureSource,
        pAttrs,
        &pReader
    );
    pAttrs->Release();
    if(FAILED(hr)) {
        std::wcout << L"Failed to create source reader: " << hr << std::endl;
        return false;
    }

    IMFMediaType* pMediaType = nullptr;
    IMFMediaType* pHighestResolutionType = nullptr;
    UINT32 highestPixelCount = 0;
    UINT32 highestWidth = 0;
    UINT32 highestHeight = 0;
    for (DWORD i = 0; ; i++) {
        hr = pReader->GetNativeMediaType(
            MF_SOURCE_READER_FIRST_VIDEO_STREAM,
            i,
            &pMediaType
        );
        
        if(FAILED(hr)) break;
        
        UINT32 width = 0; 
        UINT32 height = 0;
        hr = MFGetAttributeSize(
            pMediaType, 
            MF_MT_FRAME_SIZE, 
            &width, 
            &height
        );
        if(SUCCEEDED(hr)) {
            GUID subtype = GUID_NULL;
            pMediaType->GetGUID(MF_MT_SUBTYPE, &subtype);
            
            if(subtype == MFVideoFormat_RGB32) {
                UINT32 pixelCount = width * height;
                if(pixelCount > highestPixelCount) {
                    highestPixelCount = pixelCount;
                    highestWidth = width;
                    highestHeight = height;
                    
                    if(pHighestResolutionType) pHighestResolutionType->Release();
                    pHighestResolutionType = pMediaType;
                    pHighestResolutionType->AddRef();
                }
            }
        }
        pMediaType->Release();
    }
    if(!pHighestResolutionType) {
        std::wcout << L"No RGB32 using YUY2..." << std::endl;
        for (DWORD i = 0; ; i++) {
            hr = pReader->GetNativeMediaType(
                MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                i,
                &pMediaType
            );
            
            if(FAILED(hr)) break;
            UINT32 width = 0, height = 0;
            hr = MFGetAttributeSize(
                pMediaType, 
                MF_MT_FRAME_SIZE, 
                &width, 
                &height
            );
            if(SUCCEEDED(hr)) {
                GUID subtype = GUID_NULL;
                pMediaType->GetGUID(MF_MT_SUBTYPE, &subtype);
                if(subtype == MFVideoFormat_YUY2) {
                    UINT32 pixelCount = width * height;
                    if(pixelCount > highestPixelCount) {
                        highestPixelCount = pixelCount;
                        highestWidth = width;
                        highestHeight = height;

                        if(pHighestResolutionType) pHighestResolutionType->Release();
                        pHighestResolutionType = pMediaType;
                        pHighestResolutionType->AddRef();
                    }
                }
            }
            pMediaType->Release();
        }
    }
    if(pHighestResolutionType) {
        GUID finalSubtype = GUID_NULL;
        pHighestResolutionType->GetGUID(MF_MT_SUBTYPE, &finalSubtype);
        
        std::wcout << L"Setting resolution: " << highestWidth 
                  << L"x" << highestHeight 
                  << L" Format: " << (finalSubtype == MFVideoFormat_RGB32 ? L"RGB32" : L"YUY2")
                  << std::endl;
        
        hr = pReader->SetCurrentMediaType(
            MF_SOURCE_READER_FIRST_VIDEO_STREAM,
            nullptr,
            pHighestResolutionType
        );
        pHighestResolutionType->Release();
        
        if(SUCCEEDED(hr)) {
            hr = pReader->ReadSample(
                MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                0,
                nullptr,
                nullptr,
                nullptr,
                nullptr
            );
            return SUCCEEDED(hr);
        }
    }
    
    hr = MFCreateMediaType(&pMediaType);
    bool success = false;
    if(SUCCEEDED(hr)) {
        hr = pMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        hr = pMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
        hr = MFSetAttributeSize(pMediaType, MF_MT_FRAME_SIZE, 640, 480);
        
        if(SUCCEEDED(hr)) {
            hr = pReader->SetCurrentMediaType(
                MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                nullptr,
                pMediaType
            );
            success = SUCCEEDED(hr);
        }
        pMediaType->Release();
        
        if(success) {
            hr = pReader->ReadSample(
                MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                0,
                nullptr,
                nullptr,
                nullptr,
                nullptr
            );
        }
    }
    
    if(!success)std::wcout << L"Failed to set any media type for source reader: " << hr << std::endl;
    return success;
}

bool SourceReader::createSourceReaderWithRes(
    CaptureController* instance,
    IMFMediaSource* pCaptureSource, 
    UINT32 width, 
    UINT32 height
) {
    if(!pCaptureSource) {
        std::wcout << L"No capture source available" << std::endl;
        return false;
    }

    IMFAttributes* pAttrs = nullptr;
    HRESULT hr = MFCreateAttributes(&pAttrs, 3);
    if(FAILED(hr)) {
        std::wcout << L"Failed to create attributes" << std::endl;
        return false;
    }

    hr = pAttrs->SetUINT32(
        MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING,
        TRUE
    );
    if(FAILED(hr)) {
        std::wcout << L"Failed to enable video" << std::endl;
        pAttrs->Release();
        return false;
    }

    hr = pAttrs->SetUnknown(
        MF_SOURCE_READER_ASYNC_CALLBACK,
        instance
    );
    if(FAILED(hr)) {
        std::wcout << L"Failed to set callback" << std::endl;
        pAttrs->Release();
        return false;
    }

    hr = MFCreateSourceReaderFromMediaSource(
        pCaptureSource,
        pAttrs,
        &pReader
    );
    pAttrs->Release();
    if(FAILED(hr)) {
        std::wcout << L"Failed to create source reader: " << hr << std::endl;
        return false;
    }

    bool resolutionSet = false;
    IMFMediaType* pMediaType = nullptr;
    for (DWORD i = 0; ; i++) {
        hr = pReader->GetNativeMediaType(
            MF_SOURCE_READER_FIRST_VIDEO_STREAM,
            i,
            &pMediaType
        );
        if(FAILED(hr)) break;
        
        UINT32 mediaWidth;
        UINT32 mediaHeight;
        GUID subtype;
        if(
            SUCCEEDED(MFGetAttributeSize(pMediaType, MF_MT_FRAME_SIZE, &mediaWidth, &mediaHeight)) &&
            SUCCEEDED(pMediaType->GetGUID(MF_MT_SUBTYPE, &subtype))
        ) {
            if(
                mediaWidth == width && 
                mediaHeight == height && 
                (subtype == MFVideoFormat_YUY2 || subtype == MFVideoFormat_RGB32)
            ) {
                
                std::wcout << L"Found matching resolution: " << width << L"x" << height 
                          << L" Format: " << (subtype == MFVideoFormat_YUY2 ? L"YUY2" : L"RGB32") << std::endl;
                
                hr = pReader->SetCurrentMediaType(
                    MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                    nullptr,
                    pMediaType
                );
                
                if(SUCCEEDED(hr)) {
                    resolutionSet = true;
                    std::wcout << L"Successfully set resolution to " << width << L"x" << height << std::endl;
                    pMediaType->Release();
                    break;
                }
            }
        }
        pMediaType->Release();
    }
    
    if(!resolutionSet) {
        UINT32 bestWidth = 0; 
        UINT32 bestHeight = 0;
        IMFMediaType* bestType = nullptr;
        
        for (DWORD i = 0; ; i++) {
            hr = pReader->GetNativeMediaType(
                MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                i,
                &pMediaType
            );
            if(FAILED(hr)) break;
            
            UINT32 mediaWidth; 
            UINT32 mediaHeight;
            GUID subtype;
            if(
                SUCCEEDED(MFGetAttributeSize(pMediaType, MF_MT_FRAME_SIZE, &mediaWidth, &mediaHeight)) &&
                SUCCEEDED(pMediaType->GetGUID(MF_MT_SUBTYPE, &subtype)) &&
                (subtype == MFVideoFormat_YUY2 || subtype == MFVideoFormat_RGB32)
            ) {
                if(mediaWidth * mediaHeight > bestWidth * bestHeight) {
                    if(bestType) bestType->Release();
                    bestType = pMediaType;
                    bestType->AddRef();
                    bestWidth = mediaWidth;
                    bestHeight = mediaHeight;
                }
            }
            pMediaType->Release();
        }
        
        if(bestType) {
            hr = pReader->SetCurrentMediaType(
                MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                nullptr,
                bestType
            );
            
            if(SUCCEEDED(hr)) {
                std::wcout << L"Set to highest available resolution: " << bestWidth << L"x" << bestHeight << std::endl;
                resolutionSet = true;
            }
            bestType->Release();
        }
    }

    hr = pReader->ReadSample(
        MF_SOURCE_READER_FIRST_VIDEO_STREAM,
        0,
        nullptr,
        nullptr,
        nullptr,
        nullptr
    );
    if(SUCCEEDED(hr)) {
        std::wcout << L"Source reader configured successfully!" << std::endl;
    } else {
        std::wcout << L"Failed to start reading: " << hr << std::endl;
        return false;
    }
    return SUCCEEDED(hr);
}
