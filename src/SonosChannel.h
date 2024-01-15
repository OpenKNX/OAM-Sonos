#pragma once
#include "OpenKNX.h"
#include "SonosApi.h"
#include "VolumeController.h"

class SonosChannel : public OpenKNX::Channel, protected SonosApiNotificationHandler
{
    private:
        IPAddress _speakerIP;
        String _name;
        SonosApi& _sonosApi;
        VolumeController _volumeController;
        VolumeController _groupVolumeController;
        void notificationVolumeChanged(uint8_t volume) override;
    protected:
        void loop1() override;
        void processInputKo(GroupObject &ko) override;
    public:
        SonosChannel(SonosApi& sonosApi);
        const IPAddress& speakerIP();
        const std::string name() override;
        const std::string logPrefix() override;
        bool processCommand(const std::string cmd, bool diagnoseKo);
        
};