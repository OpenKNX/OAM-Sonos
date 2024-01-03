#pragma once
#include "OpenKNX.h"

class SonosModule : public OpenKNX::Module
{

  public:
    void loop(bool configured) override;
    void setup(bool configured) override;
#ifdef OPENKNX_DUALCORE
    void loop1(bool configured) override;
    void setup1(bool configured) override;
#endif

    const std::string name() override;
    const std::string version() override;
    void processInputKo(GroupObject &ko) override;
    void processBeforeRestart() override;
    void processBeforeTablesUnload() override;
    void showInformations() override;
    void showHelp() override;
    void savePower() override;
    bool restorePower() override;
    void init() override;
    bool processCommand(const std::string cmd, bool diagnoseKo) override;
   
  private:
   
};

extern SonosModule openknxSonosModule;