// Created by mooming.go@gmail.com, 2022

#include "TaskSystem.h"

#include "HSTL/HString.h"
#include "Log/Logger.h"
#include "OSAL/OSThread.h"
#include "String/StringBuilder.h"
#include "String/StringUtil.h"
#include "System/Time.h"
#include <algorithm>
#include <future>
#include <thread>


namespace HE
{

TaskSystem::TIndex TaskSystem::GetNumHardwareThreads()
{
    const auto hardwareConcurrency = std::thread::hardware_concurrency();
    TIndex numAvaibleHardwareThreads = hardwareConcurrency;

    return numAvaibleHardwareThreads;
}

TaskSystem::TaskSystem()
    : isRunning(false)
    , name("TaskSystem")
    , numHardwareThreads(GetNumHardwareThreads())
    , workerIndexStart(0)
    , numWorkers(0)
    , keySeed(1)
    , mainTaskThreadID(std::this_thread::get_id())
    , slots(numHardwareThreads)
{
    FatalAssert(numHardwareThreads > 0, "It should have at least one hardware thread.");
}

TaskSystem::~TaskSystem()
{
    Shutdown();
}

void TaskSystem::Initialize()
{
    auto& logger = Logger::Get();
    auto logFilter = [](auto level) { return level > ELogLevel::Warning; };
    logger.SetFilter(GetName(), logFilter);

    auto log = Logger::Get(GetName());
    log.Out([this](auto& ls)
    {
        ls << "Hardware Concurrency = " << numHardwareThreads;
    });

    BuildStreams();
}

void TaskSystem::Shutdown()
{
    Assert(IsMainThread());

    if (!isRunning)
        return;

    isRunning = false;

    for (auto& stream : streams)
    {
        auto& thread = stream.GetThread();
        if (unlikely(!thread.joinable()))
            continue;

        thread.join();
    }
}

void TaskSystem::PostUpdate()
{
    Flush();
}

void TaskSystem::Flush()
{
    Assert(IsMainThread());

    for (auto& slot : slots)
    {
        if (slot.key == InvalidKey)
            continue;

        auto& task = slot.task;
        if (!task.IsDone())
            continue;

        slot.key = InvalidKey;
        task.Reset();
    }
}

bool TaskSystem::IsMainThread() const
{
    return std::this_thread::get_id() == mainTaskThreadID;
}

bool TaskSystem::IsIOTaskThread() const
{
    return std::this_thread::get_id() == ioTaskThreadID;
}

StaticString TaskSystem::GetCurrentStreamName() const
{
    auto currentThreadId = std::this_thread::get_id();
    for (auto& stream : streams)
    {
        if (stream.GetThreadID() != currentThreadId)
            continue;

        return stream.GetName();
    }

    static StaticString unknown("Unknown");
    return unknown;
}

StaticString TaskSystem::GetStreamName(int index) const
{
    if (unlikely(!streams.IsValidIndex(index)))
    {
        static StaticString unknown("Unknown");
        return unknown;
    }
    
    return streams[index].GetName();
}

TaskSystem::TIndex TaskSystem::GetCurrentStreamIndex() const
{
    return GetStreamIndex(std::this_thread::get_id());
}

TaskSystem::TIndex TaskSystem::GetStreamIndex(ThreadID id) const
{
    TIndex index = -1;

    auto size = streams.Size();
    for (TIndex i = 0; i < size; ++i)
    {
        auto& stream = streams[i];
        if (stream.GetThreadID() != id)
            continue;

        index = i;
        break;
    }

    return index;
}

Task* TaskSystem::GetTask(TKey key, TIndex taskIndex)
{
    if (unlikely(!slots.IsValidIndex(taskIndex)))
        return nullptr;

    auto& slot = slots[taskIndex];
    if (slot.key == key)
        return &(slot.task);

    return nullptr;
}

const Task* TaskSystem::GetTask(TKey key, TIndex taskIndex) const
{
    if (unlikely(!slots.IsValidIndex(taskIndex)))
        return nullptr;

    auto& slot = slots[taskIndex];
    if (slot.key == key)
        return &(slot.task);

    return nullptr;
}

TaskHandle TaskSystem::RegisterTask(TIndex streamIndex
    , StaticString taskName, Runnable func)
{
    TaskHandle handle;

    if (unlikely(func == nullptr))
    {
        auto log = Logger::Get(GetName());
        log.OutWarning([taskName](auto& ls)
        {
            ls << "Null runnable function input for " << taskName.c_str();
        });

        return handle;
    }


    if (unlikely(!streams.IsValidIndex(streamIndex)))
    {
        auto log = Logger::Get(GetName());
        log.OutWarning([streamIndex, taskName](auto& ls)
        {
            ls << "Incorrect stream index = " << streamIndex
                << " for " << taskName.c_str();
        });

        return handle;
    }

    {
        std::lock_guard lock(requestLock);

        auto length = slots.Size();
        for (TIndex index = 0; index < length; ++index)
        {
            auto& slot = slots[index];
            if (slot.key != InvalidKey)
                continue;

            auto key = IssueTaskKey();
            slot.key = key;

            auto& task = slot.task;
            task.Reset(name, 0, func);

            auto releaseFunc =  [this, streamIndex](const TaskHandle& handle)
            {
                auto key = handle.GetKey();
                auto index = handle.GetIndex();
                DeregisterTask(streamIndex, key);
                ReleaseTask(key, index);
            };

            handle = TaskHandle(key, index, releaseFunc);

            auto& stream = streams[streamIndex];
            stream.AddResident(key, task);

            break;
        }
    }

    return handle;
}

void TaskSystem::DeregisterTask(TIndex streamIndex, TKey key)
{
    std::lock_guard lock(requestLock);
    if (unlikely(!streams.IsValidIndex(streamIndex)))
    {
        auto log = Logger::Get(GetName());
        log.OutError([streamIndex, key](auto& ls)
        {
            ls << "Invalid stream index = " << streamIndex
                << " for key " << key;
        });

        return;
    }

    auto& stream = streams[streamIndex];
    stream.RemoveResidentTaskSync(key);
}

void TaskSystem::DeregisterTaskAsync(TIndex streamIndex, TKey key)
{
    std::lock_guard lock(requestLock);
    if (unlikely(!streams.IsValidIndex(streamIndex)))
    {
        auto log = Logger::Get(GetName());
        log.OutError([streamIndex, key](auto& ls)
        {
            ls << "Invalid stream index = " << streamIndex
                << " for key " << key;
        });

        return;
    }

    auto& stream = streams[streamIndex];
    stream.RemoveResidentTask(key);
}

TaskHandle TaskSystem::DispatchTask(StaticString taskName
    , Runnable func, TIndex streamIndex)
{
    TaskHandle handle;

    auto log = Logger::Get(GetName());
    if (unlikely(func == nullptr))
    {
        log.OutWarning([taskName](auto& ls)
        {
            ls << "Null runnable function input for " << taskName.c_str();
        });

        return handle;
    }

    std::lock_guard lock(requestLock);

    if (unlikely(streams.IsValidIndex(streamIndex)))
    {
        log.OutWarning([streamIndex, taskName](auto& ls)
        {
            ls << "Invalid stream index " << streamIndex
                << " for " << taskName.c_str();
        });

        return handle;
    }

    auto length = slots.Size();
    for (TIndex index = 0; index < length; ++index)
    {
        auto& slot = slots[index];
        if (slot.key != InvalidKey)
            continue;

        auto key = IssueTaskKey();
        slot.key = key;

        auto& task = slot.task;
        task.Reset(taskName, 0, func);
        handle = TaskHandle(key, index, [this](const TaskHandle& handle)
        {
            auto key = handle.GetKey();
            auto index = handle.GetIndex();
            ReleaseTask(key, index);
        });

        auto& stream = streams[streamIndex];
        stream.Request(key, task, 0, 0);

        log.Out([taskName, &stream](auto& ls)
        {
            ls << taskName.c_str() << " is assigned to "
                << stream.GetName().c_str();
        });

        break;
    }

    return handle;
}

TaskHandle TaskSystem::DispatchTask(StaticString taskName
    , TaskIndex size, Runnable func)
{
    TaskHandle handle;

    auto log = Logger::Get(GetName());
    if (unlikely(func == nullptr))
    {
        log.OutWarning([taskName](auto& ls)
        {
            ls << "Null runnable function input for " << taskName.c_str();
        });

        return handle;
    }

    {
        std::lock_guard lock(requestLock);

        auto length = slots.Size();
        for (TIndex index = 0; index < length; ++index)
        {
            auto& slot = slots[index];
            if (slot.key != InvalidKey)
                continue;

            auto key = IssueTaskKey();
            slot.key = key;

            auto& task = slot.task;
            task.Reset(taskName, size, func);
            handle = TaskHandle(key, index, [this](const TaskHandle& handle)
            {
                auto key = handle.GetKey();
                auto index = handle.GetIndex();
                ReleaseTask(key, index);
            });

            TIndex start = 0;
            TIndex i = workerIndexStart;

            if (size > 0)
            {
                auto numWorkers = numHardwareThreads - workerIndexStart;
                numWorkers = std::min<TIndex>(numWorkers, size);

                Assert(numWorkers > 0);

                const TIndex interval = size / numWorkers;
                TIndex end = interval;

                const auto length = i + numWorkers - 1;
                for (; i < length; ++i)
                {
                    auto& stream = streams[i];
                    stream.Request(key, task, start, end);

                    log.Out([taskName, &stream, start, end](auto& ls)
                    {
                        ls << taskName.c_str() << " is assigned to "
                            << stream.GetName().c_str() << " on ["
                            << start << ", " << end << ')';
                    });

                    start = end;
                    end += interval;
                }
            }

            auto& stream = streams[i];
            stream.Request(key, task, start, size);

            log.Out([taskName, &stream, start, size](auto& ls)
            {
                ls << taskName.c_str() << " is assigned to "
                    << stream.GetName().c_str() << " on [" << start << ", "
                    << size << ')';
            });

            break;
        }
    }

    return handle;
}

TaskHandle TaskSystem::DispatchTask(StaticString taskName
    , TaskIndex size, Runnable func, TIndex numStreams)
{
    TaskHandle handle;

    auto log = Logger::Get(GetName());
    if (unlikely(func == nullptr))
    {
        log.OutWarning([taskName](auto& ls)
        {
            ls << "Null runnable function input for " << taskName.c_str();
        });

        return handle;
    }

    numStreams = std::clamp<TIndex>(numStreams, 1, streams.Size());

    {
        std::lock_guard lock(requestLock);

        auto length = slots.Size();
        for (TIndex index = 0; index < length; ++index)
        {
            auto& slot = slots[index];
            if (slot.key != InvalidKey)
                continue;

            auto key = IssueTaskKey();
            slot.key = key;

            auto& task = slot.task;
            task.Reset(taskName, size, func);
            handle = TaskHandle(key, index, [this](const TaskHandle& handle)
            {
                auto key = handle.GetKey();
                auto index = handle.GetIndex();
                ReleaseTask(key, index);
            });

            TIndex start = 0;
            TIndex i = workerIndexStart;

            if (size > 0)
            {
                const auto numWorkers = numHardwareThreads - workerIndexStart;
                Assert(numWorkers > 0);

                auto num = std::min<TIndex>(numStreams, size);
                num = std::min<TIndex>(num, numWorkers);

                const TIndex interval = size / num;
                TIndex end = interval;

                const auto length = i + num - 1;
                for (; i < length; ++i)
                {
                    auto& stream = streams[i];
                    stream.Request(key, task, start, end);

                    log.Out([taskName, &stream, start, end](auto& ls)
                    {
                        ls << taskName.c_str() << " is assigned to "
                            << stream.GetName().c_str() << " on ["
                            << start << ", " << end << ')';
                    });

                    start = end;
                    end += interval;
                }
            }

            auto& stream = streams[i];
            stream.Request(key, task, start, size);

            log.Out([taskName, &stream, start, size](auto& ls)
            {
                ls << taskName.c_str() << " is assigned to "
                    << stream.GetName().c_str() << " on [" << start << ", "
                    << size << ')';
            });

            break;
        }
    }

    return handle;
}

void TaskSystem::ReleaseTask(TKey key, TIndex index)
{
    if (unlikely(key == InvalidKey))
    {
        auto log = Logger::Get(GetName());
        log.OutWarning([func = __PRETTY_FUNCTION__](auto& ls)
        {
            InlinePoolAllocator<char, 128> alloc;
            AllocatorScope scope(alloc);
            
            using namespace StringUtil;
            ls << '[' << PrettyFunctionToMethodName(func)
                << "] Failed due to the invalid key input.";
        });

        return;
    }

    if (unlikely(!slots.IsValidIndex(index)))
    {
        auto log = Logger::Get(GetName());
        log.OutWarning([func = __PRETTY_FUNCTION__, index](auto& ls)
        {
            InlinePoolAllocator<char, 128> alloc;
            AllocatorScope scope(alloc);

            using namespace StringUtil;
            ls << '[' << PrettyFunctionToMethodName(func)
                << "] Failed due to the invalid index " << index;
        });

        return;
    }

    {
        std::lock_guard lock(requestLock);

        auto& slot = slots[index];
        if (unlikely(key != slot.key))
        {
            auto log = Logger::Get(GetName());
            log.OutWarning([func = __PRETTY_FUNCTION__, key, &slot](auto& ls)
            {
                InlinePoolAllocator<char, 128> alloc;
                AllocatorScope scope(alloc);

                using namespace StringUtil;
                ls << '[' << PrettyFunctionToMethodName(func)
                    << "] Key Mismatched: " << key << " <=> " << slot.key ;
            });

            return;
        }

        while(!slot.task.IsDone());

        slot.key = InvalidKey;
        slot.task.Reset();
    }
}

void TaskSystem::BuildStreams()
{
    Assert(IsMainThread());
    FatalAssert(numHardwareThreads >= 4, "System minimum requirement: quad cores");

    workerIndexStart = 0;

    auto log = Logger::Get(GetName());
    log.Out("# Creating TaskStreams ======================");

    streams.Swap(Array<TaskStream>(numHardwareThreads));

    {
        auto index = GetMainTaskStreamIndex();
        streams.Emplace(index, "Main");

        index = GetIOTaskStreamIndex();
        streams.Emplace(index, "IO");
    }

    workerIndexStart = GetIOTaskStreamIndex() + 1;

    InlineStringBuilder<64> streamName;
    for (TIndex i = workerIndexStart; i < numHardwareThreads; ++i)
    {
        ++numWorkers;
        streamName << "Worker" << numWorkers;
        
        streams.Emplace(i, streamName.c_str());
        streamName.Clear();
    }

    log.Out("# Starting TaskStreams ======================");

    isRunning = true;

    auto& mainTaskStream = GetMainTaskStream();
    auto& ioTaskStream = GetIOTaskStream();
    mainTaskStream.threadID = mainTaskThreadID;
    ioTaskStream.Start(*this);

    for (TIndex i = workerIndexStart; i < numHardwareThreads; ++i)
    {
        streams[i].Start(*this);
    }
}

TaskSystem::TKey TaskSystem::IssueTaskKey()
{
    Assert(requestLock.try_lock() == false);

    for (auto& slot : slots)
    {
        if (unlikely(keySeed == 0))
            ++keySeed;

        if (likely(slot.key != keySeed))
            break;

        ++keySeed;
    }

    Assert(keySeed != InvalidKey);
    return keySeed++;
}

} // HE

#ifdef __UNIT_TEST__
#include "Engine.h"
#include "OSAL/Intrinsic.h"
#include "Test/TestCollection.h"
#include <memory>


namespace HE
{

void TaskSystemTest::Prepare()
{
    AddTest("Empty Task", [this](auto& ls)
    {
        auto& engine = Engine::Get();
        auto& taskSys = engine.GetTaskSystem();

        auto handle = taskSys.DispatchTask("Test", 0, nullptr);
        if (handle.IsValid())
        {
            ls << "Handle should be invalid if func is null." << lferr;
        }
    });

    AddTest("Task of size 0", [this](auto& ls)
    {
        auto& engine = Engine::Get();
        auto& taskSys = engine.GetTaskSystem();

        auto func = [](auto start, auto end)
        {
            auto log = Logger::Get("Size 0 Task");
            log.Out([&](auto& ls)
            {
                ls << "Range[" << (start + 1)
                    << ", " << end << ')';
            });
        };

        auto handle = taskSys.DispatchTask("Test", 0, func);
        if (!handle.IsValid())
        {
            ls << "Handle should not be invalid." << lferr;
        }

        handle.BusyWait();
    });

    AddTest("Resident Task(sync)", [](auto& ls)
    {
        auto& engine = Engine::Get();
        auto& taskSys = engine.GetTaskSystem();

        int count = 0;
        
        auto func = [&count](auto start, auto end)
        {
            auto log = Logger::Get("Resident Task");
            log.Out([count](auto& ls)
            {
                ls << "Resident Frame Count = " << count;
            });

            ++count;
        };

        StaticString taskName("DummyCount");
        const auto streamIndex = taskSys.GetWorkerIndexStart();

        auto handle = taskSys.RegisterTask(streamIndex, taskName, func);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    });

    AddTest("Resident Task(async)", [](auto& ls)
    {
        auto& engine = Engine::Get();
        auto& taskSys = engine.GetTaskSystem();

        auto count = std::make_shared<uint32_t>(0);

        auto func = [count](auto start, auto end)
        {
            auto log = Logger::Get("Resident Task");
            log.Out([count](auto& ls)
            {
                ls << "Resident Frame Count = " << (*count);
            });

            ++(*count);
        };

        StaticString taskName("DummyCount");
        const auto streamIndex = taskSys.GetWorkerIndexStart();

        auto handle = taskSys.RegisterTask(streamIndex, taskName, func);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    });

    AddTest("Sum", [this](auto& ls)
    {
        std::atomic<uint64_t> sum = 0;

        auto func = [&sum](auto start, auto end)
        {
            auto log = Logger::Get("Sum");
            log.Out([start, end](auto& ls)
            {
                ls << "Start: Sum Range[" << (start + 1)
                    << ", " << end << ']';
            });

            uint64_t result = 0;
            for (uint64_t i = start + 1; i <= end; ++i)
            {
                result += i;
            }

            sum += result;

            log.Out([start, end, result](auto& ls)
            {
                ls << "Sum Range[" << (start + 1)
                    << ", " << end << "] = " << result;
            });
        };

        auto& engine = Engine::Get();
        auto& taskSys = engine.GetTaskSystem();
        auto handle = taskSys.DispatchTask("SummationTask", 10000000, func);
        if (!handle.IsValid())
        {
            ls << "Handle should not be invalid." << lferr;
            return;
        }

        auto task = handle.GetTask();
        if (task == nullptr)
        {
            ls << "DispatchTask failed due to null task." << lferr;
            return;
        }

        handle.BusyWait();

        ls << "Sum = " << sum << ", done = " << task->NumDone()
            << "/" << task->NumStreams() << lf;

        if (sum != 50000005000000L)
        {
            ls << "Incorrect summation result = " << sum << lferr;
        }
    });

    auto countPrimeNumbers = [](auto& ls, auto& lf, auto& lferr, int numWorkers)
    {
        auto IsPrimeNumbrer = [](uint32_t value) -> bool
        {
            for (uint32_t i = 2; i < value; ++i)
            {
                if ((value % i) == 0)
                    return false;
            }

            return true;
        };

        std::atomic<uint64_t> count = 0;
        std::atomic<int> numThreads = 0;

        auto func = [&count, &numThreads, IsPrimeNumbrer](auto start, auto end)
        {
            ++numThreads;

            auto log = Logger::Get("PNCounter");
            log.Out([&](auto& ls)
            {
                ls << "Start: Range[" << (start + 1) << ", " << end << ']';
            });

            uint64_t result = 0;

            for (uint64_t i = start + 1; i <= end; ++i)
            {
                if (IsPrimeNumbrer(i))
                    ++result;
            }

            count += result;

            log.Out([&](auto& ls)
            {
                ls << "Range[" << (start + 1)
                    << ", " << end << "] = " << result;
            });
        };

        constexpr int upperBound = 100000;
        constexpr int solution = 9592;

        auto& engine = Engine::Get();
        auto& taskSys = engine.GetTaskSystem();
        TaskHandle handle;

        Time::TDuration duration;
        {
            Time::Measure timer(duration);

            handle = taskSys.DispatchTask("PNCounter"
                , upperBound, func, numWorkers);

            if (!handle.IsValid())
            {
                ls << "Handle should not be invalid." << lferr;
                return;
            }

            auto taskPtr = handle.GetTask();
            if (taskPtr == nullptr)
            {
                ls << "Task is null." << lferr;
                debugBreak();
            }

            handle.BusyWait();
        }

        count = count - 1;

        if (numThreads != numWorkers)
        {
            ls << "Number of threads must be " << numWorkers
                << ", but " << numThreads << lferr;
        }

        auto taskPtr = handle.GetTask();
        if (taskPtr == nullptr)
        {
            ls << "Task is null." << lferr;
            debugBreak();
        }

        auto& task = *(taskPtr);
        ls << "# of Prime Numbers = " << count << ", done = " << task.NumDone()
            << "/" << task.NumStreams() << ", time = "
            << Time::ToFloat(duration) << " seconds" << lf;

        if (count != solution)
        {
            ls << "Number of prime numbers below " << upperBound
                << " should be " << solution << " but "
                << count << " found!" << lferr;
        }
    };

    auto& engine = Engine::Get();
    auto& taskSys = engine.GetTaskSystem();
    auto numWorkers = taskSys.GetNumWorkers();
    for (int i = 1; i <= numWorkers; ++i)
    {
        InlineStringBuilder<> testName;
        testName << "CountPrimeNumbers with " << i << " workers";

        AddTest(testName.c_str(), [this, countPrimeNumbers, i](auto& ls)
        {
            countPrimeNumbers(ls, lf, lferr, i);
        });
    }
}

} // HE
#endif //__UNIT_TEST__
