#pragma once
#include "OpenKNX.h"
#include "SonosUPnP.h"

class SonosApi
{
    const std::string logPrefix()
    {
      return "Sonos.API";
    }
    void writeSoapHttpCall(Stream& stream, IPAddress& ipAddress, const char* soapUrl, const char* soapAction, const char* action, String parameterXml);
    void writeSubscribeHttpCall(Stream& stream, IPAddress& ipAddress, const char* soapUrl);

    int postAction(IPAddress& speakerIP, const char* soapUrl, const char* soapAction, const char* action, String parameterXml);
    int subscribeEvents(IPAddress& ipAddress, const char* soapUrl);
  public:
    void setVolume(IPAddress& speakerIP, uint8_t volume);
    uint8_t getVolume(IPAddress& speakerIP);
    void setGroupVolume(IPAddress& speakerIP, uint8_t volume);
    uint8_t getGroupVolume(IPAddress& speakerIP);
    void subscribeAVTransport(IPAddress& ipAddress);
};