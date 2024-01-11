#pragma once
#include "OpenKNX.h"
#include "SonosUPnP.h"

class SonosApi
{
    const std::string logPrefix()
    {
      return "Sonos.API";
    }
    int postAction(IPAddress speakerIP, const char* soapUrl, const char* soapAction, const char* action, String parameterXml);
  public:
    void setVolume(IPAddress speakerIP, uint8_t volume);
    uint8_t getVolume(IPAddress speakerIP);
    void setGroupVolume(IPAddress speakerIP, uint8_t volume);
    uint8_t getGroupVolume(IPAddress speakerIP);
};