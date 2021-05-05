#include "saapi.h"

#define didfail(msg) if(hr != S_OK) {printf("WINDOWS ERROR: %s!\n", msg); exit(0);}

#define ERROR_

namespace SAAPI 
{
    DWORD WINAPI WinThreadProc(LPVOID Param)
    {
        SAAPIContext* context = reinterpret_cast<SAAPIContext*>(Param);
        context->threadProc();
        return 0;
    }

    SAAPIContext::SAAPIContext()
    {
        m_Callback = nullptr;
    }

    void SAAPIContext::init()
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
        
        for(int i = 0; i < deviceCount; i++)
        {
            IMMDevice* pDevice;
            IPropertyStore* pPropStore;

            hr = pCollection->Item(i, &pDevice);
            didfail("could not get device")
            if(pDevice == nullptr)
                return;
            
            hr = pDevice->OpenPropertyStore(STGM_READ, &pPropStore);
            didfail("could not read properties of device")

            PROPVARIANT varName;
            // Initialize container for property value.
            PropVariantInit(&varName);
            hr = pPropStore->GetValue(PKEY_Device_FriendlyName, &varName);
            didfail("could not get value from device")

            printf("Endpoint %d: \"%S\"\n",
                i, varName.pwszVal);

            pPropStore->Release();
            pDevice->Release();
        }

        /////// at this point we select a device to use, we will use 0, but this should be loaded from a file or something

        hr = pCollection->Item(0, &pAudioOutputDevice);
        didfail("could not get device")
        
        // activate device and get the iaudioclient
        hr = pAudioOutputDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&pClient);
        didfail("could not activate device")

        // now lets deal with WASAPI

        //https://docs.microsoft.com/en-us/windows/win32/directshow/audio-subtypes
        // deal with the return of this thing
        // if its not IEEE_FLOAT then something seriously went wrong(someone has a bad computer)
        WAVEFORMATEX* pwfx;
        hr = pClient->GetMixFormat(&pwfx);
        didfail("could not get format")

        UINT32 nFrames = 1;
        //hr = pClient->GetBufferSize(&nFrames);
        didfail("could not get buffer size")

        // create an event
        hBufferReadyEvent = CreateEventA(NULL, FALSE, FALSE, NULL);

        // this is the formula from MSDN, what does it do not sure but i bloody hope it works
        REFERENCE_TIME hnsRequestedDuration = 100000;//might produce buffer size of 9600
        
        hr = pClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
            AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
            hnsRequestedDuration,
            0,
            pwfx,
            NULL);
        didfail("failed to initialize client")

        // assign the event
        hr = pClient->SetEventHandle(hBufferReadyEvent);
        didfail("failed to setup event handle")

        // get buffer size
        hr = pClient->GetBufferSize(&bufferSize);
        didfail("could not get buffer size")

        printf("Buffer Size:               %d\n", bufferSize);
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
        hr = pClient->GetService(__uuidof(IAudioRenderClient), (void**)&pRenderer);
        didfail("could not create audio renderer")

        // get the init buffer
        unsigned char* buffer;
        hr = pRenderer->GetBuffer(bufferSize, &buffer);
        didfail("could not get buffer size")
        memset(buffer, 0, bufferSize * 4 * 2);
        hr = pRenderer->ReleaseBuffer(bufferSize, 0);
        didfail("could not release buffer")

        // create mutex
        hRunningMutex = CreateMutex(NULL, 0, nullptr);

    }

    void SAAPIContext::destroy()
    {
        pRenderer->Release();
        pClient->Release();
        pCollection->Release();
        pEnumerator->Release();
        pAudioOutputDevice->Release();
        CloseHandle(hBufferReadyEvent);
    }

    void SAAPIContext::threadProc()
    {
        // when start is called, a thread spawns, and this proc starts
        // this proc will get the internal audio buffer, gather samples buffer, 
        // format it, and write it to the internal buffer

        while(1)
        {
            // wait for mutex lock
            int result = WaitForSingleObject(hRunningMutex, 2000);
            if(result != WAIT_OBJECT_0)
            {
                // well something really badly fucked up
                // so lets just end this thread b4 we die
                printf("waiting for mutex timed out\n");
                return;
            }

            if(!running)
            {
                printf("we not running fucking idiot\n");
                return; // ends the thread which means we stopped the audio
            }

            // we are done with it, so release the mutex
            ReleaseMutex(hRunningMutex);

            // wait for "buffer to be ready" event
            DWORD retval = WaitForSingleObject(hBufferReadyEvent, 2000);
            if (retval != WAIT_OBJECT_0)
            {
                // well something really badly fucked up
                // so lets just end this thread b4 we die
                printf("waiting for audio buffer timed out\n");
                return;
            }

            HRESULT hr;

            // calculate the size of the buffer we were given
            unsigned int padding;
            hr = pClient->GetCurrentPadding(&padding);
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

    void SAAPIContext::start()
    {
        // create thread
        hAudioThread = CreateThread(
            NULL,
            0,
            WinThreadProc,
            (LPVOID*)this, // this is needed
            CREATE_SUSPENDED,
            NULL);

        // start audio
        pClient->Start();

        // set running to true
        running = true;

        // resume thread
        ResumeThread(hAudioThread);
    }

    void SAAPIContext::stop()
    {
        running = false;

        // join thread
        WaitForSingleObject(hAudioThread, 9999);

        pClient->Stop();
    }

    void SAAPIContext::setBufferAvailableCallback(BufferAvailableCallback callback)
    {
        m_Callback = callback;
    }


    static SAAPIContext* context;
    
    SAAPIContext* getContext()
    {
        if(context != nullptr)
            return context;

        context = new SAAPIContext; 
        context->init();
        return context;
    }

    void closeContext() 
    {
        context->destroy();
        delete context;
    }


}