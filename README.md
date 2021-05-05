# SAAPI - Simple Audio API

A No-Frills Simple Audio API for Microsoft Windows

- uses WASAPI
- Multithreaded by design
- 1 C++ & Header file

## Example

```c++
#include "saapi.h"

auto context = SAAPI::getContext();
context->setBufferAvailableCallback([](float* buffer, unsigned int size)
{
    // fill buffer
});
context->play();
```

## Building



## TODO
- Audio input
- MIDI(need a controller first)
- Better error handling 

