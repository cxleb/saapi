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
    class SAAPIContext
    {
    friend SAAPIContext* getContext();
    friend void closeContext();
    friend DWORD WINAPI WinThreadProc(LPVOID Param);
    private: // methods
        void init();
        void destroy();

        void threadProc();
    public: // methods
        SAAPIContext();
        void start();
        void stop();
        void setBufferAvailableCallback(BufferAvailableCallback callback);
        void waitForAudio();
    private: // members
        IMMDeviceEnumerator* pEnumerator;
        IMMDeviceCollection* pCollection;
        IMMDevice* pAudioOutputDevice;
        IAudioClient* pClient;
        IAudioRenderClient* pRenderer;
        HANDLE hBufferReadyEvent;
        HANDLE hRunningMutex;
        HANDLE hAudioThread;
        bool running; // this is protected by a mutex !beware!
        unsigned int bufferSize;
        BufferAvailableCallback m_Callback;
    public: // members

    };

    SAAPIContext* getContext();
    void closeContext();

}