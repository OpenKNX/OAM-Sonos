#include "SonosApi.h"
#include "WiFi.h"
#include "WiFiClient.h"

const char p_SoapEnvelope[] PROGMEM = "s:Envelope";
const char p_SoapBody[] PROGMEM = "s:Body";

void SonosApi::init(AsyncWebServer* webServer, IPAddress speakerIP, uint16_t channelIndex)
{
    _speakerIP = speakerIP;
    _channelIndex = channelIndex;
    webServer->addHandler(this);
    subscribeAll();
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
    _subscriptionTime = millis();
    if (_subscriptionTime == 0)
        _subscriptionTime = 1;
    _renderControlSeq = 0;
    subscribeEvents("/MediaRenderer/RenderingControl/Event");
    subscribeEvents("/MediaRenderer/GroupRenderingControl/Event");
}

void SonosApi::loop()
{
    if (_subscriptionTime > 0)
    {
        if (millis() - _subscriptionTime > (_subscriptionTimeInSeconds * 0.9) * 1000) // 90% of time
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

const char masterVolume[] PROGMEM = "&lt;Volume channel=&quot;Master&quot; val=&quot;";

void SonosApi::handleBody(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total)
{
    Serial.print("Notification received ");
    if (_notificationHandler != nullptr)
    {
        MicroXPath_P xPath;
        const int bufferSize = 2000;
        char* buffer = new char[bufferSize + 1];
        buffer[bufferSize] = 0;
        buffer[0] = 0;
        PGM_P pathLastChange[] = {p_PropertySet, p_Property, p_LastChange};
        charBuffer_xPath(xPath, (const char*)data, len, pathLastChange, 3, buffer, bufferSize);
        const char* begin = strstr(buffer, masterVolume);
        if (begin != nullptr)
        {
            char numberBuffer[4];
            begin += strlen(masterVolume);
            int i = 0;
            for (; i < sizeof(numberBuffer) - 1; i++)
            {
                auto c = *begin;
                if (c == '&' || c == '\0')
                    break;
                numberBuffer[i] = c;
                begin++;
            }
            numberBuffer[i] = '\0';
            _currentVolume = constrain(atoi(numberBuffer), 0, 100);
            Serial.print("Volume: ");
            Serial.println(_currentVolume);
            _notificationHandler->notificationVolumeChanged(_currentVolume);
        }
        xPath.reset();
        buffer[0] = 0;
        PGM_P pathGroupValue[] = {p_PropertySet, p_Property, "GroupVolume"};
        charBuffer_xPath(xPath, (const char*)data, len, pathGroupValue, 3, buffer, bufferSize);
        if (strlen(buffer) > 0)
        {
            _currentGroupVolume = constrain(atoi(buffer), 0, 100);
            Serial.print("Group Volume: ");
            Serial.println(_currentGroupVolume);
            _notificationHandler->notificationGroupVolumeChanged(_currentGroupVolume);
        }
        delete buffer;
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



uint8_t SonosApi::getVolume()
{
    _currentVolume = 0;
    String parameter;
    parameter += F("<Channel>Master</Channel>");
    postAction(renderingControlUrl, renderingControlSoapAction, "GetVolume", parameter, [=](WiFiClient& wifiClient) {
        MicroXPath_P xPath;
        PGM_P path[] = {p_SoapEnvelope, p_SoapBody, "u:GetVolumeResponse", "CurrentVolume"};
        char resultBuffer[10];
        wifiClient_xPath(xPath, wifiClient, path, 4, resultBuffer, sizeof(resultBuffer));
        _currentVolume = (uint8_t)constrain(atoi(resultBuffer), 0, 100);
    });
    return _currentVolume;
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


uint8_t SonosApi::getGroupVolume()
{
    _currentGroupVolume = 0;
    String parameter;
    postAction(renderingGroupRenderingControlUrl, renderingGroupRenderingControlSoapAction, "GetGroupVolume", parameter, [=](WiFiClient& wifiClient) {
        MicroXPath_P xPath;
        PGM_P path[] = {p_SoapEnvelope, p_SoapBody, "u:GetGroupVolumeResponse", "CurrentVolume"};
        char resultBuffer[10];
        wifiClient_xPath(xPath, wifiClient, path, 4, resultBuffer, sizeof(resultBuffer));
        _currentGroupVolume = (uint8_t)constrain(atoi(resultBuffer), 0, 100);
    });
    return _currentGroupVolume;
}

const char* renderingAVTransportUrl PROGMEM = "/MediaRenderer/AVTransport/Control";
const char* renderingAVTransportSoapAction PROGMEM = "urn:schemas-upnp-org:service:AVTransport:1";

SonosApiPlayState SonosApi::getPlayState()
{
    _currentPlayState = SonosApiPlayState::Unkown;
    String parameter;
    postAction(renderingAVTransportUrl, renderingAVTransportSoapAction, "GetTransportInfo", parameter, [=](WiFiClient& wifiClient) {
        MicroXPath_P xPath;
        PGM_P path[] = {p_SoapEnvelope, p_SoapBody, "u:GetTransportInfoResponse", "CurrentTransportState"};
        char resultBuffer[20];
        wifiClient_xPath(xPath, wifiClient, path, 4, resultBuffer, sizeof(resultBuffer));
        if (strcmp(resultBuffer, "STOPPED") == 0)
            _currentPlayState = SonosApiPlayState::Stopped;
        else if (strcmp(resultBuffer, "PLAYING") == 0)
            _currentPlayState = SonosApiPlayState::Playing;
        else if (strcmp(resultBuffer, "PAUSED_PLAYBACK") == 0)
            _currentPlayState = SonosApiPlayState::Paused_Playback;
        else if (strcmp(resultBuffer, "TRANSITIONING") == 0)
            _currentPlayState = SonosApiPlayState::Transitioning;
    });
    return _currentPlayState;
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

String& SonosApi::getUID()
{
    if (_uuid.length() == 0)
    {
        WiFiClient wifiClient;
        if (wifiClient.connect(_speakerIP, 1400) != true)
        {
            logErrorP("connect to %s:1400 failed", _speakerIP.toString());
            return _uuid;
        }
        wifiClient.print("GET http://");
        wifiClient.print(_speakerIP.toString());
        wifiClient.print(":1400/status/zp HTTP/1.1\r\n");
        // Header
        wifiClient.print("HOST: ");
        wifiClient.print(_speakerIP.toString());
        wifiClient.print("\r\n");
        wifiClient.print("Connection: close\r\n");
        wifiClient.print("\r\n");;

        auto start = millis();
        while (!wifiClient.available())
        {
            if (millis() - start > 3000)
            {
                wifiClient.stop();
                return _uuid;
            }
        }
    
        PGM_P path[] = {"ZPSupportInfo", "ZPInfo", "LocalUID"};
        char resultBuffer[30];
        MicroXPath_P xPath;
       
        wifiClient_xPath(xPath, wifiClient, path, 3, resultBuffer, 30);
        _uuid = resultBuffer;
        wifiClient.stop();
    }
    return _uuid;
}

 uint32_t formatedTimestampToSeconds(char* durationAsString) {
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

SonosTrackInfo* SonosApi::getTrackInfo()
{
    String parameter;
    postAction(renderingAVTransportUrl, renderingAVTransportSoapAction, "GetPositionInfo", parameter, [=](WiFiClient& wifiClient) {
        MicroXPath_P xPath;
        const int bufferSize = 2000;
        auto resultBuffer = new char[bufferSize];
        uint16_t queueIndex;
        uint32_t duration;
        uint32_t position;
        String uri;
        String metadata;
        PGM_P pathTrack[] = {p_SoapEnvelope, p_SoapBody, "u:GetPositionInfoResponse", "Track"};
        wifiClient_xPath(xPath, wifiClient, pathTrack, 4, resultBuffer, bufferSize);
        queueIndex = atoi(resultBuffer);
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
        _currentTrackInfo = new SonosTrackInfo(queueIndex, duration, position, uri, metadata);
   
    });
    if (_currentTrackInfo == nullptr)
    {
        auto empty = String();
        _currentTrackInfo = new SonosTrackInfo(0, 0, 0, empty, empty);
    }
    return _currentTrackInfo;
}

