// Created by mooming.go@gmail.com, 2022

#pragma once

// Definitions for Pre-compilation

// Engine
#define ENGINE_LOG_ENABLED

// Allocator
#define __MEMORY_VERIFICATION__
#define __MEMORY_STATISTICS__
#define __MEMORY_LOGGING__
//#define __MEMORY_INVESTIGATION__
//#define __MEMORY_INVESTIGATION_LOGGING__
#define __MEMORY_DANGLING_POINTER_CHECK__
//#define __MEMORY_BUFFER_UNDERRUN_CHECK__
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

// Test
//#define __MEMORY_INVESTIGATOR_TEST__