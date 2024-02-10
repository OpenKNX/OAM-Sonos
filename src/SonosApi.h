#pragma once
#include "OpenKNX.h"
#include "WiFi.h"
#include <MicroXPath.h>
#include <MicroXPath_P.h>
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

class SonosTrackInfo
{
  public:
    uint16_t trackNumber;
	  uint32_t duration;
	  String uri;
    String metadata;
};

class CurrentSonsTrackInfo : public SonosTrackInfo
{
  public:
	  uint32_t position;
};

class SonosApi;

class SonosApiNotificationHandler
{
  public:
    virtual void notificationVolumeChanged(SonosApi& caller, uint8_t volume) {}
    virtual void notificationMuteChanged(SonosApi& caller, boolean mute) {}
    virtual void notificationLoudnessChanged(SonosApi& caller, boolean loudness) {}
    virtual void notificationGroupVolumeChanged(SonosApi& caller, uint8_t volume) {};
    virtual void notificationGroupMuteChanged(SonosApi& caller, boolean mute) {};
    virtual void notificationPlayStateChanged(SonosApi& caller, SonosApiPlayState playState) {};
    virtual void notificationPlayModeChanged(SonosApi& caller, SonosApiPlayMode playMode) {};
    virtual void notificationTrackChanged(SonosApi& caller, SonosTrackInfo* trackInfo) {};
    virtual void notificationGroupCoordinatorChanged(SonosApi& caller) {};
};

class ParameterBuilder
{
  public:
    static const int ENCODE_NO = 0;
    static const int ENCODE_XML = 1;
    static const int ENCODE_DOUBLE_XML = 2;
    static const int ENCODE_URL = 3;
  private:
    Stream* _stream;
    size_t _length = 0;
    const char* currentParameterName = nullptr;
    size_t writeEncoded(const char* buffer, byte escapeMode);
    ParameterBuilder(ParameterBuilder& b) {} // Prevent copy constructor usage
  public:
    size_t length();
    ParameterBuilder(Stream* stream);
    void AddParameter(const char* name, const char* value = nullptr, byte escapeMode = ENCODE_XML);
    void AddParameter(const char* name, int32_t value);
    void AddParameter(const char* name, uint32_t value);
    void AddParameter(const char* name, bool value);
    void BeginParameter(const char* name);
    void ParmeterValuePart(const char* valuePart = nullptr, byte escapeMode = ENCODE_XML);
    void EndParameter();
};

class SonosApi : private AsyncWebHandler
{
    static std::vector<SonosApi*> AllSonosApis;
    const static uint32_t _subscriptionTimeInSeconds = 600;
    String _uuid;
    String _groupCoordinatorUuid;
    SonosApi* _groupCoordinatorCached = nullptr;
    IPAddress _speakerIP;
    uint16_t _channelIndex;
    uint32_t _renderControlSeq = 0;
    unsigned long _lastGroupSnapshot = 0;
    unsigned long _subscriptionTime = 0;
    SonosApiNotificationHandler* _notificationHandler = nullptr;
  
    const std::string logPrefix()
    {
      return "Sonos.API";
    }
    static uint32_t formatedTimestampToSeconds(char* durationAsString);
    static void wifiClient_xPath(MicroXPath_P& xPath, WiFiClient& wifiClient, PGM_P* path, uint8_t pathSize, char* resultBuffer, size_t resultBufferSize);

    typedef std::function<void(ParameterBuilder&)> TParameterBuilderFunction;
    void writeSoapHttpCall(Stream& stream, const char* soapUrl, const char* soapAction, const char* action, TParameterBuilderFunction parameterFunction);
    void writeSubscribeHttpCall(Stream& stream, const char* soapUrl);
    
    SonosApiPlayState getPlayStateFromString(const char* value);
    SonosApiPlayMode getPlayModeFromString(const char* value);
    const char* getPlayModeString(SonosApiPlayMode playMode);

    typedef std::function<void(WiFiClient&)> THandlerWifiResultFunction;
    int postAction(const char* soapUrl, const char* soapAction, const char* action, TParameterBuilderFunction parameterFunction = nullptr, THandlerWifiResultFunction wifiResultFunction = nullptr);
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
    void setLoudness(boolean loudness);
    boolean getLoudness();
    void setGroupVolume(uint8_t volume);
    void setGroupVolumeRelative(int8_t volume);
    uint8_t getGroupVolume();
    void setGroupMute(boolean mute);
    boolean getGroupMute();
    SonosApiPlayState getPlayState();
    void play();
    void pause();
    void stop();
    void next();
    void previous();
    void gotTrack(uint32_t trackNumber);
    SonosApiPlayMode getPlayMode();
    void setPlayMode(SonosApiPlayMode playMode);
    const CurrentSonsTrackInfo getTrackInfo();
    SonosApi* findGroupCoordinator(bool cached = false);
    SonosApi* findNextPlayingGroupCoordinator();
    void setAVTransportURI(const char* schema, const char* uri, const char* metadata = nullptr);
    void playInternetRadio(const char* streamingUrl, const char* radionStationName, const char* imageUrl = nullptr, const char* schema = nullptr);
    void playFromHttp(const char* url);
    void playMusicLibraryFile(const char* mediathekFilePath);
    void playMusicLibraryDirectory(const char* mediathekDirectory);
    void playLineIn();
    void playTVIn();
    void playQueue();
    void joinToGroupCoordinator(SonosApi* coordinator);
    void joinToGroupCoordinator(const char* uid);
    void unjoin();
    SonosApi* findFirstParticipant(bool cached = false);
    void delegateGroupCoordinationTo(SonosApi* sonosApi, bool rejoinGroup);
    void delegateGroupCoordinationTo(const char* uid, bool rejoinGroup);
    void gotoTrack(uint16_t trackNumber);
    void gotoTime(uint8_t hour, uint8_t minute, uint8_t second);
    void addTrackToQueue(const char *scheme, const char *address, const char* metadata = nullptr, uint16_t desiredTrackNumber = 0, bool enqueueAsNext = true);
    void removeAllTracksFromQueue();
};