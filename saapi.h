#include "initguid.h"
#include "windows.h"
#include "mmdeviceapi.h"
#include "audioclient.h"
#include "audiopolicy.h"
#include "Functiondiscoverykeys_devpkey.h"

#include <cstdlib>
#include <cstdio>

typedef void(* BufferAvailableCallback) (float*, unsigned int);

namespace SAAPI 
{
    class Context;

    class AudioRenderer
    {
    friend class Context;
    friend DWORD WINAPI AudioRenderThreadProc(LPVOID Param);
    public:
        void start();
        void stop();
        void setBufferAvailableCallback(BufferAvailableCallback callback);
        void threadProc();
    private:
        IMMDevice* pAudioDevice;
        IAudioClient* pRenderClient;
        IAudioRenderClient* pRenderer;
        bool running; // this is protected by a mutex !beware!
        unsigned int bufferSize;
        BufferAvailableCallback m_Callback;
        HANDLE hRenderThread;
        HANDLE hBufferReadyEvent;
    };

    class Context
    {
    friend AudioRenderer* getRenderer();
    friend void closeContext();
    friend void createContext();

    private: // methods
        void init();
        void destroy();

    private: // members
        IMMDeviceEnumerator* pEnumerator;
        IMMDeviceCollection* pCollection;

        AudioRenderer* pAudioRenderer;

        IMMDevice* pAudioInputDevice;
        IAudioClient* pCaptureClient;
        IAudioCaptureClient* pCapturer;
        
    };

    AudioRenderer* getRenderer();
    void closeContext();

}