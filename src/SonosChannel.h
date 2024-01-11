#pragma once
#include "OpenKNX.h"
#include "KoOwner.h"
#include "SonosApi.h"
#include "VolumeController.h"

class SonosChannel : public OpenKNX::Channel, public KoOwner
{
    private:
        IPAddress _speakerIP;
        String _name;
        SonosApi& _sonosApi;
        VolumeController _volumeController;
    protected:
        void loop1() override;
        void processInputKo(GroupObject &ko) override;
    public:
        SonosChannel(SonosApi& sonosApi);
        const std::string name() override;
        const std::string logPrefix() override;
        void test();
        
};