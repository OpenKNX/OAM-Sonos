#pragma once
#include "OpenKNX.h"

class SonosApi
{
  private:
    const char* _ipAdress;

  public:
    std::string getName();
    SonosApi(const char* ipAdress);
    void setVolume(uint8_t volume);
    uint8_t getVolume();
};