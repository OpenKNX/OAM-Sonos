#pragma once
#include "OpenKNX.h"
#include "SonosApi.h"
#include "SonosApiPlayNotification.h"
#include "WiFi.h"

class SonosModule;

class SonosChannel : public OpenKNX::Channel, protected SonosApiNotificationHandler
{
    private:
#ifdef ARDUINO_ARCH_ESP32    
        SonosApiPlayNotification* _playNotification = nullptr;
#endif
        SonosModule& _sonosModule;
        SonosSpeaker* _sonosSpeaker;
        String _name;
        void notificationVolumeChanged(SonosSpeaker* speaker, uint8_t volume) override;
        void notificationMuteChanged(SonosSpeaker* speaker, boolean mute) override;
        void notificationGroupVolumeChanged(SonosSpeaker* speaker, uint8_t volume) override;
        void notificationGroupMuteChanged(SonosSpeaker* speaker, boolean mute) override;
        void notificationPlayStateChanged(SonosSpeaker* speaker, SonosApiPlayState playState) override;
        void notificationGroupCoordinatorChanged(SonosSpeaker* speaker) override;
        void notificationTrackChanged(SonosSpeaker* speaker, SonosTrackInfo& trackInfo) override;
        void joinChannel(uint8_t channelNumber);
        void joinNextPlayingGroup();
        bool delegateCoordination(bool rejoinGroup);     
#if ARDUINO_ARCH_ESP32 
        void playNotification(byte notificationNumber);
#endif
    protected:
        void loop1() override;
        void processInputKo(GroupObject &ko) override;
    public:
        SonosChannel(SonosModule& sonosModule, uint8_t _channelIndex /* this parameter is used in macros, do not rename */, SonosApi& sonosApi);
        const IPAddress speakerIP();
        const std::string name() override;
        const std::string logPrefix() override;
        bool processCommand(const std::string cmd, bool diagnoseKo);        
};