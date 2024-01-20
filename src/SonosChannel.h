#pragma once
#include "OpenKNX.h"
#include "SonosApi.h"
#include "VolumeController.h"

class SonosChannel : public OpenKNX::Channel, protected SonosApiNotificationHandler
{
    private:
        SonosApi& _sonosApi;
        VolumeController _volumeController;
        VolumeController _groupVolumeController;
        IPAddress _speakerIP;
        String _name;
        void notificationVolumeChanged(uint8_t volume) override;
        void notificationMuteChanged(boolean mute) override;
        void notificationGroupVolumeChanged(uint8_t volume) override;
        void notificationGroupMuteChanged(boolean mute) override;
    protected:
        void loop1() override;
        void processInputKo(GroupObject &ko) override;
    public:
        SonosChannel(uint8_t _channelIndex /* this parameter is used in macros, do not rename */, SonosApi& sonosApi);
        const IPAddress& speakerIP();
        const std::string name() override;
        const std::string logPrefix() override;
        bool processCommand(const std::string cmd, bool diagnoseKo);
        
};