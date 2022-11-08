// Created by mooming.go@gmail.com 2016

#include "StringUtil.h"

#include "InlineStringBuilder.h"
#include "Log/Logger.h"
#include "Memory/AllocatorScope.h"
#include "Memory/InlinePoolAllocator.h"
#include <algorithm>
#include <memory>

#ifdef _MSC_VER
#include <limits.h>
#include <windows.h>
#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#endif // PATH_MAX
#endif // _MSC_VER


using namespace HSTL;

namespace StringUtil
{

TString Trim(const TString& str)
{
    auto start = str.begin();
    auto end = str.end();
    
    while (start != end && std::isspace(*start))
        ++start;
    
    do
    {
        --end;
    } while (std::distance(start, end) > 0 && std::isspace(*end));
    
    return TString(start, end + 1);
}

TString TrimPath(const TString& path)
{
    char tmp[PATH_MAX + 1];
    auto cStr = path.c_str();
    
    for (size_t i = 0; i < path.length(); ++i)
    {
        if (cStr[i] == '\\')
        {
            tmp[i] = '/';
        }
        else
        {
            tmp[i] = cStr[i];
        }
    }
    
    if (path.length() > 2 && tmp[path.length() - 2] != '.' && tmp[path.length() - 1] == '/')
    {
        tmp[path.length() - 1] = '\0';
    }
    
    tmp[path.length()] = '\0';
    
    return TString(tmp);
}

TString ToLowerCase(TString src)
{
    auto ToLowerChar = [](char ch)
    {
        constexpr char delta = 'A' - 'a';
        
        if ('A' <= ch && ch <= 'Z')
        {
            ch -= delta;
        }
        
        return ch;
    };
    
    std::transform(src.begin(), src.end(), src.begin(), ToLowerChar);
    
    return src;
}

bool EqualsIgnoreCase(const TString& a, const TString& b)
{
    return ToLowerCase(a) == ToLowerCase(b);
}

bool StartsWith(const TString& src, const TString& startTerm)
{
    if (src.length() < startTerm.length())
        return false;
    
    TString head = src.substr(0, startTerm.length());
    
    return head == startTerm;
}

bool StartsWithIgnoreCase(const TString& src, const TString& startTerm)
{
    return StartsWith(ToLowerCase(src), ToLowerCase(startTerm));
}

bool EndsWith(const TString& src, const TString& endTerm)
{
    if (src.length() < endTerm.length())
        return false;
    
    const char* srcStr = src.c_str();
    TString last(srcStr + (src.length() - endTerm.length()));
    
    return last == endTerm;
}

bool EndsWithIgnoreCase(const TString& src, const TString& endTerm)
{
    return EndsWith(ToLowerCase(src), ToLowerCase(endTerm));
}

TString PathToName(const TString& path)
{
    auto length = path.size();
    using Index = decltype(length);
    
    auto buffer = path.c_str();
    
    bool found = false;
    Index lastIndex = 0;
    
    for (Index i = 0; i < length; ++i)
    {
        const char ch = buffer[i];
        if (ch == '/' || ch == '\\')
        {
            lastIndex = i;
            found = true;
        }
    }
    
    if (!found || lastIndex >= (length - 1))
    {
        return path;
    }
    
    return TString(buffer + lastIndex + 1);
}

void ForEachToken(const char* str
    , const std::function<void(std::string_view)> func, const char* separators)
{
    using namespace HE;

    if (unlikely(str == nullptr))
        return;

    if (unlikely(separators == nullptr))
    {
        func(std::string_view(str));
        return;
    }

    using TIndex = size_t;

    TIndex start = 0;
    TIndex end = 0;

    auto IsSeparator = [separators](char ch) -> bool
    {
        TIndex i = 0;
        char separator = separators[i];

        while (separator != '\0')
        {
            if (ch == separator)
                return true;

            separator = separators[++i];
        }

        return false;
    };

    char ch = str[end];
    while (ch != '\0')
    {
        if (IsSeparator(ch))
        {
            if (start < end)
            {
                const char* interStr = &str[start];
                auto length = end - start;
                func(std::string_view(interStr, length));
            }

            start = end + 1;
        }

        ++end;
        ch = str[end];
    }

    if (start < end)
    {
        const char* interStr = &str[start];
        auto length = end - start;
        func(std::string_view(interStr, length));
    }
}

HE::StaticString PrettyFunctionToFunctionName(const char* PrettyFunction)
{
    using TStr = std::string_view;
    TStr str(PrettyFunction);
    
    auto start = str.find_last_of("::") + 1;
    if (start == TStr::npos)
    {
        return HE::StaticString(PrettyFunction);
    }
    
    str = str.substr(start);
    
    return HE::StaticString(str);
}

HE::StaticString PrettyFunctionToClassName(const char* PrettyFunction)
{
    using TStr = std::string_view;
    TStr str(PrettyFunction);
   
    auto end = str.find_last_of("::") - 1;
    if (end == TStr::npos)
        return HE::StaticString();
    
    str = str.substr(0, end);

    auto start = str.find_last_of(" ") + 1;
    str = str.substr(start);
    
    return HE::StaticString(str);
}

HE::StaticString PrettyFunctionToMethodName(const char* PrettyFunction)
{
    using TStr = std::string_view;
    TStr str(PrettyFunction);
    
    auto start = str.find_last_of("::");
    if (start == TStr::npos)
        return HE::StaticString(PrettyFunction);
    
    start = str.find_last_of(" ", start - 2) + 1;
    str = str.substr(start);
    
    return HE::StaticString(str);
}

HE::StaticString PrettyFunctionToCompactClassName(const char* PrettyFunction)
{
    using namespace HE;
    
    using TStr = std::string_view;
    TStr str(PrettyFunction);
   
    auto end = str.find_last_of("::") - 1;
    if (end == TStr::npos)
        return HE::StaticString();
    
    str = str.substr(0, end);
    auto start = str.find_last_of("::") + 1;
    str = str.substr(start, (end - start));
    
    return HE::StaticString(str);
}

HE::StaticString PrettyFunctionToCompactMethodName(const char* PrettyFunction)
{
    using TStr = std::string_view;
    TStr str(PrettyFunction);
    
    auto start = str.find_last_of("::");
    if (start == TStr::npos)
        return HE::StaticString();

    start = str.find_last_of("::", start - 2) + 1;
    str = str.substr(start);
    
    return HE::StaticString(str);
}

size_t StrLen(const char* text)
{
    return strlen(text);
}

size_t StrLen(const char* text, size_t bufferSize)
{
    return strnlen(text, bufferSize);
}

size_t CalculateHash(const char* text)
{
    size_t hashCode = 5381;
    
    while (*text != '\0')
    {
        size_t ch = *text;
        ++text;
        hashCode = ((hashCode << 5) + hashCode) + ch; /* hash * 33 + c */
    }
    
    return hashCode;
}

size_t CalculateHash(const std::string_view& str)
{
    size_t hashCode = 5381;

    for (size_t ch : str)
    {
        hashCode = ((hashCode << 5) + hashCode) + ch; /* hash * 33 + c */
    }

    return hashCode;
}

} // StringUtil

#ifdef __UNIT_TEST__

namespace HE
{

void StringUtilTest::Prepare()
{
    using namespace StringUtil;

    AddTest("Tokenizer(default)", [this](auto& ls)
    {
        TVector<TString> tokens;
        auto func = [&tokens](auto token)
        {
            tokens.emplace_back(token);
        };

        ForEachToken("abc def    123\n 456  \t\n\r 789    000 end.", func);

        const auto numTokens = tokens.size();
        if (numTokens != 7)
        {
            ls << "Incorrect number of tokens " << tokens.size()
                << ", 7 is expected." << lferr;
        }

        const char* solutions[7] = {"abc", "def", "123", "456", "789", "000", "end."};

        for (int i = 0; i < numTokens; ++i)
        {
            TString solution(solutions[i]);
            if (tokens[i] != solution)
            {
                ls << "Invalid token " << tokens[i].c_str()
                    << ", " << solution.c_str() << " is expected." << lferr;
            }
        }

        for (auto& token : tokens)
        {
            ls << "Token: " << token.c_str() << lf;
        }
    });

    AddTest("Tokenizer", [this](auto& ls)
    {
        auto str = "abc::def;;;123.......456::;;..;;::789.000.end.";

        TVector<TString> tokens;
        auto func = [&tokens](auto token)
        {
            tokens.emplace_back(token);
        };

        ForEachToken(str, func, ".:;");

        const auto numTokens = tokens.size();
        if (numTokens != 7)
        {
            ls << "Incorrect number of tokens " << tokens.size()
                << ", 7 is expected." << lferr;
        }

        const char* solutions[7] = {"abc", "def", "123", "456", "789", "000", "end"};

        for (int i = 0; i < numTokens; ++i)
        {
            TString solution(solutions[i]);
            if (tokens[i] != solution)
            {
                ls << "Invalid token " << tokens[i].c_str()
                    << ", " << solution.c_str() << " is expected." << lferr;
            }
        }

        for (auto& token : tokens)
        {
            ls << "Token: " << token.c_str() << lf;
        }
    });

    auto prettyFunction = __PRETTY_FUNCTION__;

    AddTest("PrettyFunctionToClassName", [this, prettyFunction](auto& ls)
    {
        StaticString className("HE::StringUtilTest");

        auto name = PrettyFunctionToClassName(prettyFunction);
        ls << "Class Name is " << name << " / " << className << lf;

        if (name != className)
        {
            ls << "PrettyFunctionToClassName " << name
                << " doesn't coincide with " << className << lferr;
        }
    });

    AddTest("PrettyFunctionToCompactClassName", [this, prettyFunction](auto& ls)
    {
        StaticString className("StringUtilTest");

        auto name = PrettyFunctionToCompactClassName(prettyFunction);
        ls << "Compact Class Name is " << name << " / " << className << lf;

        if (name != className)
        {
            ls << "PrettyFunctionToCompactClassName " << name
                << " doesn't coincide with " << className << lferr;
        }
    });


    AddTest("PrettyFunctionToFunctionName", [this, prettyFunction](auto& ls)
    {
        StaticString funcName("Prepare()");
        StaticString funcName2("Prepare(void)");

        auto name = PrettyFunctionToFunctionName(prettyFunction);
        ls <<  "Function Name is " << name  << " / (" << funcName
            << " or " << funcName2 << ')' << lf;
        
        if (name != funcName && name != funcName2)
        {
            ls << "PrettyFunctionToFunctionName " << name
                << " doesn't coincide with niether " << funcName
                << " nor " << funcName2 << lferr;
        }
    });

    AddTest("PrettyFunctionToMethodName", [this, prettyFunction](auto& ls)
    {

        StaticString funcName("HE::StringUtilTest::Prepare()");
        StaticString funcName2("HE::StringUtilTest::Prepare(void)");
        auto name = PrettyFunctionToMethodName(prettyFunction);

        ls << "Function Name is " << name  << " / (" << funcName
            << " or " << funcName2 << ')' << lf;
        
        if (name != funcName && name != funcName2)
        {
            ls << "PrettyFunctionToMethodName " << name
                << " doesn't coincide with niether " << funcName
                << " nor " << funcName2 << lferr;
        }

    });

    AddTest("PrettyFunctionToCompactMethodName", [this, prettyFunction](auto& ls)
    {
        StaticString funcName("StringUtilTest::Prepare()");
        StaticString funcName2("StringUtilTest::Prepare(void)");
        auto name = PrettyFunctionToCompactMethodName(prettyFunction);

        ls << "Function Name is " << name  << " / (" << funcName
            << " or " << funcName2 << ')' << lf;
        
        if (name != funcName && name != funcName2)
        {
            ls << "PrettyFunctionToCompactMethodName " << name
                << " doesn't coincide with niether " << funcName
                << " nor " << funcName2 << lferr;
        }
    });
}
} // HE
#endif //__UNIT_TEST__
