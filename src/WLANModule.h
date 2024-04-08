#pragma once
#include "OpenKNX.h"
#include "WiFi.h"

class WLANModule : public OpenKNX::Module
{
    bool wlanConfigured = false;
    bool usingSavedConfiguration = false;
  public:
    WLANModule();

    void setup(bool configured) override;
    void loop() override;

    const std::string name() override;
    const std::string version() override;
    void showHelp() override;
    bool connected();
    bool processCommand(const std::string cmd, bool diagnoseKo) override;

};

extern WLANModule openknxWLANModule;