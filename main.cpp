#include "saapi.h"
#include <cstdio>
#include <string>
int phase = 0;

double freq;

double notes[] = {
    440.0,  // a
    493.88, // b
    523.25, // c
    587.33, // d
    659.25, // e
    698.46, // f
    783.99, // g
};


bool getChar(TCHAR &ch)
{
    bool    ret = false;

    HANDLE  stdIn = GetStdHandle(STD_INPUT_HANDLE);

    DWORD   saveMode;
    GetConsoleMode(stdIn, &saveMode);
    SetConsoleMode(stdIn, ENABLE_PROCESSED_INPUT);

    if (WaitForSingleObject(stdIn, INFINITE) == WAIT_OBJECT_0)
    {
        DWORD num;
        ReadConsole(stdIn, &ch, 1, &num, NULL);

        if (num == 1) ret = true;
    }

    SetConsoleMode(stdIn, saveMode);

    return(ret);
}

TCHAR getChar(void)
{
    TCHAR ch = 0;
    getChar(ch);
    return(ch);
}

int main(int argc, char** argv)
{

    auto context = SAAPI::getContext();

    freq=440.0;

    context->setBufferAvailableCallback([](float* buffer, unsigned int size){
        for(int i = 0; i < size; i++)
        {
            float formant = sin(3.1415926535 * (((double)phase/96000.0)*(freq/4.0))); // 96kHz audio ytbs
            float sample = sin(3.1415926535 * ( (formant * 0.4) + ((double)phase/96000.0)*freq)); // 96kHz audio ytbs
            sample *= 0.5; // 50% volume
            buffer[i*2] = sample; // left 
            buffer[i*2 + 1] = sample; // right or left and im wrong
            phase++;
        }
    });

    context->start();

    printf("Z-X-C-V-B-N-M -> A-B-C-D-E-F-G\n");
    while(1)
    {
        char c = getChar();
        switch(c)
        {
            case 'z':
            case 'Z':
            freq=notes[0];
            break;
            case 'x':
            case 'X':
            freq=notes[1];
            break;
            case 'c':
            case 'C':
            freq=notes[2];
            break;
            case 'v':
            case 'V':
            freq=notes[3];
            break;
            case 'b':
            case 'B':
            freq=notes[4];
            break;
            case 'n':
            case 'N':
            freq=notes[5];
            break;
            case 'm':
            case 'M':
            freq=notes[6];
            break;
        }
    }

    return 0;
}