#include "SonosApi.h"

SonosApi::SonosApi(const char* ipAdress)
: _ipAdress(ipAdress)
{
}

std::string SonosApi::getName()
{
    return _ipAdress;
}

void SonosApi::setVolume(uint8_t volume)
{
}

uint8_t SonosApi::getVolume()
{
    return 50;
}
