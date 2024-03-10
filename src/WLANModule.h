#pragma once
#include "OpenKNX.h"

class WLANModule : public OpenKNX::Module
{
  public:
    WLANModule();

    void setup(bool configured) override;

    const std::string name() override;
    const std::string version() override;
};

extern WLANModule openknxWLANModule;