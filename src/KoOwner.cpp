 #include "KoOwner.h"

bool KoOwner::isKo(GroupObject& ko, GroupObject& koCompare)
{
    return ko.asap() == koCompare.asap();
}

bool KoOwner::isKo(GroupObject& ko, GroupObject& koCompare, const Dpt& dpt)
{
    return isKo(ko, koCompare);
}

bool KoOwner::koSet(GroupObject& ko, const Dpt& dpt, const KNXValue& value, bool forceSend)
{
    if (forceSend || (u_int64_t) ko.value(dpt) != (u_int64_t) value)
    {
        if (forceSend)
            logInfoP("Send ko %d: %f (forced)", ko.asap(), (float) value);
        else
            logInfoP("Send ko %d: %f", ko.asap(), (float) value);    
        ko.value(value, dpt);
        return true;
    }
    return false;
}

void KoOwner::koSetWithoutSend(GroupObject& ko, const Dpt& dpt, const KNXValue& value)
{
    logDebugP("Set ko %d: %f (no send)", ko.asap(), (float) value);
    ko.valueNoSend(value, dpt);
}

const KNXValue KoOwner::koGet(GroupObject& ko, const Dpt& dpt)
{
    return ko.value(dpt);
}

void KoOwner::koSendReadRequest(GroupObject& ko, const Dpt& dpte)
{
    logInfoP("Read request for ko %d", ko.asap());
    ko.requestObjectRead();
}
