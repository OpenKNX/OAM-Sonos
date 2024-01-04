#pragma once
#include "OpenKNX.h"
#include "KoOwner.h"
#include "SonosApi.h"

class SonosChannel : public OpenKNX::Channel, public KoOwner
{
    private:
        SonosApi* _sonosApi;
    public:
        SonosChannel(SonosApi* sonosApi);
        const std::string name() override;
        const std::string logPrefix() override;
};