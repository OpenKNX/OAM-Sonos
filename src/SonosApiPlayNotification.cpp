#if ARDUINO_ARCH_ESP32
#include "SonosApiPlayNotification.h"

WebSocketsClient* SonosApiPlayNotification::_webSocket = nullptr;

SonosApiPlayNotification::SonosApiPlayNotification(IPAddress& speakerIP, const char* streamUrl, byte volume, String& playerId)
: _speakerIP(speakerIP), _streamUrl(streamUrl), _playerId(playerId), _volume(volume)
{
    checkFinished();
}
SonosApiPlayNotification::~SonosApiPlayNotification()
{
    if (_state == 1)
    {
        delete _webSocket;
        _webSocket = nullptr;
    }
}
bool SonosApiPlayNotification::checkFinished()
{
    if (_state < 2)
    {
        if (millis() - _started > 2000)
        {
            // timeout reached
            if (_state == 1)
            {
                delete _webSocket;
                _webSocket = nullptr;
            }
            _state = 3;
        }
    }
    switch (_state)
    {
        case 0: // websocket not yet started
        if (_webSocket == nullptr)
        {
            _webSocket = new WebSocketsClient();
            _webSocket->setExtraHeaders("X-Sonos-Api-Key: 123e4567-e89b-12d3-a456-426655440000");
            _webSocket->beginSSL(_speakerIP.toString().c_str(), 1443, "/websocket/api", "", "v1.api.smartspeaker.audio");
            _state = 1;
        }
        else
            break;
        case 1: // websocket started
        _webSocket->loop();
        if (_webSocket->isConnected())
        {
            String text;
            text += "[{\"namespace\":\"audioClip:1\",\"command\":\"loadAudioClip\",\"playerId\":\"";
            text += _playerId;
            text += "\",\"sessionId\":null,\"cmdId\":null},{\"name\": \"Sonos TS Notification\", \"appId\": \"openknx\", \"streamUrl\": \"";
            text += _streamUrl;
            text += "\", \"volume\":";
            text += _volume;
            text += "}]";
            _webSocket->sendTXT(text);
            delete _webSocket;
            _webSocket = nullptr;
            _state = 2;
        }
        else
            break;
        case 2: // finished
            return true;
        case 3: // timed out
            return true;
    }
    return false;
}
#endif
