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
const char masterMute[] PROGMEM = "&lt;Mute channel=&quot;Master&quot; val=&quot;";

boolean readFromEncodeXML(const char* encodedXML, const char* search,  char* resultBuffer, size_t resultBufferSize)
{
    if (resultBufferSize > 0)
        resultBuffer[0] = '\0';
    const char* begin = strstr(encodedXML, search);
    if (begin != nullptr)
    {
        begin += strlen(masterVolume);
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
        char numberBuffer[4];
        if (readFromEncodeXML(buffer, masterVolume, numberBuffer, sizeof(numberBuffer)))
        {
            auto currentVolume = constrain(atoi(numberBuffer), 0, 100);
            Serial.print("Volume: ");
            Serial.println(currentVolume);
            _notificationHandler->notificationVolumeChanged(currentVolume);
        }
        if (readFromEncodeXML(buffer, masterMute, numberBuffer, sizeof(numberBuffer)))
        {
            auto currentMute = numberBuffer[0] == '1';
            Serial.print("Mute: ");
            Serial.println(currentMute);
            _notificationHandler->notificationMuteChanged(currentMute);
        }
        xPath.reset();
        buffer[0] = 0;
        PGM_P pathGroupVolume[] = {p_PropertySet, p_Property, "GroupVolume"};
        charBuffer_xPath(xPath, (const char*)data, len, pathGroupVolume, 3, buffer, bufferSize);
        if (strlen(buffer) > 0)
        {
            auto currentGroupVolume = constrain(atoi(buffer), 0, 100);
            Serial.print("Group Volume: ");
            Serial.println(currentGroupVolume);
            _notificationHandler->notificationGroupVolumeChanged(currentGroupVolume);
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
            _notificationHandler->notificationGroupMuteChanged(currentGroupMute);
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
    postAction(renderingGroupRenderingControlUrl, renderingGroupRenderingControlSoapAction, "SetGroupVolume", parameter);

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

SonosApiPlayState SonosApi::getPlayState()
{
    SonosApiPlayState currentPlayState = SonosApiPlayState::Unkown;
    String parameter;
    postAction(renderingAVTransportUrl, renderingAVTransportSoapAction, "GetTransportInfo", parameter, [=, &currentPlayState](WiFiClient& wifiClient) {
        MicroXPath_P xPath;
        PGM_P path[] = {p_SoapEnvelope, p_SoapBody, "u:GetTransportInfoResponse", "CurrentTransportState"};
        char resultBuffer[20];
        wifiClient_xPath(xPath, wifiClient, path, 4, resultBuffer, sizeof(resultBuffer));
        if (strcmp(resultBuffer, "STOPPED") == 0)
            currentPlayState = SonosApiPlayState::Stopped;
        else if (strcmp(resultBuffer, "PLAYING") == 0)
            currentPlayState = SonosApiPlayState::Playing;
        else if (strcmp(resultBuffer, "PAUSED_PLAYBACK") == 0)
            currentPlayState = SonosApiPlayState::Paused_Playback;
        else if (strcmp(resultBuffer, "TRANSITIONING") == 0)
            currentPlayState = SonosApiPlayState::Transitioning;
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

const SonosTrackInfo SonosApi::getTrackInfo()
{
    SonosTrackInfo sonosTrackInfo;
    String parameter;
    postAction(renderingAVTransportUrl, renderingAVTransportSoapAction, "GetPositionInfo", parameter, [=, &sonosTrackInfo](WiFiClient& wifiClient) {
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
        sonosTrackInfo.queueIndex = queueIndex;
        sonosTrackInfo.duration = duration;
        sonosTrackInfo.position = position;
        sonosTrackInfo.uri = uri;
        sonosTrackInfo.metadata = metadata;
    });
    return sonosTrackInfo;
}

