#include "SonosChannel.h"

#define KO_VOLUME           KoSON_VOLUME_ , DPT_Scaling
#define KO_VOLUME_FEEDBACK  KoSON_VOLUME_FEEDBACK_ , DPT_Scaling

SonosChannel::SonosChannel(SonosApi* sonosApi)
    : _sonosApi(sonosApi)
{
}

const std::string SonosChannel::name() 
{
    return _sonosApi->getName();
}
const std::string SonosChannel::logPrefix()
{
    return "SonosChannel";
}
