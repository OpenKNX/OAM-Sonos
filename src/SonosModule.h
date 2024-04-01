#pragma once
#include "OpenKNX.h"
#include "ChannelOwnerModule.h"
#include "SonosApi.h"
#if USE_ESP_ASNC_WEB_SERVER 
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#endif
#ifndef OPENKNX_DUALCORE
#error Sonos Module requires OPENKNX_DUALCORE
#endif

class SonosModule : public ChannelOwnerModule
{
    SonosApi _sonosApi;
    bool _channelSetupCalled = false;
    volatile bool _channelSetup1Called = false;
  public:
    SonosModule();

    void setup() override;
    void setup1() override;

    const std::string name() override;
    const std::string version() override;
    void processInputKo(GroupObject &ko) override;
    void processBeforeRestart() override;
    void processBeforeTablesUnload() override;
    void showInformations() override;
    void savePower() override;
    bool restorePower() override;
    void loop() override;
    void loop1() override;
    void showHelp() override;
    bool processCommand(const std::string cmd, bool diagnoseKo) override;
  protected:
    OpenKNX::Channel* createChannel(uint8_t _channelIndex /* this parameter is used in macros, do not rename */) override; 
};

extern SonosModule openknxSonosModule;