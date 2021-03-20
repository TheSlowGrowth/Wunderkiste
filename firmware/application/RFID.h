#pragma once
#include "FixedSizeString.h"
#include "UiEventQueue.h"

class RfidTagId
{
public:
    static RfidTagId invalid() { return RfidTagId(); }

    RfidTagId() :
        id_(0),
        isValid_(false)
    {
    }

    RfidTagId(uint32_t id) :
        id_(id),
        isValid_(true)
    {
    }

    RfidTagId(const RfidTagId& other) :
        id_(other.id_),
        isValid_(other.isValid_)
    {
    }

    bool isValid() const { return isValid_; }

    operator uint32_t() const { return id_; }
    bool operator==(const RfidTagId& other)
    {
        return (!isValid_ && !other.isValid_)
               || id_ == other.id_;
    }
    RfidTagId& operator=(const RfidTagId& other)
    {
        id_ = other.id_;
        isValid_ = other.isValid_;
        return *this;
    }

    FixedSizeStr<8> asString() const
    {
        if (!isValid_)
            return "";

        FixedSizeStr<8> result;
        static constexpr char hexChars[] = "0123456789ABCDEF";
        // store the ID first
        for (int nibble = 7; nibble >= 0; nibble--)
        {
            const auto shift = nibble * 4;
            result.append(hexChars[(id_ >> shift) & 0x0F]);
        }
        return result;
    }

private:
    uint32_t id_;
    bool isValid_;
};

class RfidReader
{
public:
    static void init();
    static void readAndGenerateEvents(UiEventQueue& queue);
};