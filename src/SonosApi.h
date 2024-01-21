#pragma once
#include "OpenKNX.h"
#include "SonosUPnP.h"
#include "WiFi.h"
#include <ESPAsyncWebServer.h>

enum SonosApiPlayState : byte
{
  Unkown,
  Stopped, // STOPPED
  Transitioning, // TRANSITIONING
  Playing, // PLAYING
  Paused_Playback, // PAUSED_PLAYBACK
};

enum SonosApiPlayMode : byte
{
  Normal = 0, // NORMAL
  Repeat = 1, // REPEAT_ONE || REPEAT_ALL || SHUFFLE_REPEAT_ONE || SHUFFLE_NOREPEAT
  RepeatModeAll = 2, // REPEAT_ALL || SHUFFLE
  Shuffle = 4 // SHUFFLE_NOREPEAT || SHUFFLE_NOREPEAT || SHUFFLE || SHUFFLE_REPEAT_ONE
};

class SonosApi;

class SonosApiNotificationHandler
{
  public:
    virtual void notificationVolumeChanged(SonosApi& caller, uint8_t volume) {}
    virtual void notificationMuteChanged(SonosApi& caller, boolean mute) {}
    virtual void notificationGroupVolumeChanged(SonosApi& caller, uint8_t volume) {};
    virtual void notificationGroupMuteChanged(SonosApi& caller, boolean mute) {};
    virtual void notificationPlayStateChanged(SonosApi& caller, SonosApiPlayState playState) {};
    virtual void notificationPlayModeChanged(SonosApi& caller, SonosApiPlayMode playMode) {};
};

class SonosTrackInfo
{
  public:
    uint16_t queueIndex;
	  uint32_t duration;
	  uint32_t position;
	  String uri;
    String metadata;
};

class SonosApi : private AsyncWebHandler
{
    static std::vector<SonosApi*> AllSonosApis;
    const static uint32_t _subscriptionTimeInSeconds = 600;
    String _uuid;
    IPAddress _speakerIP;
    uint16_t _channelIndex;
    uint32_t _renderControlSeq = 0;
    unsigned long _subscriptionTime = 0;
    SonosApiNotificationHandler* _notificationHandler = nullptr;
  
    const std::string logPrefix()
    {
      return "Sonos.API";
    }

    static void wifiClient_xPath(MicroXPath_P& xPath, WiFiClient& wifiClient, PGM_P* path, uint8_t pathSize, char* resultBuffer, size_t resultBufferSize);

    void writeSoapHttpCall(Stream& stream, const char* soapUrl, const char* soapAction, const char* action, String parameterXml);
    void writeSubscribeHttpCall(Stream& stream, const char* soapUrl);
    
    SonosApiPlayState getPlayStateFromString(const char* value);
    SonosApiPlayMode getPlayModeFromString(const char* value);
    const char* getPlayModeString(SonosApiPlayMode playMode);

    typedef std::function<void(WiFiClient&)> THandlerWifiResultFunction;
    int postAction(const char* soapUrl, const char* soapAction, const char* action, String parameterXml, THandlerWifiResultFunction wifiResultFunction = nullptr);
    int subscribeEvents(const char* soapUrl);
    void subscribeAll();

    // AsyncWebHandler
    bool canHandle(AsyncWebServerRequest *request) override final;
    void handleBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) override final;
   
  public:
    ~SonosApi();
    String& getUID();
    static String getUID(IPAddress ipAddress);

    IPAddress& getSpeakerIP();
    void init(AsyncWebServer* webServer, IPAddress speakerIP);
    void setCallback(SonosApiNotificationHandler* notificationHandler);
    void loop();
    void setVolume(uint8_t volume);
    void setVolumeRelative(int8_t volume);
    uint8_t getVolume();
    void setMute(boolean mute);
    boolean getMute();
    void setGroupVolume(uint8_t volume);
    void setGroupVolumeRelative(int8_t volume);
    uint8_t getGroupVolume();
    void setGroupMute(boolean mute);
    boolean getGroupMute();
    SonosApiPlayState getPlayState();
    void play();
    void pause();
    void next();
    void previous();
    SonosApiPlayMode getPlayMode();
    void setPlayMode(SonosApiPlayMode playMode);
    const SonosTrackInfo getTrackInfo();
    SonosApi* findGroupCoordinator();
    SonosApi* findNextPlayingGroupCoordinator();
    void setAVTransportURI(const char* schema, const char* currentURI, const char* currentURIMetaData = nullptr);
    void joinToGroupCoordinator(SonosApi* coordinator);
    void joinToGroupCoordinator(const char* uid);
};