// Created by mooming.go@gmail.com 2022

#pragma once

#include "LogLevel.h"
#include "Config/EngineSettings.h"
#include "HSTL/HString.h"
#include "String/StaticString.h"
#include <chrono>
#include <memory>


namespace HE
{

struct LogLine final
{
    using TTimePoint = std::chrono::time_point<std::chrono::steady_clock>;
    
    TTimePoint timeStamp;
    StaticString threadName;
    StaticString category;
    ELogLevel level;
    char text[Config::LogLineSize];
    
    inline LogLine()
        : level(ELogLevel::Info)
    {
    }
    
    inline LogLine(ELogLevel level, StaticString threadName, StaticString category, const char* inText)
        : timeStamp(std::chrono::steady_clock::now())
        , threadName(threadName)
        , category(category)
        , level(level)
    {
        if (unlikely(inText == nullptr))
        {
            text[0] = '\0';
            return;
        }

        constexpr auto LastIndex = Config::LogLineSize - 1;
        int length = 0;
        for (; length < LastIndex; ++length)
        {
            if (unlikely(inText[length] == '\0'))
                break;
        }

        std::copy(&inText[0], &inText[length], text);
        text[length] = '\0';
    }
};

} // HE
