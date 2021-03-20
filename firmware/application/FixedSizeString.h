#pragma once

#include <string_view>
#include <algorithm>

template <std::size_t capacity, class CharType = char, class Traits = std::char_traits<CharType>>
class FixedSizeStr
{
public:
    constexpr FixedSizeStr() noexcept = default;

    template <size_t otherSize>
    constexpr FixedSizeStr(const FixedSizeStr<otherSize> str) :
        size_(std::min(str.size(), capacity))
    {
        copy_(str.data(), str.data() + size_, buffer_);
    }

    constexpr FixedSizeStr(const CharType* str) :
        size_(std::min(Traits::length(str), capacity))
    {
        copy_(str, str + size_, buffer_);
    }

    constexpr FixedSizeStr(const CharType* str, std::size_t length) :
        size_(std::min(length, capacity))
    {
        copy_(str, str + size_, buffer_);
    }

    template <size_t otherSize>
    constexpr FixedSizeStr& operator=(const FixedSizeStr<otherSize> str)
    {
        size_ = std::min(str.size(), capacity);
        copy_(str.data(), str.data() + size_, buffer_);
        buffer_[size_] = 0;
        return *this;
    }

    constexpr FixedSizeStr& operator=(const CharType* str)
    {
        size_ = std::min(Traits::length(str), capacity);
        copy_(str, str + size_, buffer_);
        buffer_[size_] = 0;
        return *this;
    }

    constexpr operator const CharType*() const noexcept { return buffer_; }
    constexpr const CharType* c_str() const noexcept { return buffer_; }
    constexpr const CharType* data() const noexcept { return buffer_; }
    constexpr CharType* data() noexcept { return buffer_; }

    constexpr auto size() const noexcept { return size_; }
    constexpr auto updateSize() noexcept
    {
        size_ = std::min(Traits::length(buffer_), capacity);
        buffer_[size_] = 0;
    }

    constexpr auto maxSize() const noexcept { return capacity; }

    constexpr auto empty() const noexcept { return size_ == 0; }

    constexpr void clear() noexcept
    {
        size_ = 0;
        buffer_[0] = '\0';
    }

    constexpr void reset(const CharType* str)
    {
        size_ = std::min(Traits::length(str), capacity);
        reset_(str, size_);
    }

    constexpr void reset(const CharType* str, std::size_t length)
    {
        size_ = std::min(length, capacity);
        reset_(str, size_);
    }

    constexpr void resetAt(const CharType* str, std::size_t writePosition)
    {
        if (writePosition > size_)
            return;
        const auto strLen = Traits::length(str);
        const auto newSize = std::max(std::min(strLen + writePosition, capacity), size_);
        const auto numCharsToWrite = std::min(newSize - writePosition, strLen);
        size_ = newSize;
        resetAt_(str, numCharsToWrite, writePosition);
    }

    constexpr void append(const CharType singleChar)
    {
        if (size_ < capacity)
        {
            buffer_[size_] = singleChar;
            size_++;
        }
    }

    constexpr void append(const CharType* str)
    {
        auto to_copy = std::min(Traits::length(str), (capacity - size_));
        append_(str, to_copy);
    }

    constexpr void append(const CharType* str, std::size_t length)
    {
        auto to_copy = std::min(length, (capacity - size_));
        append_(str, to_copy);
    }

    constexpr bool startsWith(const CharType* pattern) const noexcept
    {
        const CharType* ptr = buffer_;
        while (*pattern)
        {
            if (*ptr != *pattern)
                return false;
            pattern++;
            ptr++;
        }
        return true;
    }

    constexpr bool startsWithIgnoringCase(const CharType* pattern) const noexcept
    {
        const CharType* ptr = buffer_;
        while (*pattern)
        {
            if (toUpper_(*ptr) != toUpper_(*pattern))
                return false;
            pattern++;
            ptr++;
        }
        return true;
    }

    constexpr bool endsWith(const CharType* pattern) const noexcept
    {
        const CharType* ptr = &buffer_[size_ - 1];
        const auto patternLength = Traits::length(pattern);
        const CharType* patternPtr = pattern + patternLength - 1;
        while (patternPtr > pattern)
        {
            if (*ptr != *patternPtr)
                return false;
            patternPtr--;
            ptr--;
        }
        return *ptr == *patternPtr;
    }

    constexpr bool endsWithIgnoringCase(const CharType* pattern) const noexcept
    {
        const CharType* ptr = &buffer_[size_ - 1];
        const auto patternLength = Traits::length(pattern);
        const CharType* patternPtr = pattern + patternLength - 1;
        while (patternPtr > pattern)
        {
            if (toUpper_(*ptr) != toUpper_(*patternPtr))
                return false;
            patternPtr--;
            ptr--;
        }
        return toUpper_(*ptr) == toUpper_(*patternPtr);
    }

    constexpr void removePrefix(std::size_t length)
    {
        copy_(buffer_ + length, buffer_ + size_, buffer_);
        size_ -= length;
        buffer_[size_] = '\0';
    }

    constexpr void removeSuffix(std::size_t length) noexcept
    {
        size_ = size_ - length;
        buffer_[size_] = '\0';
    }

    constexpr bool operator==(const CharType* rhs) const
    {
        const CharType* ptr = buffer_;
        while (*rhs && *ptr) // abort on first string end
        {
            if (*ptr != *rhs)
                return false;
            rhs++;
            ptr++;
        }

        return *rhs == *ptr; // both strings ended at the same '0'?
    }

    constexpr bool operator!=(const CharType* rhs) const
    {
        return !(*this == rhs);
    }

    constexpr bool operator<(const CharType* other) const
    {
        auto ptr = buffer_;
        while (*ptr && *other && (*ptr == *other))
        {
            ptr++;
            other++;
        }
        return *ptr < *other;
    }

    constexpr bool operator<=(const CharType* other) const
    {
        return (*this == other) || (*this < other);
    }

    constexpr bool operator>(const CharType* other) const
    {
        auto ptr = buffer_;
        while (*ptr && *other && (*ptr == *other))
        {
            ptr++;
            other++;
        }
        return *ptr > *other;
    }

    constexpr bool operator>=(const CharType* other) const
    {
        return (*this == other) || (*this > other);
    }

    constexpr void swap(FixedSizeStr& rhs) noexcept
    {
        auto tmp = size_;
        size_ = rhs.size_;
        rhs.size_ = tmp;

        swap_(buffer_, rhs.buffer_, std::max(size_, rhs.size_));
    }

private:
    constexpr void reset_(const CharType* str, std::size_t length)
    {
        copy_(str, str + length, buffer_);
        buffer_[length] = '\0';
    }

    constexpr void resetAt_(const CharType* str, std::size_t strLen, std::size_t writePosition)
    {
        copy_(str, str + strLen, buffer_ + writePosition);
        if (writePosition + strLen > size_)
            buffer_[writePosition + strLen] = '\0';
    }

    constexpr void append_(const CharType* str, std::size_t to_copy)
    {
        copy_(str, str + to_copy, buffer_ + size_);
        size_ += to_copy;
        buffer_[size_] = '\0';
    }

    static constexpr void copy_(const CharType* src, const CharType* srcEnd, CharType* dest)
    {
        while (src != srcEnd)
        {
            *dest = *src;
            src++;
            dest++;
        }
    }

    static constexpr void swap_(CharType* a, CharType* b, size_t length)
    {
        for (size_t i = 0; i < length; i++)
        {
            CharType tmp = *a;
            *a = *b;
            *b = tmp;
            a++;
            b++;
        }
    }

    // TODO: add wstring version
    static constexpr char toUpper_(char c) noexcept
    {
        switch (c)
        {
            default:
                return c;
            case 'a':
                return 'A';
            case 'b':
                return 'B';
            case 'c':
                return 'C';
            case 'd':
                return 'D';
            case 'e':
                return 'E';
            case 'f':
                return 'F';
            case 'g':
                return 'G';
            case 'h':
                return 'H';
            case 'i':
                return 'I';
            case 'j':
                return 'J';
            case 'k':
                return 'K';
            case 'l':
                return 'L';
            case 'm':
                return 'M';
            case 'n':
                return 'N';
            case 'o':
                return 'O';
            case 'p':
                return 'P';
            case 'q':
                return 'Q';
            case 'r':
                return 'R';
            case 's':
                return 'S';
            case 't':
                return 'T';
            case 'u':
                return 'U';
            case 'v':
                return 'V';
            case 'w':
                return 'W';
            case 'x':
                return 'X';
            case 'y':
                return 'Y';
            case 'z':
                return 'Z';
        }
    }

    std::size_t size_ { 0 };
    CharType buffer_[capacity + 1] {};
};

template <class CharType, std::size_t capacity, class Traits = std::char_traits<CharType>>
inline constexpr void swap(const FixedSizeStr<capacity, CharType>& lhs, const FixedSizeStr<capacity, CharType>& rhs) noexcept
{
    rhs.swap(lhs);
}
