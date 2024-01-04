#pragma once
#include <OpenKNX.h>
#include "MemoryAllocator.h"

class KoOwner
{
    protected:
        bool koSet(GroupObject& ko, const Dpt& dpt, const KNXValue& value, bool forceSend);
        void koSetWithoutSend(GroupObject& ko, const Dpt& dpt, const KNXValue& value);
        const KNXValue koGet(GroupObject& ko, const Dpt& type);
        void koSendReadRequest(GroupObject& ko, const Dpt& dpt);
        bool isKo(GroupObject& ko, GroupObject& koCompare);
        bool isKo(GroupObject& ko, GroupObject& koCompare, const Dpt& type); 
        virtual const std::string logPrefix() = 0;
    public:
        virtual const std::string name() = 0;

        static void* operator new(size_t size)
        {
            return HS_MALLOC(size);
        }
};
