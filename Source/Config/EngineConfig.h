// Created by mooming.go@gmail.com, 2017

#pragma once

#include <cstdint>


// Definitions for Pre-compilation
//#define __TEST__
//#define __UNIT_TEST__

// Engine
#define ENGINE_LOG_ENABLED

// Allocator
#define __MEMORY_VERIFICATION__
#define __MEMORY_STATISTICS__
//#define __MEMORY_LOGGING__
//#define __MEMORY_INVESTIGATION__
#define __MEMORY_OVERFLOW_CHECK__ 1 // 0: under-run, 1: over-run
//#define __USE_SYSTEM_MALLOC__

// Performance
#define __PERFORMANCE_COUNTER__

// Log
#define LOG_ENABLED

// Debug Control
#if !defined(NDEBUG) || defined(_DEBUG) || defined(DEBUG)
#undef __DEBUG__
#define __DEBUG__
#endif // _DEBUG

// Mathematics
//#define __LEFT_HANDED__
#define __RIGHT_HANDED__

// Default Setting Values
namespace HE
{
    namespace Config
    {
        // Engine
        static constexpr uint8_t EngineLogLevel = 1;
    
        // Memory
        static constexpr uint8_t MemLogLevel = 1;
    
        // String
        static constexpr int MaxPathLength = 512;
        static constexpr int StaticStringBufferSize = 8 * 1024 * 1024;
        static constexpr int StaticStringNumHashBuckets = 256;
    
        // Log
        static constexpr int LogBufferSize = 1024;
        static constexpr int LogMemoryBlockSize = 2048;
        static constexpr int LogNumMemoryBlocks = 2048;
    }
} // HE