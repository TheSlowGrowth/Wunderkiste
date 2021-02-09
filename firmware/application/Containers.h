#pragma once
#include <stdint.h>
#include <cstddef>
#include <algorithm>
#include <utility>

template <typename T>
class LateInitializedObject
{
public:
    LateInitializedObject() :
        object_(nullptr)
    {
    }

    template<typename ...Args>
    void create(Args&&... args) 
    {
        object_ = new (::std::addressof(storage_)) T(std::forward<Args>(args)...);
    }

    operator T*() { return object_; }
    operator const T*() const { return object_; }
    T* operator->() { return object_; }
    const T* operator->() const { return object_; }

private:
    typename std::aligned_storage<sizeof(T)>::type storage_;
    T* object_;
};

template <class ValueType, std::size_t capacity>
class StaticVector
{
private:
    ValueType arr_[capacity];
    std::size_t size_;

public:
    StaticVector() :
        size_(0)
    {
    }

    ValueType& operator[](std::size_t index)
    {
        const auto clampedIndex = std::clamp(index, std::size_t(0), size_ - 1);
        return arr_[clampedIndex];
    }

    void clear()
    {
        size_ = 0;
    }

    void remove(std::size_t index)
    {
        if (index >= 0 && index < size_)
        {
            for (std::size_t i = index; i < size_ - 1; i++)
            {
                arr_[i] = arr_[i + 1];
            }
            size_--;
        }
    }

    void sortAscending()
    {
        // "simplesort" algorithm
        for (int targetIdx = size_ - 1; targetIdx >= 0; targetIdx--)
        {
            for (int probeIdx = 0; probeIdx < targetIdx; probeIdx++)
            {
                if (arr_[probeIdx] > arr_[targetIdx])
                {
                    auto tmp = arr_[probeIdx];
                    arr_[probeIdx] = arr_[targetIdx];
                    arr_[targetIdx] = tmp;
                }
            }
        }
    }

    void add(ValueType val)
    {
        if (size_ < capacity)
        {
            arr_[size_] = val;
            size_++;
        }
    }

    bool contains(const ValueType& value) const
    {
        for (std::size_t i = 0; i < size_; i++)
        {
            if (arr_[i] == value)
                return true;
        }
        return false;
    }

    bool findAndRemove(const ValueType& value)
    {
        for (std::size_t i = 0; i < size_; i++)
        {
            if (arr_[i] == value)
            {
                remove(i);
                return true;
            }
        }
        return false;
    }

    std::size_t size() const
    {
        return size_;
    }

    std::size_t getCapacity() const
    {
        return capacity;
    }
};