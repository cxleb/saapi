#include "saapi.h"

#define didfail(msg) if(hr != S_OK) {printf("WINDOWS ERROR: %s! %d\n", msg, hr); exit(0);}

#define ERROR_

namespace SAAPI 
{
    ////////
    //////// AUDIO RENDERER
    ////////
    
    DWORD WINAPI AudioRenderThreadProc(LPVOID Param)
    {
        AudioRenderer* renderer = reinterpret_cast<AudioRenderer*>(Param);
        renderer->threadProc();
        return 0;
    }
    
    void AudioRenderer::threadProc()
    {
        // when start is called, a thread spawns, and this proc starts
        // this proc will get the internal audio buffer, gather samples buffer, 
        // format it, and write it to the internal buffer

        while(1)
        {
            if(!running)
            {
                printf("we not running fucking idiot\n");
                return; // ends the thread which means we stopped the audio
            }

            // wait for "buffer to be ready" event
            DWORD retval = WaitForSingleObject(hBufferReadyEvent, 2000);
            if (retval != WAIT_OBJECT_0)
            {
                // well something really badly fucked up
                // so lets just end this thread b4 we die
                printf("audio buffer retrieval timed out\n");
                return;
            }

            HRESULT hr;

            // calculate the size of the buffer we were given
            unsigned int padding;
            hr = pRenderClient->GetCurrentPadding(&padding);
            didfail("failed to get current padding")

            unsigned int avail = bufferSize - padding;

            // get the buffer
            unsigned char* buffer;
            hr = pRenderer->GetBuffer(avail, &buffer);
            didfail("could not get buffer")

            // fill the buffer
            if(m_Callback != nullptr)
            {   
                m_Callback(reinterpret_cast<float*>(buffer), avail);
            }
            else
            {
                printf("fuck");
            }

            // fucken submit the cunt
            hr = pRenderer->ReleaseBuffer(avail, 0);
            didfail("could not release buffer")
        }
    }

    void AudioRenderer::start()
    {
        // create thread
        hRenderThread = CreateThread(
            NULL,
            0,
            AudioRenderThreadProc,
            (LPVOID*)this, // this is needed
            CREATE_SUSPENDED,
            NULL);

        // start audio
        pRenderClient->Start();

        // set running to true
        running = true;

        // resume thread
        ResumeThread(hRenderThread);
    }

    void AudioRenderer::stop()
    {
        running = false;

        // join thread
        WaitForSingleObject(hRenderThread, 9999);

        pRenderClient->Stop();
    }

    void AudioRenderer::setBufferAvailableCallback(BufferAvailableCallback callback)
    {
        m_Callback = callback;
    }

    ////////
    //////// CONTEXT STUFF
    ////////

    void Context::init()
    {
        CoInitialize(nullptr);

        HRESULT hr; 
    
        hr = CoCreateInstance(
                __uuidof(MMDeviceEnumerator), 
                NULL,
                CLSCTX_ALL, 
                __uuidof(IMMDeviceEnumerator),
                (void**)&pEnumerator);
        didfail("device enumerater failed to be made")

        hr = pEnumerator->EnumAudioEndpoints(eAll, DEVICE_STATE_ACTIVE, &pCollection);
        didfail("could not enumerate devices")

        unsigned int deviceCount;
        hr = pCollection->GetCount(&deviceCount);
        didfail("could not get device count")
        
        // for(int i = 0; i < deviceCount; i++)
        // {
        //     IMMDevice* pDevice;
        //     IPropertyStore* pPropStore;

        //     hr = pCollection->Item(i, &pDevice);
        //     didfail("could not get device")
        //     if(pDevice == nullptr)
        //         return;
            
        //     hr = pDevice->OpenPropertyStore(STGM_READ, &pPropStore);
        //     didfail("could not read properties of device")

        //     PROPVARIANT varName;
        //     // Initialize container for property value.
        //     PropVariantInit(&varName);
        //     hr = pPropStore->GetValue(PKEY_Device_FriendlyName, &varName);
        //     didfail("could not get value from device")

        //     printf("Endpoint %d: \"%S\"\n",
        //         i, varName.pwszVal);

        //     pPropStore->Release();
        //     pDevice->Release();
        // }

        /////// at this point we select a device to use, we will use 0, but this should be loaded from a file or something\



        /// COMPILE AUDIO RENDERER
        
        // get default audio device
        pAudioRenderer = new AudioRenderer;

        hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &pAudioRenderer->pAudioDevice);
        didfail("could not get the default audio render device.")







        IPropertyStore* pPropStore;
        hr = pAudioRenderer->pAudioDevice->OpenPropertyStore(STGM_READ, &pPropStore);
        didfail("could not read properties of device")

        PROPVARIANT varName;
        // Initialize container for property value.
        PropVariantInit(&varName);
        hr = pPropStore->GetValue(PKEY_Device_FriendlyName, &varName);
        didfail("could not get value from device")

        printf("Default device: \"%S\"\n", varName.pwszVal);













        // activate device and get the iaudioclient
        hr = pAudioRenderer->pAudioDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&pAudioRenderer->pRenderClient);
        didfail("could not activate device")

        // now lets deal with WASAPI

        //https://docs.microsoft.com/en-us/windows/win32/directshow/audio-subtypes
        // deal with the return of this thing
        // if its not IEEE_FLOAT then something seriously went wrong(someone has a bad computer)
        WAVEFORMATEX* pwfx;
        hr = pAudioRenderer->pRenderClient->GetMixFormat(&pwfx);
        didfail("could not get format")

        UINT32 nFrames = 1;
        //hr = pClient->GetBufferSize(&nFrames);
        didfail("could not get buffer size")

        // create an event
        pAudioRenderer->hBufferReadyEvent = CreateEventA(NULL, FALSE, FALSE, NULL);

        // this is the formula from MSDN, what does it do not sure but i bloody hope it works
        REFERENCE_TIME hnsRequestedDuration = 100000;//might produce buffer size of 9600
        
        hr = pAudioRenderer->pRenderClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
            AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
            hnsRequestedDuration,
            0,
            pwfx,
            NULL);
        didfail("failed to initialize client")

        // assign the event
        hr = pAudioRenderer->pRenderClient->SetEventHandle(pAudioRenderer->hBufferReadyEvent);
        didfail("failed to setup event handle")

        // get buffer size
        hr = pAudioRenderer->pRenderClient->GetBufferSize(&pAudioRenderer->bufferSize);
        didfail("could not get buffer size")

        printf("Buffer Size:               %d\n", pAudioRenderer->bufferSize);
        printf("Number of channels:        %d\n", pwfx->nChannels);
        printf("Sample rate:               %d\n", pwfx->nSamplesPerSec);
        printf("Bits per sample:           %d\n", pwfx->wBitsPerSample);

        if(pwfx->wFormatTag == 0xFFFE)
        {
            printf("Extended Format Audio :(\n");
            WAVEFORMATEXTENSIBLE* pexwf = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(pwfx);
            printf("Samples per block:     %d\n", pexwf->Samples.wSamplesPerBlock);
            printf("Valid bits per sample: %d\n", pexwf->Samples.wValidBitsPerSample);
            GUID guid = pexwf->SubFormat;
            // taken from stack over flow lol
            printf("Guid = {%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX}\n", 
            guid.Data1, guid.Data2, guid.Data3, 
            guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
            guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
        }
        
        // get the render client so we can render some audio maybe?
        //IAudioRenderClient* pRenderer;
        hr = pAudioRenderer->pRenderClient->GetService(__uuidof(IAudioRenderClient), (void**)&pAudioRenderer->pRenderer);
        didfail("could not create audio renderer")

        // get the init buffer
        unsigned char* buffer;
        hr = pAudioRenderer->pRenderer->GetBuffer(pAudioRenderer->bufferSize, &buffer);
        didfail("could not get buffer size")
        memset(buffer, 0, pAudioRenderer->bufferSize * 4 * 2);
        hr = pAudioRenderer->pRenderer->ReleaseBuffer(pAudioRenderer->bufferSize, 0);
        didfail("could not release buffer")

        }

    void Context::destroy()
    {
        pCapturer->Release();
        pCaptureClient->Release();

        pAudioRenderer->pRenderer->Release();
        pAudioRenderer->pRenderClient->Release();
        pAudioRenderer->pAudioDevice->Release();
        CloseHandle(pAudioRenderer->hBufferReadyEvent);
        
        pCollection->Release();
        pEnumerator->Release();

        delete pAudioRenderer;
        
        //pAudioInputDevice->Release();

    }



    ////////
    //////// WEIRD SINGLETON STUFF
    ////////

    // is this bad? probably but it makes it easy to use, i wouldnt exactly call this a deployable solution

    static Context* context;
    
    void createContext()
    {
        if(context != nullptr)
            return;
        context = new Context; 
        context->init();
    }

    AudioRenderer* getRenderer()
    {
        createContext();
        return context->pAudioRenderer;
    }

    void closeContext() 
    {
        context->destroy();
        delete context;
    }


}