#pragma once
#include "OpenKNX.h"
#include "SonosApi.h"
#include "VolumeController.h"

class SonosModule;

class SonosChannel : public OpenKNX::Channel, protected SonosApiNotificationHandler
{
    private:
        SonosModule& _sonosModule;
        SonosApi& _sonosApi;
        VolumeController _volumeController;
        VolumeController _groupVolumeController;
        IPAddress _speakerIP;
        String _name;
        void notificationVolumeChanged(SonosApi& caller, uint8_t volume) override;
        void notificationMuteChanged(SonosApi& caller, boolean mute) override;
        void notificationGroupVolumeChanged(SonosApi& caller, uint8_t volume) override;
        void notificationGroupMuteChanged(SonosApi& caller, boolean mute) override;
        void notificationPlayStateChanged(SonosApi& caller, SonosApiPlayState playState) override;
        void notificationGroupCoordinatorChanged(SonosApi& caller) override;
    protected:
        void loop1() override;
        void processInputKo(GroupObject &ko) override;
    public:
        SonosChannel(SonosModule& sonosModule, uint8_t _channelIndex /* this parameter is used in macros, do not rename */, SonosApi& sonosApi);
        const IPAddress& speakerIP();
        const std::string name() override;
        const std::string logPrefix() override;
        bool processCommand(const std::string cmd, bool diagnoseKo);
        void joinNextPlayingGroup();
        
};