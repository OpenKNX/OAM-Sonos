#pragma once
#if ARDUINO_ARCH_ESP32 
#include "WebSocketsClient.h"

class SonosApiPlayNotification
{
    static WebSocketsClient* _webSocket;
    IPAddress _speakerIP;
    const char* _streamUrl;
    byte _volume;
    String _playerId;
    unsigned long _started = millis(); 
    byte _state = 0;
public:
    SonosApiPlayNotification(IPAddress& speakerIP, const char* streamUrl, byte volume, String& playerId);
    ~SonosApiPlayNotification();
    bool checkFinished();
};
#endif