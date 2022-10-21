// Created by mooming.go@gmail.com, 2022

#pragma once

#include "PoolAllocator.h"
#include "HSTL/HVector.h"
#include "String/StaticString.h"


namespace HE
{
class MultiPoolAllocator final
{
public:
    using This = MultiPoolAllocator;
    using TPoolConfig = std::pair<size_t, size_t>;
    
private:
    using TInitializerList = std::initializer_list<TPoolConfig>;
    
    TAllocatorID id;
    StaticString name;
    HSTL::HVector<PoolAllocator<uint32_t>> multiPool;
    size_t fallbackCount;
    
public:
    MultiPoolAllocator(const char* name, TInitializerList list);
    ~MultiPoolAllocator();
    
    void* Allocate(size_t size);
    void Deallocate(void* ptr);
    
    inline auto GetName() const { return name; }
    inline auto GetID() const { return id; }
    inline auto GetFallbackCount() const { return fallbackCount; }
    
    size_t GetUsage() const;
    size_t GetAvailableMemory() const;
    size_t GetCapacity() const;
    
    void PrintUsage() const;
    
private:
    size_t GetPoolIndex(size_t nBytes) const;
    size_t GetPoolIndex(void* ptr) const;
};
} // HE

#ifdef __UNIT_TEST__

#include "Test/TestCollection.h"


namespace HE
{
class MultiPoolAllocatorTest : public TestCollection
{
public:
    MultiPoolAllocatorTest() : TestCollection("MultiPoolAllocatorTest")
    {
    }
    
protected:
    virtual void Prepare() override;
};
} // HE
#endif //__UNIT_TEST__
