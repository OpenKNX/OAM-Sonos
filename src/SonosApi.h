#pragma once
#include "OpenKNX.h"
#include "SonosUPnP.h"
#include "WiFi.h"
#include <ESPAsyncWebServer.h>

enum SonosApiNotification
{
  SonosVolumeChange = 0,
};

class SonosApiNotificationHandler
{
  public:
    virtual void notificationVolumeChanged(uint8_t volume) = 0;
};

enum SonosApiPlayState : byte
{
  Unkown,
  Stopped,
  Transitioning,
  Playing,
  Paused_Playback,
};

class SonosApi : AsyncWebHandler
{
    const static uint32_t _subscriptionTimeInSeconds = 600;

    IPAddress _speakerIP;
    uint16_t _channelIndex;
    uint32_t _renderControlSeq = 0;
    unsigned long _subscriptionTime = 0;
    volatile uint8_t _currentVolume = 0;
    volatile uint8_t _currentGroupVolume = 0;
    volatile SonosApiPlayState _currentPlayState = SonosApiPlayState::Unkown;
    SonosApiNotificationHandler* _notificationHandler = nullptr;
  
    const std::string logPrefix()
    {
      return "Sonos.API";
    }

    void wifiClient_xPath(MicroXPath_P& xPath, WiFiClient& wifiClient, PGM_P* path, uint8_t pathSize, char* resultBuffer, size_t resultBufferSize);

    void writeSoapHttpCall(Stream& stream, const char* soapUrl, const char* soapAction, const char* action, String parameterXml);
    void writeSubscribeHttpCall(Stream& stream, const char* soapUrl);


    typedef std::function<void(WiFiClient&)> THandlerWifiResultFunction;
    int postAction(const char* soapUrl, const char* soapAction, const char* action, String parameterXml, THandlerWifiResultFunction wifiResultFunction = nullptr);
    int subscribeEvents(const char* soapUrl);
    void subscribeAll();

    // AsyncWebHandler
    bool canHandle(AsyncWebServerRequest *request) override final;
    void handleBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) override final;
   
  public:
    void init(AsyncWebServer* webServer, IPAddress speakerIP, uint16_t channelIndex);
    void setCallback(SonosApiNotificationHandler* notificationHandler);
    void loop();
    void setVolume(uint8_t volume);
    uint8_t getVolume();
    void setGroupVolume(uint8_t volume);
    uint8_t getGroupVolume();
    SonosApiPlayState getPlayState();
};