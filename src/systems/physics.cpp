#include "physics.hpp"

#include <iostream>
#include <cstdarg>
#include <thread>

namespace dare {
    Physics::Physics() {
        RegisterDefaultAllocator();

        Trace = TraceImpl;
        JPH_IF_ENABLE_ASSERTS(AssertFailed = AssertFailedImpl;)

        Factory::sInstance = new Factory();

        // Register all Jolt physics types
        RegisterTypes();

        temp_allocator = new TempAllocatorImpl(10 * 1024 * 1024);
        job_system = new JobSystemThreadPool(cMaxPhysicsJobs, cMaxPhysicsBarriers, thread::hardware_concurrency() - 1);
        physics_system.Init(cMaxBodies, cNumBodyMutexes, cMaxBodyPairs, cMaxContactConstraints, broad_phase_layer_interface, MyBroadPhaseCanCollide, MyObjectCanCollide);

        physics_system.SetBodyActivationListener(&body_activation_listener);
        physics_system.SetContactListener(&contact_listener);
    }

    Physics::~Physics() {
        delete temp_allocator;
        delete job_system;

        delete Factory::sInstance;
        Factory::sInstance = nullptr;
    }

    auto Physics::get_body_interface() -> JPH::BodyInterface& {
        return physics_system.GetBodyInterface();
    }

    void Physics::optimize() {
        physics_system.OptimizeBroadPhase();
    }

    void Physics::update() {
        ++step;
        physics_system.Update(cDeltaTime, cCollisionSteps, cIntegrationSubSteps, temp_allocator, job_system);
    }

    void Physics::setup() {
        BodyInterface &body_interface = physics_system.GetBodyInterface();

        BoxShapeSettings floor_shape_settings(Vec3(100.0f, 1.0f, 100.0f));

        ShapeSettings::ShapeResult floor_shape_result = floor_shape_settings.Create();
        ShapeRefC floor_shape = floor_shape_result.Get(); // We don't expect an error here, but you can check floor_shape_result for HasError() / GetError()

        BodyCreationSettings floor_settings(floor_shape, Vec3(0.0f, -1.0f, 0.0f), Quat::sIdentity(), EMotionType::Static, Layers::NON_MOVING);
        floor = body_interface.CreateBody(floor_settings); // Note that if we run out of bodies this can return nullptr

        body_interface.AddBody(floor->GetID(), EActivation::DontActivate);
        BodyCreationSettings sphere_settings(new SphereShape(0.5f), Vec3(0.0f, 2.0f, 0.0f), Quat::sIdentity(), EMotionType::Dynamic, Layers::MOVING);
        sphere_id = body_interface.CreateAndAddBody(sphere_settings, EActivation::Activate);
        body_interface.SetLinearVelocity(sphere_id, Vec3(0.0f, -5.0f, 0.0f));
    }

    void Physics::cleanup() {
        BodyInterface &body_interface = physics_system.GetBodyInterface();

        body_interface.RemoveBody(sphere_id);
        body_interface.DestroyBody(sphere_id);

        body_interface.RemoveBody(floor->GetID());
        body_interface.DestroyBody(floor->GetID());
    }
}