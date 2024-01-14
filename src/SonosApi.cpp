#include "SonosApi.h"
#include "WiFiClient.h"
#include "WiFi.h"

void SonosApi::writeSoapHttpCall(Stream& stream, IPAddress& ipAddress, const char* soapUrl, const char* soapAction, const char* action, String parameterXml)
{
    uint16_t contentLength = 200 + strlen(action) * 2 + strlen(soapAction) + parameterXml.length();
    // HTTP Method, URL, version
    stream.print("POST ");
    stream.print(soapUrl);
    stream.print(" HTTP/1.1\r\n");
    // Header
    stream.print("HOST: ");
    stream.print(ipAddress.toString());
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
    stream.print (parameterXml);

    stream.print("</u:");
    stream.print(action);
    stream.print(F("></s:Body></s:Envelope>"));
}



int SonosApi::postAction(IPAddress& speakerIP, const char* soapUrl, const char* soapAction, const char* action, String parameterXml)
{
    WiFiClient wifiClient;
    if (wifiClient.connect(speakerIP, 1400) != true)
    {
        logErrorP("connect to %s:1400 failed", speakerIP.toString().c_str());
        return -1;
    }
   
    writeSoapHttpCall(Serial, speakerIP, soapUrl, soapAction, action, parameterXml);
    writeSoapHttpCall(wifiClient, speakerIP, soapUrl, soapAction, action, parameterXml);
   
    auto start = millis();
    while (!wifiClient.available())
    {
        if (millis() - start > 3000)
        {
            return -2;
        }
    }
    while (auto c = wifiClient.read()) 
    {
        if (c == -1)
            break;
        Serial.print((char) c);
    }
    
    Serial.println();
    wifiClient.stop();
    return 0;
}

const char* renderingControlUrl PROGMEM = "/MediaRenderer/RenderingControl/Control";
const char* renderingControlSoapAction PROGMEM = "urn:schemas-upnp-org:service:RenderingControl:1";

void SonosApi::setVolume(IPAddress& speakerIP, uint8_t volume)
{
    String parameter;
    parameter += F("<Channel>Master</Channel>");
    parameter += F("<DesiredVolume>");
    parameter += volume;
    parameter += F("</DesiredVolume>");
    postAction(speakerIP, renderingControlUrl, renderingControlSoapAction, "SetVolume",  parameter);
}


uint8_t SonosApi::getVolume(IPAddress& speakerIP)
{
   return 0;
}

void SonosApi::writeSubscribeHttpCall(Stream& stream, IPAddress& ipAddress, const char* soapUrl)
{
    auto ip = ipAddress.toString();
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
    stream.print("/>\r\n");
    stream.print("NT: upnp:event\r\n");
    stream.print("Timeout: Second-3600\r\n");
    stream.print("Content-Length: 0\r\n");
    stream.print("\r\n");
}

int SonosApi::subscribeEvents(IPAddress& speakerIP, const char* soapUrl)
{
    WiFiClient wifiClient;
    if (wifiClient.connect(speakerIP, 1400) != true)
    {
        logErrorP("connect to %s:1400 failed", speakerIP.toString());
        return -1;
    }
   
    writeSubscribeHttpCall(Serial, speakerIP, soapUrl);
    writeSubscribeHttpCall(wifiClient, speakerIP, soapUrl);

    auto start = millis();
    while (!wifiClient.available())
    {
        if (millis() - start > 3000)
        {
            return -2;
        }
    }
    while (auto c = wifiClient.read()) 
    {
        if (c == -1)
            break;
        Serial.print((char) c);
    }
    Serial.println();
    wifiClient.stop();
    return 0;
}

void SonosApi::subscribeAVTransport(IPAddress& ipAddress)
{
    subscribeEvents(ipAddress, "/MediaRenderer/AVTransport/Event");
}