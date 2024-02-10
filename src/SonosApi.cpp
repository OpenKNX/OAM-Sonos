#include "SonosApi.h"
#include "WiFi.h"
#include "WiFiClient.h"

//  Original Source of the escape function
//    FILE: XMLWriter.cpp
//  AUTHOR: Rob Tillaart
// VERSION: 0.3.5
//    DATE: 2013-11-06
// PURPOSE: Arduino library for creating XML

static char cXML[6] = "\"\'<>&";

PROGMEM const char quote[] = "&quot;";
PROGMEM const char apostrophe[] = "&apos;";
PROGMEM const char lessthen[] = "&lt;";
PROGMEM const char greaterthen[] = "&gt;";
PROGMEM const char ampersand[] = "&amp;";

PROGMEM const char* const expandedXML[] =
    {
        quote, apostrophe, lessthen, greaterthen, ampersand};

PROGMEM const char quote2[] = "&amp;quot;";
PROGMEM const char apostrophe2[] = "&amp;apos;";
PROGMEM const char lessthen2[] = "&amp;lt;";
PROGMEM const char greaterthen2[] = "&amp;gt;";
PROGMEM const char ampersand2[] = "&amp;amp;";

PROGMEM const char* const doubleExpandedXML[] =
    {
        quote2, apostrophe2, lessthen2, greaterthen2, ampersand2};

static char cURL[] = " !\"#$%&'()*+,-./:;<=>?@";

size_t ParameterBuilder::writeEncoded(const char* str, byte mode)
{
    if (str == nullptr)
        return 0;
    const char* c;
    const char* const* expanded = nullptr;
    switch (mode)
    {
        case ENCODE_XML:
            c = cXML;
            expanded = expandedXML;
            break;
        case ENCODE_DOUBLE_XML:
            c = cXML;
            expanded = doubleExpandedXML;
            break;
        case ENCODE_URL:
            c = cURL;
            break;
        default:
            if (_stream == nullptr)
                return strlen(str);
            else
                return _stream->write(str);
    }
    size_t length = 0;
    char* p = (char*)str;
    while (*p != 0)
    {
        char* q = strchr(c, *p);
        if (q == NULL)
        {
            length++;
            if (_stream != nullptr)
                _stream->write(p, 1);
        }
        else
        {
            char buf[11];
            if (mode == ENCODE_URL)
            {
                sprintf(buf, "%%%02X", (unsigned int)*p);
            }
            else
            {
                strcpy(buf, (expanded[q - c]));
            }
            auto len = strlen(buf);
            length += len;
            if (_stream != nullptr)
                _stream->write(buf, len);
        }
        p++;
    }
    return length;
}

const char p_SoapEnvelope[] PROGMEM = "s:Envelope";
const char p_SoapBody[] PROGMEM = "s:Body";
const char sonosSchemaMaster[] PROGMEM = "x-rincon:";

ParameterBuilder::ParameterBuilder(Stream* stream)
    : _stream(stream)
{
}

void ParameterBuilder::AddParameter(const char* name, int32_t value)
{
    char buffer[12];
    itoa(value, buffer, 10);
    AddParameter(name, buffer, ENCODE_NO);
}

void ParameterBuilder::AddParameter(const char* name, bool value)
{
    AddParameter(name, value ? "1" : "0", ENCODE_NO);
}

void ParameterBuilder::AddParameter(const char* name, const char* value, byte escapeMode)
{
    if (_stream == nullptr)
    {
        _length += strlen(name) * 2 + 5;
        _length += writeEncoded(value, escapeMode);
    }
    else
    {
        _length += _stream->write('<');
        _length += _stream->write(name);
        _length += _stream->write('>');
        _length += writeEncoded(value, escapeMode);
        _stream->write("</");
        _stream->write(name);
        _stream->write('>');
    }
}

void ParameterBuilder::BeginParameter(const char* name)
{
    currentParameterName = name;
    if (_stream == nullptr)
    {
        _length += strlen(name) + 2;
    }
    else
    {
        _stream->write('<');
        _stream->write(name);
        _stream->write('>');
    }
}
void ParameterBuilder::ParmeterValuePart(const char* valuePart, byte escapeMode)
{
    _length += writeEncoded(valuePart, escapeMode);     
}


void ParameterBuilder::EndParameter()
{
    if (_stream == nullptr)
    {
        _length += strlen(currentParameterName) + 3;
    }
    else
    {
        _length += _stream->write("</");
        _length += _stream->write(currentParameterName);
        _length += _stream->write('>');
    }
    currentParameterName = nullptr;
}
size_t ParameterBuilder::length()
{
    return _length;
}

std::vector<SonosApi*> SonosApi::AllSonosApis;

SonosApi::~SonosApi()
{
    auto it = std::find(AllSonosApis.begin(), AllSonosApis.end(), this);
    if (it != AllSonosApis.end())
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

boolean readFromEncodeXML(const char* encodedXML, const char* search, char* resultBuffer, size_t resultBufferSize)
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
        return (SonosApiPlayMode)((byte)SonosApiPlayMode::Repeat | (byte)SonosApiPlayMode::RepeatModeAll);
    if (strcmp(value, p_PlayModeShuffle) == 0)
        return (SonosApiPlayMode)((byte)SonosApiPlayMode::Shuffle | (byte)SonosApiPlayMode::Repeat | (byte)SonosApiPlayMode::RepeatModeAll);
    if (strcmp(value, p_PlayModeShuffleRepeatOne) == 0)
        return (SonosApiPlayMode)((byte)SonosApiPlayMode::Shuffle | (byte)SonosApiPlayMode::Repeat);
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
    if (playMode == (SonosApiPlayMode)((byte)SonosApiPlayMode::Repeat | (byte)SonosApiPlayMode::RepeatModeAll))
        return p_PlayModeRepeatAll;
    if (playMode == (SonosApiPlayMode)((byte)SonosApiPlayMode::Shuffle | (byte)SonosApiPlayMode::Repeat | (byte)SonosApiPlayMode::RepeatModeAll))
        return p_PlayModeShuffle;
    if (playMode == (SonosApiPlayMode)((byte)SonosApiPlayMode::Shuffle | (byte)SonosApiPlayMode::Repeat))
        return p_PlayModeShuffleRepeatOne;
    if (playMode == SonosApiPlayMode::Shuffle)
        return p_PlayModeShuffleNoRepeat;
    return p_PlayModeNormal;
}

const char masterVolume[] PROGMEM = "&lt;Volume channel=&quot;Master&quot; val=&quot;";
const char masterMute[] PROGMEM = "&lt;Mute channel=&quot;Master&quot; val=&quot;";
const char masterLoudness[] PROGMEM = "&lt;Loudness channel=&quot;Master&quot; val=&quot;";
const char treble[] PROGMEM = "&lt;Treble val=&quot;";
const char bass[] PROGMEM = "&lt;Bass val=&quot;";
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
    // Serial.println(String(data, len));
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
            if (readFromEncodeXML(buffer, bass, valueBuffer, valueBufferSize))
            {
                auto currentBass = constrain(atoi(valueBuffer), -10, 10);
                Serial.print("Bass: ");
                Serial.println(currentBass);
                _notificationHandler->notificationBassChanged(*this, currentBass);
            }
            if (readFromEncodeXML(buffer, treble, valueBuffer, valueBufferSize))
            {
                auto currentTreble = constrain(atoi(valueBuffer), -10, 10);
                Serial.print("Treble: ");
                Serial.println(currentTreble);
                _notificationHandler->notificationTrebleChanged(*this, currentTreble);
            }            
            if (readFromEncodeXML(buffer, masterMute, valueBuffer, valueBufferSize))
            {
                auto currentMute = valueBuffer[0] == '1';
                Serial.print("Mute: ");
                Serial.println(currentMute);
                _notificationHandler->notificationMuteChanged(*this, currentMute);
            }
            if (readFromEncodeXML(buffer, masterLoudness, valueBuffer, valueBufferSize))
            {
                auto currentMute = valueBuffer[0] == '1';
                Serial.print("Loudness: ");
                Serial.println(currentMute);
                _notificationHandler->notificationLoudnessChanged(*this, currentMute);
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
                    auto newGroupCoordinator = sonosTrackInfo->uri.substring(strlen(sonosSchemaMaster));
                    if (newGroupCoordinator != _groupCoordinatorUuid)
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

void SonosApi::writeSoapHttpCall(Stream& stream, const char* soapUrl, const char* soapAction, const char* action, TParameterBuilderFunction parameterFunction, bool addInstanceNode)
{
    uint16_t contentLength = 174 + strlen(action) * 2 + strlen(soapAction);
    if (addInstanceNode)
        contentLength += 26;
    if (parameterFunction != nullptr)
    {
        ParameterBuilder countCharParameterBuilder(nullptr);
        parameterFunction(countCharParameterBuilder);
        contentLength += countCharParameterBuilder.length();
    }
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
    stream.print("'>");
    if (addInstanceNode)
        stream.print(F("<InstanceID>0</InstanceID>"));
    if (parameterFunction != nullptr)
    {
        ParameterBuilder context(&stream);
        parameterFunction(context);
    }
    stream.print("</u:");
    stream.print(action);
    stream.print(F("></s:Body></s:Envelope>"));
}

int SonosApi::postAction(const char* soapUrl, const char* soapAction, const char* action, TParameterBuilderFunction parameterFunction, THandlerWifiResultFunction wifiResultFunction, bool addInstanceNode)
{
    WiFiClient wifiClient;
    if (wifiClient.connect(_speakerIP, 1400) != true)
    {
        logErrorP("connect to %s:1400 failed", _speakerIP.toString());
        return -1;
    }

    writeSoapHttpCall(Serial, soapUrl, soapAction, action, parameterFunction, addInstanceNode);
    Serial.println();
    writeSoapHttpCall(wifiClient, soapUrl, soapAction, action, parameterFunction, addInstanceNode);

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
    postAction(renderingControlUrl, renderingControlSoapAction, "SetVolume", [volume](ParameterBuilder& b) {
        b.AddParameter("Channel", "Master");
        b.AddParameter("DesiredVolume", volume);
    });
}

void SonosApi::setVolumeRelative(int8_t volume)
{
    postAction(renderingControlUrl, renderingControlSoapAction, "SetRelativeVolume", [volume](ParameterBuilder& b) {
        b.AddParameter("Channel", "Master");
        b.AddParameter("Adjustment", volume);
    });
}

uint8_t SonosApi::getVolume()
{
    uint8_t currentVolume = 0;
    postAction(
        renderingControlUrl, renderingControlSoapAction, "GetVolume", [](ParameterBuilder& b) { b.AddParameter("Channel", "Master"); }, [=, &currentVolume](WiFiClient& wifiClient) {
        MicroXPath_P xPath;
        PGM_P path[] = {p_SoapEnvelope, p_SoapBody, "u:GetVolumeResponse", "CurrentVolume"};
        char resultBuffer[10];
        wifiClient_xPath(xPath, wifiClient, path, 4, resultBuffer, sizeof(resultBuffer));
        currentVolume = (uint8_t)constrain(atoi(resultBuffer), 0, 100); });
    return currentVolume;
}

void SonosApi::setMute(boolean mute)
{
    postAction(renderingControlUrl, renderingControlSoapAction, "SetMute", [mute](ParameterBuilder& b) {
        b.AddParameter("Channel", "Master");
        b.AddParameter("DesiredMute", mute);
    });
}

boolean SonosApi::getMute()
{
    boolean currentMute = 0;
    postAction(
        renderingControlUrl, renderingControlSoapAction, "GetMute", [](ParameterBuilder& b) { b.AddParameter("Channel", "Master"); }, [=, &currentMute](WiFiClient& wifiClient) {
        MicroXPath_P xPath;
        PGM_P path[] = {p_SoapEnvelope, p_SoapBody, "u:GetMuteResponse", "CurrentMute"};
        char resultBuffer[10];
        wifiClient_xPath(xPath, wifiClient, path, 4, resultBuffer, sizeof(resultBuffer));
        currentMute = resultBuffer[0] == '1'; });
    return currentMute;
}

const char* devicePropertiesUrl PROGMEM = "/DeviceProperties/Control";
const char* devicePropertiesSoapAction PROGMEM = "urn:schemas-upnp-org:service:DeviceProperties:1";


void SonosApi::setStatusLight(boolean on)
{
    postAction(devicePropertiesUrl, devicePropertiesSoapAction, "SetLEDState", [on](ParameterBuilder& b) {
        b.AddParameter("DesiredLEDState", on ? "On" : "Off");
    }, nullptr, false);
}

boolean SonosApi::getStatusLight()
{
    boolean currentLEDState = 0;
    postAction(
        devicePropertiesUrl, devicePropertiesSoapAction, "GetLEDState", nullptr, [=, &currentLEDState](WiFiClient& wifiClient) {
        MicroXPath_P xPath;
        PGM_P path[] = {p_SoapEnvelope, p_SoapBody, "u:GetLEDStateResponse", "CurrentLEDState"};
        char resultBuffer[10];
        wifiClient_xPath(xPath, wifiClient, path, 4, resultBuffer, sizeof(resultBuffer));
        currentLEDState = strcmp(resultBuffer, "On") == 0;
        }, false);
    return currentLEDState;
}

int8_t SonosApi::getTreble()
{
    int8_t currentTreble = 0;
    postAction(renderingControlUrl, renderingControlSoapAction, "GetTreble", [](ParameterBuilder& b) { b.AddParameter("Channel", "Master"); }, [=, &currentTreble](WiFiClient& wifiClient) {
        MicroXPath_P xPath;
        PGM_P path[] = {p_SoapEnvelope, p_SoapBody, "u:GetTrebleResponse", "CurrentTreble"};
        char resultBuffer[10];
        wifiClient_xPath(xPath, wifiClient, path, 4, resultBuffer, sizeof(resultBuffer));
        currentTreble = (int8_t)constrain(atoi(resultBuffer), -10, 10);
    });
    return currentTreble;
}

void SonosApi::setTreble(int8_t treble)
{
    postAction(renderingControlUrl, renderingControlSoapAction, "SetTreble", [treble](ParameterBuilder& b) {
        b.AddParameter("Channel", "Master");
        b.AddParameter("DesiredTreble", treble);
    });
}

int8_t SonosApi::getBass()
{
    int8_t currentBass = 0;
    postAction(renderingControlUrl, renderingControlSoapAction, "GetBass", [](ParameterBuilder& b) { b.AddParameter("Channel", "Master"); }, [=, &currentBass](WiFiClient& wifiClient) {
        MicroXPath_P xPath;
        PGM_P path[] = {p_SoapEnvelope, p_SoapBody, "u:GetBassResponse", "CurrentBass"};
        char resultBuffer[10];
        wifiClient_xPath(xPath, wifiClient, path, 4, resultBuffer, sizeof(resultBuffer));
        currentBass = (int8_t)constrain(atoi(resultBuffer), -10, 10);
    });
    return currentBass;
}

void SonosApi::setBass(int8_t bass)
{
    postAction(renderingControlUrl, renderingControlSoapAction, "SetBass", [bass](ParameterBuilder& b) {
        b.AddParameter("Channel", "Master");
        b.AddParameter("DesiredBass", bass);
    });
}


void SonosApi::setLoudness(boolean loudness)
{
    postAction(renderingControlUrl, renderingControlSoapAction, "SetLoudness", [loudness](ParameterBuilder& b) {
        b.AddParameter("Channel", "Master");
        b.AddParameter("DesiredLoudness", loudness);
    });
}

boolean SonosApi::getLoudness()
{
    boolean currentLoudness = 0;
    postAction(
        renderingControlUrl, renderingControlSoapAction, "GetLoudness", [](ParameterBuilder& b) { b.AddParameter("Channel", "Master"); }, [=, &currentLoudness](WiFiClient& wifiClient) {
        MicroXPath_P xPath;
        PGM_P path[] = {p_SoapEnvelope, p_SoapBody, "u:GetLoudnessResponse", "CurrentLoudness"};
        char resultBuffer[10];
        wifiClient_xPath(xPath, wifiClient, path, 4, resultBuffer, sizeof(resultBuffer));
        currentLoudness = resultBuffer[0] == '1'; });
    return currentLoudness;
}

const char* renderingGroupRenderingControlUrl PROGMEM = "/MediaRenderer/GroupRenderingControl/Control";
const char* renderingGroupRenderingControlSoapAction PROGMEM = "urn:schemas-upnp-org:service:GroupRenderingControl:1";

void SonosApi::setGroupVolume(uint8_t volume)
{
    auto currentTime = millis();
    if (currentTime - _lastGroupSnapshot > 5000 || _lastGroupSnapshot == 0)
    {
        _lastGroupSnapshot = currentTime;
        postAction(renderingGroupRenderingControlUrl, renderingGroupRenderingControlSoapAction, "SnapshotGroupVolume");
    }
    postAction(renderingGroupRenderingControlUrl, renderingGroupRenderingControlSoapAction, "SetGroupVolume", [volume](ParameterBuilder& b) {
        b.AddParameter("DesiredVolume", volume);
    });
}

void SonosApi::setGroupVolumeRelative(int8_t volume)
{
    auto currentTime = millis();
    if (currentTime - _lastGroupSnapshot > 5000 || _lastGroupSnapshot == 0)
    {
        _lastGroupSnapshot = currentTime;
        postAction(renderingGroupRenderingControlUrl, renderingGroupRenderingControlSoapAction, "SnapshotGroupVolume");
    }
    postAction(renderingGroupRenderingControlUrl, renderingGroupRenderingControlSoapAction, "SetRelativeGroupVolume", [volume](ParameterBuilder& b) {
        b.AddParameter("Adjustment", volume);
    });
}

uint8_t SonosApi::getGroupVolume()
{
    uint8_t currentGroupVolume = 0;
    postAction(renderingGroupRenderingControlUrl, renderingGroupRenderingControlSoapAction, "GetGroupVolume", nullptr, [=, &currentGroupVolume](WiFiClient& wifiClient) {
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
    postAction(renderingGroupRenderingControlUrl, renderingGroupRenderingControlSoapAction, "SetGroupMute", [mute](ParameterBuilder& b) {
        b.AddParameter("DesiredMute", mute);
    });
}

boolean SonosApi::getGroupMute()
{
    boolean currentGroupMute = false;
    postAction(renderingGroupRenderingControlUrl, renderingGroupRenderingControlSoapAction, "GetGroupMute", nullptr, [=, &currentGroupMute](WiFiClient& wifiClient) {
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
    postAction(renderingAVTransportUrl, renderingAVTransportSoapAction, "SetAVTransportURI", [schema, currentURI, currentURIMetaData](ParameterBuilder& b) {
        b.BeginParameter("CurrentURI");
        b.ParmeterValuePart(schema, ParameterBuilder::ENCODE_NO);
        b.ParmeterValuePart(currentURI, ParameterBuilder::ENCODE_XML);
        b.EndParameter();
        b.AddParameter("CurrentURIMetaData", currentURIMetaData);
    });
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

void SonosApi::unjoin()
{
    postAction(renderingAVTransportUrl, renderingAVTransportSoapAction, "BecomeCoordinatorOfStandaloneGroup");
}

SonosApiPlayState SonosApi::getPlayState()
{
    SonosApiPlayState currentPlayState = SonosApiPlayState::Unkown;
    postAction(renderingAVTransportUrl, renderingAVTransportSoapAction, "GetTransportInfo", nullptr, [=, &currentPlayState](WiFiClient& wifiClient) {
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
    postAction(renderingAVTransportUrl, renderingAVTransportSoapAction, "Play", [](ParameterBuilder& b) {
        b.AddParameter("Speed", 1);
    });
}

void SonosApi::pause()
{
    postAction(renderingAVTransportUrl, renderingAVTransportSoapAction, "Pause");
}

void SonosApi::stop()
{
    postAction(renderingAVTransportUrl, renderingAVTransportSoapAction, "Stop");
}

void SonosApi::next()
{
    postAction(renderingAVTransportUrl, renderingAVTransportSoapAction, "Next");
}

void SonosApi::previous()
{
    postAction(renderingAVTransportUrl, renderingAVTransportSoapAction, "Previous");
}

SonosApiPlayMode SonosApi::getPlayMode()
{
    SonosApiPlayMode currentPlayMode = SonosApiPlayMode::Normal;
    postAction(renderingAVTransportUrl, renderingAVTransportSoapAction, "GetTransportSettings", nullptr, [=, &currentPlayMode](WiFiClient& wifiClient) {
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
    postAction(renderingAVTransportUrl, renderingAVTransportSoapAction, "SetPlayMode", [this, playMode](ParameterBuilder& b) {
        b.AddParameter("NewPlayMode", this->getPlayModeString(playMode));
    });
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
    wifiClient.print("\r\n");
    ;

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
    for (size_t i = 0;; i++)
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
    postAction(renderingAVTransportUrl, renderingAVTransportSoapAction, "GetPositionInfo", nullptr, [=, &sonosTrackInfo](WiFiClient& wifiClient) {
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
    for (auto iter = AllSonosApis.begin(); iter < AllSonosApis.end(); iter++)
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
    for (auto iter = AllSonosApis.begin(); iter < AllSonosApis.end(); iter++)
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
    for (auto iter = allGroupCoordinators.begin(); iter < allGroupCoordinators.end(); iter++)
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
            else if (!uidOfOwnCoordinator.isEmpty())
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
    for (auto iter = allGroupCoordinators.begin(); iter < allGroupCoordinators.end(); iter++)
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
    for (auto iter = AllSonosApis.begin(); iter < AllSonosApis.end(); iter++)
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
    postAction(renderingAVTransportUrl, renderingAVTransportSoapAction, "DelegateGroupCoordinationTo", [uid, rejoinGroup](ParameterBuilder& b) {
        b.AddParameter("NewCoordinator", uid);
        b.AddParameter("RejoinGroup", rejoinGroup);
    });
}

void SonosApi::gotoTrack(uint16_t trackNumber)
{
    postAction(renderingAVTransportUrl, renderingAVTransportSoapAction, "Seek", [trackNumber](ParameterBuilder& b) {
        b.AddParameter("Unit", "TRACK_NR");
        b.AddParameter("Target", trackNumber);
    });
}

void SonosApi::gotoTime(uint8_t hour, uint8_t minute, uint8_t second)
{
    postAction(renderingAVTransportUrl, renderingAVTransportSoapAction, "Seek", [hour, minute, second](ParameterBuilder& b) {
        char time[12];
        sprintf_P(time, "%d:%02d:%02d", hour, minute, second);
        b.AddParameter("Unit", "REL_TIME");
        b.AddParameter("Target", trackNumber);
    });
}

void SonosApi::addTrackToQueue(const char* scheme, const char* address, const char* metadata, uint16_t desiredTrackNumber, bool enqueueAsNext)
{
    postAction(renderingAVTransportUrl, renderingAVTransportSoapAction, "AddURIToQueue", [scheme, address, metadata, desiredTrackNumber, enqueueAsNext](ParameterBuilder& b) {
        b.BeginParameter("EnqueuedURI");
        b.ParmeterValuePart(scheme, ParameterBuilder::ENCODE_NO);
        b.ParmeterValuePart(address);
        b.EndParameter();
        b.AddParameter("EnqueuedURIMetaData", metadata);
        b.AddParameter("DesiredFirstTrackNumberEnqueued", desiredTrackNumber);
        b.AddParameter("EnqueueAsNext", enqueueAsNext);
    });
}

void SonosApi::removeAllTracksFromQueue()
{
    postAction(renderingAVTransportUrl, renderingAVTransportSoapAction, "RemoveAllTracksFromQueue");
}

const char radioMetadataBeginTitle[] PROGMEM = "<DIDL-Lite xmlns:dc='http://purl.org/dc/elements/1.1/' xmlns:upnp='urn:schemas-upnp-org:metadata-1-0/upnp/' xmlns:r='urn:schemas-rinconnetworks-com:metadata-1-0/' xmlns='urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/'><item id='R:0/0/46' parentID='R:0/0' restricted='true'><dc:title>";
const char radioMetadataEndTitleBeginImageUrl[] PROGMEM = "</dc:title><upnp:albumArtURI>";
const char radioMetadataEndImageUrl[] PROGMEM = "</upnp:albumArtURI><upnp:class>object.item.audioItem.audioBroadcast</upnp:class><desc id='cdudn' nameSpace='urn:schemas-rinconnetworks-com:metadata-1-0/'>SA_RINCON65031_</desc></item></DIDL-Lite>";

void SonosApi::playInternetRadio(const char* streamingUrl, const char* radioStationName, const char* imageUrl, const char* schema)
{
    if (schema == nullptr)
        schema = "x-rincon-mp3radio://";
    postAction(renderingAVTransportUrl, renderingAVTransportSoapAction, "SetAVTransportURI", [streamingUrl, radioStationName, imageUrl, schema](ParameterBuilder& b) {
        b.BeginParameter("CurrentURI");
        b.ParmeterValuePart(schema, ParameterBuilder::ENCODE_NO);
        b.ParmeterValuePart(streamingUrl, ParameterBuilder::ENCODE_XML);
        b.EndParameter();
        b.BeginParameter("CurrentURIMetaData");
        b.ParmeterValuePart(radioMetadataBeginTitle, ParameterBuilder::ENCODE_XML);
        b.ParmeterValuePart(radioStationName, ParameterBuilder::ENCODE_DOUBLE_XML);
        b.ParmeterValuePart(radioMetadataEndTitleBeginImageUrl, ParameterBuilder::ENCODE_XML);
        b.ParmeterValuePart(imageUrl, ParameterBuilder::ENCODE_DOUBLE_XML);
        b.ParmeterValuePart(radioMetadataEndImageUrl, ParameterBuilder::ENCODE_XML);
        b.EndParameter();
    });
    play();
}

void SonosApi::playFromHttp(const char* url)
{
    setAVTransportURI(nullptr, url);
    play();
}

void SonosApi::playMusicLibraryFile(const char* mediathekFilePath)
{
    postAction(renderingAVTransportUrl, renderingAVTransportSoapAction, "SetAVTransportURI", [mediathekFilePath](ParameterBuilder& b) {
        b.BeginParameter("CurrentURI");
        b.ParmeterValuePart("x-file-cifs:", ParameterBuilder::ENCODE_NO);
        b.ParmeterValuePart(mediathekFilePath, ParameterBuilder::ENCODE_XML);
        b.EndParameter();
        b.AddParameter("CurrentURIMetaData", nullptr);
    });
    play();
}

void SonosApi::playMusicLibraryDirectory(const char* mediathekDirectory)
{
    removeAllTracksFromQueue();
    String dir = mediathekDirectory;
    if (dir.endsWith("/"))
        dir = dir.substring(0, dir.length() - 1);
    const char* cDir = dir.c_str();

    postAction(renderingAVTransportUrl, renderingAVTransportSoapAction, "AddURIToQueue", [this, cDir](ParameterBuilder& b) {
        b.BeginParameter("EnqueuedURI");
        b.ParmeterValuePart("x-rincon-playlist:", ParameterBuilder::ENCODE_NO);
        b.ParmeterValuePart(this->getUID().c_str(), ParameterBuilder::ENCODE_NO);
        b.ParmeterValuePart("#S:", ParameterBuilder::ENCODE_NO);
        b.ParmeterValuePart(cDir, ParameterBuilder::ENCODE_NO);
        b.EndParameter();
        b.AddParameter("EnqueuedURIMetaData");
        b.AddParameter("DesiredFirstTrackNumberEnqueued", 0);
        b.AddParameter("EnqueueAsNext", true);
    });
    playQueue();
}

void SonosApi::playLineIn()
{
    setAVTransportURI("x-rincon-stream:", getUID().c_str());
    play();
}

void SonosApi::playTVIn()
{
    String url = getUID();
    url += ":spdif";
    setAVTransportURI("x-sonos-htastream:", url.c_str());
    play();
}

void SonosApi::playQueue()
{
    String url = getUID() + "#0";
    setAVTransportURI("x-rincon-queue:", url.c_str());
    play();
}
