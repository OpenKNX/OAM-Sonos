#pragma once
#include "OpenKNX.h"
#include "SonosUPnP.h"

class SonosApi
{
  private:
    SonosUPnP* _sonosUpn;
    void ethernetConnectionError();
  public:
    SonosApi();
    void setVolume(IPAddress speakerIP, uint8_t volume);
    uint8_t getVolume(IPAddress speakerIP);
    void setGroupVolume(IPAddress speakerIP, uint8_t volume);
    uint8_t getGroupVolume(IPAddress speakerIP);
};