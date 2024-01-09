#include "SonosApi.h"
#include "WiFiClient.h"

SonosApi::SonosApi()
{
    WiFiClient wifiClient;
    _sonosUpn = new SonosUPnP(wifiClient, []() { } );
}

void SonosApi::setVolume(IPAddress speakerIP, uint8_t volume)
{
    _sonosUpn->setVolume(speakerIP, volume); 
}


uint8_t SonosApi::getVolume(IPAddress speakerIP)
{
    return  _sonosUpn->getVolume(speakerIP);
}
