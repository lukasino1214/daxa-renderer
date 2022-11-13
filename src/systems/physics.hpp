#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <iostream>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace dare {
    inline auto from_jolt(const JPH::Vec3 v) -> glm::vec3 {
        return glm::vec3{v.GetX(), v.GetY(), v.GetZ()};
    }

    inline auto from_jolt(const JPH::Vec4 v) -> glm::vec4 {
        return glm::vec4{v.GetX(), v.GetY(), v.GetZ(), v.GetW()};
    }

    inline auto from_jolt(JPH::Float3 v) -> glm::vec3 {
        return glm::vec3{v.x, v.y, v.z};
    }

    inline auto from_jolt(JPH::ColorArg c) -> glm::vec4 {
        return glm::vec4{c.r, c.g, c.b, c.a};
    }

    inline auto from_jolt(const JPH::Quat q) -> glm::quat {
    #if defined(GLM_FORCE_QUAT_DATA_XYZW)
        return glm::quat{q.GetX(), q.GetY(), q.GetZ(), q.GetW()};
    #else
        return glm::quat{q.GetW(), q.GetX(), q.GetY(), q.GetZ()};
    #endif
    }

    inline auto to_jolt(const glm::vec3 v) -> JPH::Vec3 {
        return JPH::Vec3{v.x, v.y, v.z};
    }

    inline auto to_jolt(const glm::vec4 v) -> JPH::Vec4 {
        return JPH::Vec4{v.x, v.y, v.z, v.w};
    }

    inline auto to_jolt(const glm::quat q) -> JPH::Quat {
        const auto nq = glm::normalize(q);
        const auto jq = JPH::Quat{nq.x, nq.y, nq.z, nq.w};
        return jq;
    }

    inline auto from_jolt(const JPH::Mat44 m) -> glm::mat4 {
        return glm::mat4{
            m(0, 0), m(1, 0), m(2, 0), m(3, 0),
            m(0, 1), m(1, 1), m(2, 1), m(3, 1),
            m(0, 2), m(1, 2), m(2, 2), m(3, 2),
            m(0, 3), m(1, 3), m(2, 3), m(3, 3)
        };
    }

    inline auto to_jolt(const glm::mat4 m) -> JPH::Mat44 {
        return JPH::Mat44{to_jolt(m[0]), to_jolt(m[1]), to_jolt(m[2]), to_jolt(m[3])};
    }

    JPH_SUPPRESS_WARNINGS

    using namespace JPH;
    using namespace std;

    static void TraceImpl(const char *inFMT, ...) { 
        va_list list;
        va_start(list, inFMT);
        char buffer[1024];
        vsnprintf(buffer, sizeof(buffer), inFMT, list);
        va_end(list);

        cout << buffer << endl;
    }

    #ifdef JPH_ENABLE_ASSERTS

    static bool AssertFailedImpl(const char *inExpression, const char *inMessage, const char *inFile, uint inLine) { 
        cout << inFile << ":" << inLine << ": (" << inExpression << ") " << (inMessage != nullptr? inMessage : "") << endl;
        return true;
    };

    #endif

    namespace Layers {
        static constexpr uint8 NON_MOVING = 0;
        static constexpr uint8 MOVING = 1;
        static constexpr uint8 NUM_LAYERS = 2;
    };

    static bool MyObjectCanCollide(ObjectLayer inObject1, ObjectLayer inObject2) {
        switch (inObject1) {
            case Layers::NON_MOVING:
                return inObject2 == Layers::MOVING;
            case Layers::MOVING:
                return true;
            default:
                JPH_ASSERT(false);
                return false;
        }
    };

    namespace BroadPhaseLayers {
        static constexpr BroadPhaseLayer NON_MOVING(0);
        static constexpr BroadPhaseLayer MOVING(1);
        static constexpr uint NUM_LAYERS(2);
    };

    class BPLayerInterfaceImpl final : public BroadPhaseLayerInterface {
    public:
        BPLayerInterfaceImpl() {
            mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
            mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
        }

        virtual uint GetNumBroadPhaseLayers() const override {
            return BroadPhaseLayers::NUM_LAYERS;
        }

        virtual BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer inLayer) const override {
            JPH_ASSERT(inLayer < Layers::NUM_LAYERS);
            return mObjectToBroadPhase[inLayer];
        }

    #if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
        virtual const char * GetBroadPhaseLayerName(BroadPhaseLayer inLayer) const override {
            switch ((BroadPhaseLayer::Type)inLayer) {
                case (BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING:	return "NON_MOVING";
                case (BroadPhaseLayer::Type)BroadPhaseLayers::MOVING:		return "MOVING";
                default:													JPH_ASSERT(false); return "INVALID";
            }
        }
    #endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED

    private:
        BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS];
    };

    static bool MyBroadPhaseCanCollide(ObjectLayer inLayer1, BroadPhaseLayer inLayer2) {
        switch (inLayer1) {
            case Layers::NON_MOVING:
                return inLayer2 == BroadPhaseLayers::MOVING;
            case Layers::MOVING:
                return true;	
            default:
                JPH_ASSERT(false);
                return false;
        }
    }

    class MyContactListener : public ContactListener {
    public:
        virtual ValidateResult OnContactValidate(const Body &inBody1, const Body &inBody2, const CollideShapeResult &inCollisionResult) override {
            cout << "Contact validate callback" << endl;
            return ValidateResult::AcceptAllContactsForThisBodyPair;
        }

        virtual void OnContactAdded(const Body &inBody1, const Body &inBody2, const ContactManifold &inManifold, ContactSettings &ioSettings) override {
            cout << "A contact was added" << endl;
        }

        virtual void OnContactPersisted(const Body &inBody1, const Body &inBody2, const ContactManifold &inManifold, ContactSettings &ioSettings) override {
            cout << "A contact was persisted" << endl;
        }

        virtual void OnContactRemoved(const SubShapeIDPair &inSubShapePair) override { 
            cout << "A contact was removed" << endl;
        }
    };

    class MyBodyActivationListener : public BodyActivationListener {
    public:
        virtual void OnBodyActivated(const BodyID &inBodyID, uint64 inBodyUserData) override {
            cout << "A body got activated" << endl;
        }

        virtual void OnBodyDeactivated(const BodyID &inBodyID, uint64 inBodyUserData) override {
            cout << "A body went to sleep" << endl;
        }
    };

    struct Physics {
        JPH::TempAllocatorImpl* temp_allocator;
        JPH::JobSystemThreadPool* job_system;
        BPLayerInterfaceImpl broad_phase_layer_interface;
        JPH::PhysicsSystem physics_system;
        MyBodyActivationListener body_activation_listener;
        MyContactListener contact_listener;

        const JPH::uint cMaxBodies = 1024;
        const JPH::uint cNumBodyMutexes = 0;
        const JPH::uint cMaxBodyPairs = 1024;
        const JPH::uint cMaxContactConstraints = 1024;
        const float cDeltaTime = 1.0f / 60.0f;
        const int cCollisionSteps = 1;
        const int cIntegrationSubSteps = 1;
        uint32_t step = 0;

        BodyID sphere_id;
        Body *floor;

        Physics();
        ~Physics();

        auto get_body_interface() -> JPH::BodyInterface&;
        void optimize();
        void update();
        void setup();
        void cleanup();
    };
}