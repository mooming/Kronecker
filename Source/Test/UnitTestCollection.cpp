// Created by mooming.go@gmail.com, 2017

#ifdef __UNIT_TEST__

#include "UnitTestCollection.h"

#include "TestEnv.h"
#include "Container/LinkedList.h"
#include "Math/MathUtil.h"
#include "Math/Vector2.h"
#include "Math/Vector3.h"
#include "Math/Vector4.h"
#include "Math/Matrix2x2.h"
#include "Math/Matrix3x3.h"
#include "Math/Matrix4x4.h"
#include "Math/Quaternion.h"
#include "Math/Transform.h"
#include "Math/UniformTransform.h"
#include "Math/RigidTransform.h"
#include "Math/AABB.h"
#include "Memory/BaseAllocator.h"
#include "Memory/InlinePoolAllocator.h"
#include "Memory/Optional.h"
#include "Memory/PoolAllocator.h"
#include "Memory/MultiPoolAllocator.h"
#include "Memory/StackAllocator.h"
#include "Memory/SystemAllocator.h"
#include "String/StaticString.h"
#include "String/StringUtil.h"
#include "System/ComponentSystem.h"
#include "System/TaskSystem.h"
#include <iostream>


using namespace HE;

namespace Test
{
    void RunUnitTests()
    {
        using namespace std;

        cout << fixed;
        cout.precision(3);

        auto& testEnv = TestEnv::GetEnv();

        testEnv.AddTestCollection<SystemAllocatorTest>();
        testEnv.AddTestCollection<BaseAllocatorTest>();
        testEnv.AddTestCollection<InlinePoolAllocatorTest>();
        testEnv.AddTestCollection<StackAllocatorTest>();
        testEnv.AddTestCollection<PoolAllocatorTest>();
        testEnv.AddTestCollection<MultiPoolAllocatorTest>();

        testEnv.AddTestCollection<ArrayTest>();
        testEnv.AddTestCollection<LinkedListTest>();
        testEnv.AddTestCollection<OptionalTest>();

        testEnv.AddTestCollection<StaticStringTest>();
        testEnv.AddTestCollection<StringTest>();
        testEnv.AddTestCollection<StringUtilTest>();

        testEnv.AddTestCollection<MathUtilTest>();
        testEnv.AddTestCollection<Vector2Test>();
        testEnv.AddTestCollection<Vector3Test>();
        testEnv.AddTestCollection<Vector4Test>();

        testEnv.AddTestCollection<Matrix3x3Test>();
        testEnv.AddTestCollection<QuaternionTest>();
        testEnv.AddTestCollection<UniformTransformTest>();
        testEnv.AddTestCollection<RigidTransformTest>();
        testEnv.AddTestCollection<AABBTest>();
        testEnv.AddTestCollection<TransformTest>();

        testEnv.AddTestCollection<ComponentSystemTest>();
        testEnv.AddTestCollection<TaskSystemTest>();
        
        testEnv.Start();
    }
}

#endif // __UNIT_TEST__
