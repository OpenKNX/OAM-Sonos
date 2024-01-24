#include "SonosApi.h"
#include "WiFi.h"
#include "WiFiClient.h"

const char p_SoapEnvelope[] PROGMEM = "s:Envelope";
const char p_SoapBody[] PROGMEM = "s:Body";
const char sonosSchemaMaster[] PROGMEM = "x-rincon:";

std::vector<SonosApi*> SonosApi::AllSonosApis;

SonosApi::~SonosApi()
{
    auto it = std::find(AllSonosApis.begin(), AllSonosApis.end(), this);
    if(it != AllSonosApis.end())
        AllSonosApis.erase(it);
}

IPAddress& SonosApi::getSpeakerIP()
{
    return _speakerIP;
}

void SonosApi::init(AsyncWebServer* webServer, IPAddress speakerIP)
{
    _speakerIP = speakerIP;
    _channelIndex = AllSonosApis.size();
    webServer->addHandler(this);
    AllSonosApis.push_back(this);
    _subscriptionTime = millis() - ((_subscriptionTimeInSeconds - _channelIndex) * 1000); // prevent inital subscription to be at the same time for all channels
}

void SonosApi::wifiClient_xPath(MicroXPath_P& xPath, WiFiClient& wifiClient, PGM_P* path, uint8_t pathSize, char* resultBuffer, size_t resultBufferSize)
{
    xPath.setPath(path, pathSize);
    while (wifiClient.available() && !xPath.getValue(wifiClient.read(), resultBuffer, resultBufferSize))
        ;
}

void SonosApi::setCallback(SonosApiNotificationHandler* notificationHandler)
{
    _notificationHandler = notificationHandler;
}

void SonosApi::subscribeAll()
{
    Serial.print("Subscribe ");
    Serial.println(millis());
    _subscriptionTime = millis();
    if (_subscriptionTime == 0)
        _subscriptionTime = 1;
    _renderControlSeq = 0;
    subscribeEvents("/MediaRenderer/RenderingControl/Event");
    subscribeEvents("/MediaRenderer/GroupRenderingControl/Event");
    subscribeEvents("/MediaRenderer/AVTransport/Event");
}

void SonosApi::loop()
{
    if (_subscriptionTime > 0)
    {
        if (millis() - _subscriptionTime > _subscriptionTimeInSeconds * 1000)
        {
            subscribeAll();
        }
    }
}

bool SonosApi::canHandle(AsyncWebServerRequest* request)
{
    if (request->url().startsWith("/notification/"))
    {
        if (request->url() == "/notification/" + (String)_channelIndex)
        {
            return true;
        }
    }
    return false;
}

const char p_PropertySet[] PROGMEM = "e:propertyset";
const char p_Property[] PROGMEM = "e:property";
const char p_LastChange[] PROGMEM = "LastChange";

void charBuffer_xPath(MicroXPath_P& xPath, const char* readBuffer, size_t readBufferLength, PGM_P* path, uint8_t pathSize, char* resultBuffer, size_t resultBufferSize)
{
    xPath.setPath(path, pathSize);
    for (size_t i = 0; i < readBufferLength; i++)
    {
        if (xPath.getValue(readBuffer[i], resultBuffer, resultBufferSize))
            return;
    }
}

boolean readFromEncodeXML(const char* encodedXML, const char* search,  char* resultBuffer, size_t resultBufferSize)
{
    if (resultBufferSize > 0)
        resultBuffer[0] = '\0';
    const char* begin = strstr(encodedXML, search);
    if (begin != nullptr)
    {
        begin += strlen(search);
        int i = 0;
        for (; i < resultBufferSize - 1; i++)
        {
            auto c = *begin;
            if (c == '&' || c == '\0')
                break;
            resultBuffer[i] = c;
            begin++;
        }
        resultBuffer[i] = '\0';
        return true;
    }
    return false;
}

SonosApiPlayState SonosApi::getPlayStateFromString(const char* value)
{
    if (strcmp(value, "STOPPED") == 0)
        return SonosApiPlayState::Stopped;
    else if (strcmp(value, "PLAYING") == 0)
        return SonosApiPlayState::Playing;
    else if (strcmp(value, "PAUSED_PLAYBACK") == 0)
        return SonosApiPlayState::Paused_Playback;
    else if (strcmp(value, "TRANSITIONING") == 0)
        return SonosApiPlayState::Transitioning;
    return SonosApiPlayState::Unkown;
}

const char p_PlayModeNormal[] PROGMEM = "NORMAL"; 
const char p_PlayModeRepeatOne[] PROGMEM = "REPEAT_ONE"; 
const char p_PlayModeRepeatAll[] PROGMEM = "REPEAT_ALL"; 
const char p_PlayModeShuffle[] PROGMEM = "SHUFFLE"; 
const char p_PlayModeShuffleRepeatOne[] PROGMEM = "SHUFFLE_REPEAT_ONE"; 
const char p_PlayModeShuffleNoRepeat[] PROGMEM = "SHUFFLE_NOREPEAT"; 

/*
  Normal = 0, // NORMAL
  Repeat = 1, // REPEAT_ONE || REPEAT_ALL || SHUFFLE_REPEAT_ONE || SHUFFLE_NOREPEAT
  RepeatModeAll = 2, // REPEAT_ALL || SHUFFLE
  Shuffle = 4 // SHUFFLE_NOREPEAT || SHUFFLE_NOREPEAT || SHUFFLE || SHUFFLE_REPEAT_ONE
*/

SonosApiPlayMode SonosApi::getPlayModeFromString(const char* value)
{
    if (strcmp(value, p_PlayModeNormal) == 0)
        return SonosApiPlayMode::Normal;
    if (strcmp(value, p_PlayModeRepeatOne) == 0)
        return SonosApiPlayMode::Repeat;
    if (strcmp(value, p_PlayModeRepeatAll) == 0)
        return (SonosApiPlayMode)((byte) SonosApiPlayMode::Repeat | (byte) SonosApiPlayMode::RepeatModeAll);
    if (strcmp(value, p_PlayModeShuffle) == 0)
        return (SonosApiPlayMode)((byte) SonosApiPlayMode::Shuffle | (byte) SonosApiPlayMode::Repeat | (byte) SonosApiPlayMode::RepeatModeAll);
    if (strcmp(value, p_PlayModeShuffleRepeatOne) == 0)
        return (SonosApiPlayMode)((byte) SonosApiPlayMode::Shuffle | (byte) SonosApiPlayMode::Repeat);
    if (strcmp(value, p_PlayModeShuffleNoRepeat) == 0)
        return SonosApiPlayMode::Shuffle;
    else
        return SonosApiPlayMode::Normal;
}

const char* SonosApi::getPlayModeString(SonosApiPlayMode playMode)
{
    if (playMode == SonosApiPlayMode::Normal)
        return p_PlayModeNormal;
    if (playMode == SonosApiPlayMode::Repeat)
        return p_PlayModeRepeatOne;
    if (playMode == (SonosApiPlayMode)((byte) SonosApiPlayMode::Repeat | (byte) SonosApiPlayMode::RepeatModeAll))
        return p_PlayModeRepeatAll;
    if (playMode == (SonosApiPlayMode)((byte) SonosApiPlayMode::Shuffle | (byte) SonosApiPlayMode::Repeat | (byte) SonosApiPlayMode::RepeatModeAll))
        return p_PlayModeShuffle;
    if (playMode == (SonosApiPlayMode)((byte) SonosApiPlayMode::Shuffle | (byte) SonosApiPlayMode::Repeat))
        return p_PlayModeShuffleRepeatOne;
    if (playMode == SonosApiPlayMode::Shuffle)
        return p_PlayModeShuffleNoRepeat;
    return p_PlayModeNormal;
}


const char masterVolume[] PROGMEM = "&lt;Volume channel=&quot;Master&quot; val=&quot;";
const char masterMute[] PROGMEM = "&lt;Mute channel=&quot;Master&quot; val=&quot;";
const char transportState[] PROGMEM = "&lt;TransportState val=&quot;";
const char playMode[] PROGMEM = "&lt;CurrentPlayMode val=&quot;";
const char trackURI[] PROGMEM = "&lt;CurrentTrackURI val=&quot;";
const char trackMetaData[] PROGMEM = "&lt;CurrentTrackMetaData val=&quot;";
const char trackDuration[] PROGMEM = "&lt;CurrentTrackDuration val=&quot;";
const char trackNumber[] PROGMEM = "&lt;CurrentTrack val=&quot;";



// const char numberOfTracks[] PROGMEM = "&lt;NumberOfTracks val=&quot;";
// const char currentTrack[] PROGMEM = "&lt;CurrentTrack val=&quot;";

void SonosApi::handleBody(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total)
{
    Serial.print("Notification received ");
    Serial.println(millis());
    //Serial.println(String(data, len));
    if (_notificationHandler != nullptr)
    {
        bool groupCoordinatorChanged = false;
        MicroXPath_P xPath;
        const int bufferSize = 2000;
        char* buffer = new char[bufferSize + 1];
        buffer[bufferSize] = 0;
        buffer[0] = 0;
        PGM_P pathLastChange[] = {p_PropertySet, p_Property, p_LastChange};
        charBuffer_xPath(xPath, (const char*)data, len, pathLastChange, 3, buffer, bufferSize);
        const static int valueBufferSize = 1000;
        SonosTrackInfo* sonosTrackInfo = nullptr;
        char* valueBuffer = new char[1000];
        try
        {
            if (readFromEncodeXML(buffer, masterVolume, valueBuffer, valueBufferSize))
            {
                auto currentVolume = constrain(atoi(valueBuffer), 0, 100);
                Serial.print("Volume: ");
                Serial.println(currentVolume);
                _notificationHandler->notificationVolumeChanged(*this, currentVolume);
            }
            if (readFromEncodeXML(buffer, masterMute, valueBuffer, valueBufferSize))
            {
                auto currentMute = valueBuffer[0] == '1';
                Serial.print("Mute: ");
                Serial.println(currentMute);
                _notificationHandler->notificationMuteChanged(*this, currentMute);
            }
            if (readFromEncodeXML(buffer, transportState, valueBuffer, valueBufferSize))
            {
                auto playState = getPlayStateFromString(valueBuffer);
                Serial.print("Play State: ");
                Serial.println(playState);
                _notificationHandler->notificationPlayStateChanged(*this, playState);
            }
            if (readFromEncodeXML(buffer, playMode, valueBuffer, valueBufferSize))
            {
                auto playMode = getPlayModeFromString(valueBuffer);
                Serial.print("Play Mode: ");
                Serial.println(playMode);
                _notificationHandler->notificationPlayModeChanged(*this, playMode);
            }
            if (readFromEncodeXML(buffer, trackURI, valueBuffer, valueBufferSize))
            {
                Serial.print("Track URI: ");
                Serial.println(valueBuffer);
                if (sonosTrackInfo == nullptr)
                    sonosTrackInfo = new SonosTrackInfo();
                sonosTrackInfo->uri = valueBuffer;
            }
            if (readFromEncodeXML(buffer, trackMetaData, valueBuffer, valueBufferSize))
            {
                Serial.print("Track Metadata: ");
                Serial.println(valueBuffer);
                if (sonosTrackInfo == nullptr)
                    sonosTrackInfo = new SonosTrackInfo();
                sonosTrackInfo->metadata = valueBuffer;
            }
            if (readFromEncodeXML(buffer, trackDuration, valueBuffer, valueBufferSize))
            {
                Serial.print("Track Duration: ");
                Serial.println(valueBuffer);
                if (sonosTrackInfo == nullptr)
                    sonosTrackInfo = new SonosTrackInfo();
            }
            if (readFromEncodeXML(buffer, trackNumber, valueBuffer, valueBufferSize))
            {
                Serial.print("Track Number: ");
                Serial.println(valueBuffer);
                if (sonosTrackInfo == nullptr)
                    sonosTrackInfo = new SonosTrackInfo();
                sonosTrackInfo->trackNumber = atoi(valueBuffer);
            }
            if (sonosTrackInfo != nullptr)
            {
                _notificationHandler->notificationTrackChanged(*this, sonosTrackInfo);
                if (sonosTrackInfo->uri.startsWith(sonosSchemaMaster))
                {
                    auto newGroupCoordinator= sonosTrackInfo->uri.substring(strlen(sonosSchemaMaster));
                    if (newGroupCoordinator !=  _groupCoordinatorUuid)
                    {
                        _groupCoordinatorUuid = newGroupCoordinator;
                        groupCoordinatorChanged = true;
                    }
                }
                else
                {
                    if (!_groupCoordinatorUuid.isEmpty())
                    {
                        _groupCoordinatorUuid.clear();
                        groupCoordinatorChanged = true;
                    }
                }
            }
        }
        catch (...)
        {
            delete sonosTrackInfo;
            delete valueBuffer;
            throw;
        }
        delete sonosTrackInfo;
        delete valueBuffer;
        xPath.reset();
        buffer[0] = 0;
        PGM_P pathGroupVolume[] = {p_PropertySet, p_Property, "GroupVolume"};
        charBuffer_xPath(xPath, (const char*)data, len, pathGroupVolume, 3, buffer, bufferSize);
        if (strlen(buffer) > 0)
        {
            auto currentGroupVolume = constrain(atoi(buffer), 0, 100);
            Serial.print("Group Volume: ");
            Serial.println(currentGroupVolume);
            _notificationHandler->notificationGroupVolumeChanged(*this, currentGroupVolume);           
        }
        xPath.reset();
        buffer[0] = 0;
        PGM_P pathGroupMute[] = {p_PropertySet, p_Property, "GroupMute"};
        charBuffer_xPath(xPath, (const char*)data, len, pathGroupMute, 3, buffer, bufferSize);
        if (strlen(buffer) > 0)
        {
            auto currentGroupMute = buffer[0] == '1';
            Serial.print("Group Mute: ");
            Serial.println(currentGroupMute);
            _notificationHandler->notificationGroupMuteChanged(*this, currentGroupMute);
        }
        delete buffer;
        if (groupCoordinatorChanged)
            _notificationHandler->notificationGroupCoordinatorChanged(*this);
    }
    // Serial.println(String(data, len));
    request->send(200);
}

void SonosApi::writeSoapHttpCall(Stream& stream, const char* soapUrl, const char* soapAction, const char* action, String parameterXml)
{
    uint16_t contentLength = 200 + strlen(action) * 2 + strlen(soapAction) + parameterXml.length();
    // HTTP Method, URL, version
    stream.print("POST ");
    stream.print(soapUrl);
    stream.print(" HTTP/1.1\r\n");
    // Header
    stream.print("HOST: ");
    stream.print(_speakerIP.toString());
    stream.print("\r\n");
    stream.print("Content-Type: text/xml; charset=\"utf-8\"\n");
    stream.printf("Content-Length: %d\r\n", contentLength);
    stream.print("SOAPAction: ");
    stream.print(soapAction);
    stream.print("#");
    stream.print(action);
    stream.print("\r\n");
    stream.print("Connection: close\r\n");
    stream.print("\r\n");
    // Body
    stream.print(F("<s:Envelope xmlns:s='http://schemas.xmlsoap.org/soap/envelope/' s:encodingStyle='http://schemas.xmlsoap.org/soap/encoding/'><s:Body><u:"));
    stream.print(action);
    stream.print(" xmlns:u='");
    stream.print(soapAction);
    stream.print(F("'><InstanceID>0</InstanceID>"));
    stream.print(parameterXml);

    stream.print("</u:");
    stream.print(action);
    stream.print(F("></s:Body></s:Envelope>"));
}

int SonosApi::postAction(const char* soapUrl, const char* soapAction, const char* action, String parameterXml, THandlerWifiResultFunction wifiResultFunction)
{
    WiFiClient wifiClient;
    if (wifiClient.connect(_speakerIP, 1400) != true)
    {
        logErrorP("connect to %s:1400 failed", _speakerIP.toString());
        return -1;
    }

    writeSoapHttpCall(Serial, soapUrl, soapAction, action, parameterXml);
    writeSoapHttpCall(wifiClient, soapUrl, soapAction, action, parameterXml);

    auto start = millis();
    while (!wifiClient.available())
    {
        if (millis() - start > 3000)
        {
            wifiClient.stop();
            return -2;
        }
    }
    if (wifiResultFunction != nullptr)
        wifiResultFunction(wifiClient);
    else
    {
        while (auto c = wifiClient.read())
        {
            if (c == -1)
                break;
            Serial.print((char)c);
        }
    }

    Serial.println();
    wifiClient.stop();
    return 0;
}


const char* renderingControlUrl PROGMEM = "/MediaRenderer/RenderingControl/Control";
const char* renderingControlSoapAction PROGMEM = "urn:schemas-upnp-org:service:RenderingControl:1";

void SonosApi::setVolume(uint8_t volume)
{
    String parameter;
    parameter += F("<Channel>Master</Channel>");
    parameter += F("<DesiredVolume>");
    parameter += volume;
    parameter += F("</DesiredVolume>");
    postAction(renderingControlUrl, renderingControlSoapAction, "SetVolume", parameter);
}

void SonosApi::setVolumeRelative(int8_t volume)
{
    String parameter;
    parameter += F("<Channel>Master</Channel>");
    parameter += F("<Adjustment>");
    parameter += volume;
    parameter += F("</Adjustment>");
    postAction(renderingControlUrl, renderingControlSoapAction, "SetRelativeVolume", parameter);
}

uint8_t SonosApi::getVolume()
{
    uint8_t currentVolume = 0;
    String parameter;
    parameter += F("<Channel>Master</Channel>");
    postAction(renderingControlUrl, renderingControlSoapAction, "GetVolume", parameter, [=, &currentVolume](WiFiClient& wifiClient) {
        MicroXPath_P xPath;
        PGM_P path[] = {p_SoapEnvelope, p_SoapBody, "u:GetVolumeResponse", "CurrentVolume"};
        char resultBuffer[10];
        wifiClient_xPath(xPath, wifiClient, path, 4, resultBuffer, sizeof(resultBuffer));
        currentVolume = (uint8_t)constrain(atoi(resultBuffer), 0, 100);
    });
    return currentVolume;
}

void SonosApi::setMute(boolean mute)
{
    String parameter;
    parameter += F("<Channel>Master</Channel>");
    parameter += F("<DesiredMute>");
    parameter += mute ? "1" : "0";
    parameter += F("</DesiredMute>");
    postAction(renderingControlUrl, renderingControlSoapAction, "SetMute", parameter);

}

boolean SonosApi::getMute()
{
    boolean currentMute = 0;
    String parameter;
    parameter += F("<Channel>Master</Channel>");
    postAction(renderingControlUrl, renderingControlSoapAction, "GetMute", parameter, [=, &currentMute](WiFiClient& wifiClient) {
        MicroXPath_P xPath;
        PGM_P path[] = {p_SoapEnvelope, p_SoapBody, "u:GetMuteResponse", "CurrentMute"};
        char resultBuffer[10];
        wifiClient_xPath(xPath, wifiClient, path, 4, resultBuffer, sizeof(resultBuffer));
        currentMute = resultBuffer[0] == '1';
    });
    return currentMute;

}


const char* renderingGroupRenderingControlUrl PROGMEM = "/MediaRenderer/GroupRenderingControl/Control";
const char* renderingGroupRenderingControlSoapAction PROGMEM = "urn:schemas-upnp-org:service:GroupRenderingControl:1";

void SonosApi::setGroupVolume(uint8_t volume)
{
    String parameter;
    parameter += F("<DesiredVolume>");
    parameter += volume;
    parameter += F("</DesiredVolume>");
    postAction(renderingGroupRenderingControlUrl, renderingGroupRenderingControlSoapAction, "SetGroupVolume", parameter);
}

void SonosApi::setGroupVolumeRelative(int8_t volume)
{
    String parameter;
    parameter += F("<Adjustment>");
    parameter += volume;
    parameter += F("</Adjustment>");
    postAction(renderingGroupRenderingControlUrl, renderingGroupRenderingControlSoapAction, "SetRelativeGroupVolume", parameter);
}

uint8_t SonosApi::getGroupVolume()
{
    uint8_t currentGroupVolume = 0;
    String parameter;
    postAction(renderingGroupRenderingControlUrl, renderingGroupRenderingControlSoapAction, "GetGroupVolume", parameter, [=, &currentGroupVolume](WiFiClient& wifiClient) {
        MicroXPath_P xPath;
        PGM_P path[] = {p_SoapEnvelope, p_SoapBody, "u:GetGroupVolumeResponse", "CurrentVolume"};
        char resultBuffer[10];
        wifiClient_xPath(xPath, wifiClient, path, 4, resultBuffer, sizeof(resultBuffer));
        currentGroupVolume = (uint8_t)constrain(atoi(resultBuffer), 0, 100);
    });
    return currentGroupVolume;
}

void SonosApi::setGroupMute(boolean mute)
{
    String parameter;
    parameter += F("<DesiredMute>");
    parameter += mute ? "1" : "0";
    parameter += F("</DesiredMute>");
    postAction(renderingGroupRenderingControlUrl, renderingGroupRenderingControlSoapAction, "SetGroupMute", parameter);

}

boolean SonosApi::getGroupMute()
{
    boolean currentGroupMute = false;
    String parameter;
    postAction(renderingGroupRenderingControlUrl, renderingGroupRenderingControlSoapAction, "GetGroupMute", parameter, [=, &currentGroupMute](WiFiClient& wifiClient) {
        MicroXPath_P xPath;
        PGM_P path[] = {p_SoapEnvelope, p_SoapBody, "u:GetGroupMuteResponse", "CurrentMute"};
        char resultBuffer[10];
        wifiClient_xPath(xPath, wifiClient, path, 4, resultBuffer, sizeof(resultBuffer));
        currentGroupMute = resultBuffer[0] == '1';
    });
    return currentGroupMute;   
}

const char* renderingAVTransportUrl PROGMEM = "/MediaRenderer/AVTransport/Control";
const char* renderingAVTransportSoapAction PROGMEM = "urn:schemas-upnp-org:service:AVTransport:1";

void SonosApi::setAVTransportURI(const char* schema, const char* currentURI, const char* currentURIMetaData)
{
    String parameter;
    parameter += F("<CurrentURI>");
    if (schema != nullptr)
        parameter += schema;
    parameter += currentURI;
    parameter +=  F("</CurrentURI>");
    parameter += F("<CurrentURIMetaData>");
    if (currentURIMetaData != nullptr)
        parameter += currentURIMetaData;
    parameter +=  F("</CurrentURIMetaData>");
    postAction(renderingAVTransportUrl, renderingAVTransportSoapAction, "SetAVTransportURI", parameter);
}


void SonosApi::joinToGroupCoordinator(SonosApi* coordinator)
{
    if (coordinator == nullptr)
        return;
    joinToGroupCoordinator(coordinator->getUID().c_str());
}

void SonosApi::joinToGroupCoordinator(const char* uid)
{
    setAVTransportURI(sonosSchemaMaster, uid);
}


SonosApiPlayState SonosApi::getPlayState()
{
    SonosApiPlayState currentPlayState = SonosApiPlayState::Unkown;
    String parameter;
    postAction(renderingAVTransportUrl, renderingAVTransportSoapAction, "GetTransportInfo", parameter, [=, &currentPlayState](WiFiClient& wifiClient) {
        MicroXPath_P xPath;
        PGM_P path[] = {p_SoapEnvelope, p_SoapBody, "u:GetTransportInfoResponse", "CurrentTransportState"};
        char resultBuffer[20];
        wifiClient_xPath(xPath, wifiClient, path, 4, resultBuffer, sizeof(resultBuffer));
        currentPlayState = getPlayStateFromString(resultBuffer);
    });
    return currentPlayState;
}

void SonosApi::play()
{
    String parameter;
    parameter += F("<Speed>1</Speed>");
    postAction(renderingAVTransportUrl, renderingAVTransportSoapAction, "Play", parameter);
}

void SonosApi::pause()
{
    String parameter;
    postAction(renderingAVTransportUrl, renderingAVTransportSoapAction, "Pause", parameter);
}

void SonosApi::next()
{
    String parameter;
    postAction(renderingAVTransportUrl, renderingAVTransportSoapAction, "Next", parameter);
}

void SonosApi::previous()
{
    String parameter;
    postAction(renderingAVTransportUrl, renderingAVTransportSoapAction, "Previous", parameter);
}

SonosApiPlayMode SonosApi::getPlayMode()
{
    SonosApiPlayMode currentPlayMode = SonosApiPlayMode::Normal;
    String parameter;
    postAction(renderingAVTransportUrl, renderingAVTransportSoapAction, "GetTransportSettings", parameter, [=, &currentPlayMode](WiFiClient& wifiClient) {
        MicroXPath_P xPath;
        PGM_P path[] = {p_SoapEnvelope, p_SoapBody, "u:GetTransportSettingsResponse", "PlayMode"};
        char resultBuffer[20];
        wifiClient_xPath(xPath, wifiClient, path, 4, resultBuffer, sizeof(resultBuffer));
        currentPlayMode = getPlayModeFromString(resultBuffer);
    });
    return currentPlayMode;
}

void SonosApi::setPlayMode(SonosApiPlayMode playMode)
{
    String parameter;
    parameter += F("<NewPlayMode>");
    parameter += getPlayModeString(playMode);
    parameter += F("</NewPlayMode>");
    postAction(renderingAVTransportUrl, renderingAVTransportSoapAction, "SetPlayMode", parameter);
}


void SonosApi::writeSubscribeHttpCall(Stream& stream, const char* soapUrl)
{
    auto ip = _speakerIP.toString();
    // HTTP Method, URL, version
    stream.print("SUBSCRIBE ");
    stream.print(soapUrl);
    stream.print(" HTTP/1.1\r\n");
    // Header
    stream.print("HOST: ");
    stream.print(ip);
    stream.print("\r\n");
    stream.print("callback: <http://");
    stream.print(WiFi.localIP());
    stream.print("/notification/");
    stream.print(_channelIndex);
    stream.print(">\r\n");
    stream.print("NT: upnp:event\r\n");
    stream.print("Timeout: Second-");
    stream.print(_subscriptionTimeInSeconds);
    stream.print("\r\n");
    stream.print("Content-Length: 0\r\n");
    stream.print("\r\n");
}

int SonosApi::subscribeEvents(const char* soapUrl)
{
    WiFiClient wifiClient;
    if (wifiClient.connect(_speakerIP, 1400) != true)
    {
        logErrorP("connect to %s:1400 failed", _speakerIP.toString());
        return -1;
    }

    writeSubscribeHttpCall(Serial, soapUrl);
    writeSubscribeHttpCall(wifiClient, soapUrl);

    auto start = millis();
    while (!wifiClient.available())
    {
        if (millis() - start > 3000)
        {
            wifiClient.stop();
            return -2;
        }
    }
    while (auto c = wifiClient.read())
    {
        if (c == -1)
            break;
        Serial.print((char)c);
    }
    Serial.println();
    wifiClient.stop();
    return 0;
}

String SonosApi::getUID(IPAddress ipAddress)
{
    WiFiClient wifiClient;
    if (wifiClient.connect(ipAddress, 1400) != true)
    {
        return String();
    }
    wifiClient.print("GET http://");
    wifiClient.print(ipAddress.toString());
    wifiClient.print(":1400/status/zp HTTP/1.1\r\n");
    // Header
    wifiClient.print("HOST: ");
    wifiClient.print(ipAddress.toString());
    wifiClient.print("\r\n");
    wifiClient.print("Connection: close\r\n");
    wifiClient.print("\r\n");;

    auto start = millis();
    while (!wifiClient.available())
    {
        if (millis() - start > 3000)
        {
            wifiClient.stop();
            return String();
        }
    }

    PGM_P path[] = {"ZPSupportInfo", "ZPInfo", "LocalUID"};
    char resultBuffer[30];
    MicroXPath_P xPath;
    
    wifiClient_xPath(xPath, wifiClient, path, 3, resultBuffer, 30);
    wifiClient.stop();
    return resultBuffer;
}

String& SonosApi::getUID()
{
    if (_uuid.length() == 0)
    {
        _uuid = getUID(_speakerIP);
    }
    return _uuid;
}

uint32_t SonosApi::formatedTimestampToSeconds(char* durationAsString) 
{
        const char* begin = durationAsString;
        int multiplicator = 3600;
        uint32_t result = 0;
        for (size_t i = 0; ; i++)
        {
            auto c = durationAsString[i];
            if (c == ':' || c == 0)
            {
                durationAsString[i] = 0;
                result += atoi(begin) * multiplicator;
                begin = durationAsString + i + 1;
                if (multiplicator == 1 || c == 0)
                    break;
                multiplicator = multiplicator / 60;
            }
        }
        return result;
	}

const CurrentSonsTrackInfo SonosApi::getTrackInfo()
{
    CurrentSonsTrackInfo sonosTrackInfo;
    String parameter;
    postAction(renderingAVTransportUrl, renderingAVTransportSoapAction, "GetPositionInfo", parameter, [=, &sonosTrackInfo](WiFiClient& wifiClient) {
        MicroXPath_P xPath;
        const int bufferSize = 2000;
        auto resultBuffer = new char[bufferSize];
        uint16_t trackNumber;
        uint32_t duration;
        uint32_t position;
        String uri;
        String metadata;
        PGM_P pathTrack[] = {p_SoapEnvelope, p_SoapBody, "u:GetPositionInfoResponse", "Track"};
        wifiClient_xPath(xPath, wifiClient, pathTrack, 4, resultBuffer, bufferSize);
        trackNumber = atoi(resultBuffer);
        PGM_P pathDuration[] = {p_SoapEnvelope, p_SoapBody, "u:GetPositionInfoResponse", "TrackDuration"};   
        wifiClient_xPath(xPath, wifiClient, pathDuration, 4, resultBuffer, bufferSize);
        duration = formatedTimestampToSeconds(resultBuffer);
        PGM_P pathMetadata[] = {p_SoapEnvelope, p_SoapBody, "u:GetPositionInfoResponse", "TrackMetaData"};   
        wifiClient_xPath(xPath, wifiClient, pathMetadata, 4, resultBuffer, bufferSize);
        metadata = resultBuffer;
        PGM_P pathUri[] = {p_SoapEnvelope, p_SoapBody, "u:GetPositionInfoResponse", "TrackURI"};   
        wifiClient_xPath(xPath, wifiClient, pathUri, 4, resultBuffer, bufferSize);
        uri = resultBuffer;
        PGM_P pathRelTime[] = {p_SoapEnvelope, p_SoapBody, "u:GetPositionInfoResponse", "RelTime"};   
        wifiClient_xPath(xPath, wifiClient, pathRelTime, 4, resultBuffer, bufferSize);
        position = formatedTimestampToSeconds(resultBuffer);

        delete resultBuffer;
        sonosTrackInfo.trackNumber = trackNumber;
        sonosTrackInfo.duration = duration;
        sonosTrackInfo.position = position;
        sonosTrackInfo.uri = uri;
        sonosTrackInfo.metadata = metadata;
    });
    return sonosTrackInfo;
}

SonosApi* SonosApi::findGroupCoordinator(bool cached)
{
    if (cached)
    {
        if (_groupCoordinatorCached != nullptr)
            return _groupCoordinatorCached;
        if (_groupCoordinatorUuid.isEmpty())
        {
            _groupCoordinatorCached = this;
            return this;
        }
    }
    else
    {
        auto trackInfo = getTrackInfo();
        if (!trackInfo.uri.startsWith(sonosSchemaMaster))
        {
            if (!_groupCoordinatorUuid.isEmpty())
            {
                _groupCoordinatorUuid.clear();
                _groupCoordinatorCached = this;
                if (_notificationHandler != nullptr)
                    _notificationHandler->notificationGroupCoordinatorChanged(*this);
            }
            return this;
        }
        auto uidCoordinator = trackInfo.uri.substring(9);
        if (uidCoordinator != _groupCoordinatorUuid)
        {
            _groupCoordinatorUuid = uidCoordinator;
            _groupCoordinatorCached = nullptr;
            if (_notificationHandler != nullptr)
                _notificationHandler->notificationGroupCoordinatorChanged(*this);
        }
    }
    for(auto iter = AllSonosApis.begin(); iter < AllSonosApis.end(); iter++)
    {
        auto sonosApi = *iter;
        if (sonosApi != this && sonosApi->getUID() == _groupCoordinatorUuid)
        {
            _groupCoordinatorCached = sonosApi;
            return sonosApi;
        }
    }
    return nullptr;
}

SonosApi* SonosApi::findNextPlayingGroupCoordinator()
{
    std::vector<SonosApi*> allGroupCoordinators;
    allGroupCoordinators.reserve(AllSonosApis.size());
    SonosApi* ownCoordinator = nullptr;
    String uidOfOwnCoordinator;
    for(auto iter = AllSonosApis.begin(); iter < AllSonosApis.end(); iter++)
    {
        auto sonosApi = *iter;
        auto trackInfo = sonosApi->getTrackInfo();
        bool isCoordinator = !trackInfo.uri.startsWith(sonosSchemaMaster);
        if (isCoordinator)
        {
            allGroupCoordinators.push_back(sonosApi);
        }
        if (sonosApi == this)
        {
            if (isCoordinator)
            {
                ownCoordinator = this;
            }
            else
            {
                uidOfOwnCoordinator = trackInfo.uri.substring(9);
            }       
        }
    }
    bool searchPlayingGroup = false;
    // Searh playing coordinator behind own coordinator
    for(auto iter = allGroupCoordinators.begin(); iter < allGroupCoordinators.end(); iter++)
    {
        auto sonosApi = *iter;
        if (searchPlayingGroup)
        {
            auto playState = sonosApi->getPlayState();
            if (playState == SonosApiPlayState::Playing || playState == SonosApiPlayState::Transitioning)
                return sonosApi; // Next coordinator found
        }
        else
        {
            if (ownCoordinator == sonosApi)
            {
                // Own coordinator found
                searchPlayingGroup = true;
            }
            else if(!uidOfOwnCoordinator.isEmpty())
            {
                if (sonosApi->getUID() == uidOfOwnCoordinator)
                {
                    // Own coordinator found
                    searchPlayingGroup = true;
                    ownCoordinator = sonosApi;
                }
            }
        }
    }
    // Searh playing coordinator before own coordinator
    for(auto iter = allGroupCoordinators.begin(); iter < allGroupCoordinators.end(); iter++)
    {
        auto sonosApi = *iter;
        if (sonosApi == ownCoordinator)
            return nullptr; // no other playing coordinator found
        auto playState = sonosApi->getPlayState();
        if (playState == SonosApiPlayState::Playing || playState == SonosApiPlayState::Transitioning)
            return sonosApi; // Next coordinator found    
    }
    return nullptr; // no other playing coordinator found
}

SonosApi* SonosApi::findFirstParticipant(bool cached)
{
    auto uid = getUID();
    for(auto iter = AllSonosApis.begin(); iter < AllSonosApis.end(); iter++)
    {
        auto sonosApi = *iter;
        if (sonosApi != this)
        {
            if (findGroupCoordinator(cached) == this)
                return sonosApi;
        }
    }
    return nullptr;
}

void SonosApi::delegateGroupCoordinationTo(SonosApi* sonosApi, bool rejoinGroup)
{
    if (sonosApi == nullptr)
        return;
    delegateGroupCoordinationTo(sonosApi->getUID().c_str(), rejoinGroup);
}

void SonosApi::delegateGroupCoordinationTo(const char* uid, bool rejoinGroup)
{
    String parameter;
    parameter += F("<NewCoordinator>");
    parameter += uid;
    parameter += F("</NewCoordinator>");
    parameter += F("<RejoinGroup>");
    parameter += rejoinGroup ? "1" : "0";
    parameter += F("</RejoinGroup>");
    postAction(renderingAVTransportUrl, renderingAVTransportSoapAction, "DelegateGroupCoordinationTo", parameter);

}