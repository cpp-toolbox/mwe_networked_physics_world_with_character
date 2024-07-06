#include "physics.hpp"

// The Jolt headers don't include Jolt.h. Always include Jolt.h before including
// any other Jolt header. You can use Jolt.h in your precompiled header to speed
// up compilation.
#include "../../math/conversions.hpp"
#include "Jolt/Physics/Character/CharacterVirtual.h"
#include "Jolt/Physics/Collision/Shape/CapsuleShape.h"
#include "Jolt/Physics/Collision/Shape/ConvexHullShape.h"
#include "Jolt/Physics/Collision/Shape/MeshShape.h"
#include <stdexcept>

//// Disable common warnings triggered by Jolt, you can use
/// JPH_SUPPRESS_WARNING_PUSH / JPH_SUPPRESS_WARNING_POP to
/// store and restore the warning state
// JPH_SUPPRESS_WARNINGS

Physics::Physics() {
    this->initialize_engine();
    this->initialize_world_objects();
}

Physics::~Physics() { this->clean_up_world(); }

void Physics::initialize_engine() {
    JPH::RegisterDefaultAllocator();

    JPH::Trace = TraceImpl;
    JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = AssertFailedImpl;)

    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();

    // dynamic allocation, we don't worry about the rule of three since we never
    // copy the physics system, there is only ever one instance
    temp_allocator = new JPH::TempAllocatorImpl(10 * 1024 * 1024);
    job_system = new JPH::JobSystemThreadPool(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers,
                                              JPH::thread::hardware_concurrency() - 1);

    physics_system.Init(cMaxBodies, cNumBodyMutexes, cMaxBodyPairs, cMaxContactConstraints, broad_phase_layer_interface,
                        object_vs_broadphase_layer_filter, object_vs_object_layer_filter);

    body_activation_listener = new MyBodyActivationListener();
    contact_listener = new MyContactListener();

    physics_system.SetBodyActivationListener(body_activation_listener);
    physics_system.SetContactListener(contact_listener);
}

void Physics::initialize_world_objects() {

    physics_system.SetGravity(JPH::Vec3(0, -40.0, 0));

    JPH::BodyInterface &body_interface = physics_system.GetBodyInterface();

    JPH::Array<JPH::Vec3> vertices = fibonacci_sphere(60);
    JPH::ConvexHullShapeSettings low_poly_ball_settings(vertices, JPH::cDefaultConvexRadius);
    JPH::ShapeSettings::ShapeResult ball_shape_result = low_poly_ball_settings.Create();

    if (!ball_shape_result.IsValid()) {
        throw std::runtime_error("ball shape is invalid");
    }

    JPH::ShapeRefC ball_shape = ball_shape_result.Get(); // We don't expect an error here, but you can check
                                                         // floor_shape_result for HasError() / GetError()

    JPH::BodyCreationSettings ball_creation_settings(ball_shape, JPH::RVec3(5.0, 20.0, 5.0), JPH::Quat::sIdentity(),
                                                     JPH::EMotionType::Dynamic, Layers::MOVING);
    JPH::Body *ball = body_interface.CreateBody(ball_creation_settings); // Note that if we run out of bodies this can
                                                                         // return nullptr
    body_interface.AddBody(ball->GetID(), JPH::EActivation::Activate);
    created_body_ids.push_back(ball->GetID());
    body_interface.SetLinearVelocity(ball->GetID(), JPH::Vec3(0.0f, -5.0f, 0.0f));
}

/**
 * \brief For every mesh in this model, we create a physics object that
 * represents the mesh
 */
void Physics::load_model_into_physics_world(Model *model) {

    JPH::BodyInterface &body_interface = physics_system.GetBodyInterface();

    for (int i = 0; i < model->meshes.size(); i++) {

        Mesh mesh = model->meshes[i];

        JPH::TriangleList triangles;

        assert(mesh.indices.size() % 3 == 0); // only contains triangles
        for (int j = 0; j < mesh.indices.size(); j += 3) {
            unsigned int j1 = mesh.indices[j];
            unsigned int j2 = mesh.indices[j + 1];
            unsigned int j3 = mesh.indices[j + 2];

            glm::vec3 temp_v1 = mesh.vertices[j1].position;
            JPH::Float3 v1 = JPH::Float3(temp_v1.x, temp_v1.y, temp_v1.z);

            glm::vec3 temp_v2 = mesh.vertices[j2].position;
            JPH::Float3 v2 = JPH::Float3(temp_v2.x, temp_v2.y, temp_v2.z);

            glm::vec3 temp_v3 = mesh.vertices[j3].position;
            JPH::Float3 v3 = JPH::Float3(temp_v3.x, temp_v3.y, temp_v3.z);

            JPH::Triangle tri = JPH::Triangle(v1, v2, v3);

            triangles.push_back(tri);
        }

        JPH::MeshShapeSettings settings = JPH::MeshShapeSettings(triangles);

        JPH::Ref<JPH::Shape> mesh_shape;

        // Create shape
        JPH::Shape::ShapeResult result = settings.Create();
        if (result.IsValid()) {
            mesh_shape = result.Get();
        } else {
            throw std::runtime_error("couldn't get resulting shape");
        }

        JPH::BodyCreationSettings mesh_settings(mesh_shape, JPH::RVec3(0.0, 0.0, 0.0), JPH::Quat::sIdentity(),
                                                JPH::EMotionType::Static, Layers::NON_MOVING);
        JPH::Body *mesh_body = body_interface.CreateBody(mesh_settings); // Note that if we run out of bodies this can
                                                                         // return nullptr
        body_interface.AddBody(mesh_body->GetID(), JPH::EActivation::DontActivate);
        created_body_ids.push_back(mesh_body->GetID());
    }
}

/**
 * \brief create character controller for a user
 * \todo do I have to account for dynamic memory? Come back when you know what
 * ref is
 */
void Physics::create_character(uint64_t client_id) {
    JPH::Ref<JPH::CharacterVirtualSettings> settings = new JPH::CharacterVirtualSettings();
    settings->mShape = new JPH::CapsuleShape(0.5f * this->character_height, this->character_radius);
    settings->mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(),
                                             -this->character_radius); // Accept contacts that touch the
                                                                       // lower sphere of the capsule

    JPH::Ref<JPH::CharacterVirtual> character =
        new JPH::CharacterVirtual(settings, JPH::RVec3(0.0f, 10.0f, 0.0f), JPH::Quat::sIdentity(), &physics_system);

    client_id_to_physics_character[client_id] = character;
}

void Physics::delete_character(uint64_t client_id) { client_id_to_physics_character.erase(client_id); }

/**
 * \brief updates the objects part of this physics simulation
 */
void Physics::update(float delta_time) {

    // JPH::Vec3 ch_up = character->GetUp();
    // std::cout << "Character UP = (" << ch_up.GetX() << ", " << ch_up.GetY() <<
    // ", " << ch_up.GetZ() << ")" << std::endl;

    // Settings for our update function
    JPH::CharacterVirtual::ExtendedUpdateSettings update_settings;
    // update_settings.mStickToFloorStepDown = character->GetUp() *
    // update_settings.mStickToFloorStepDown.Length();
    // update_settings.mWalkStairsStepUp = character->GetUp() *
    // update_settings.mWalkStairsStepUp.Length();
    //
    for (const auto &pair : client_id_to_physics_character) {
        JPH::Ref<JPH::CharacterVirtual> character = pair.second;
        character->ExtendedUpdate(delta_time, -character->GetUp() * physics_system.GetGravity().Length(),
                                  update_settings, physics_system.GetDefaultBroadPhaseLayerFilter(Layers::MOVING),
                                  physics_system.GetDefaultLayerFilter(Layers::MOVING), {}, {}, *temp_allocator);
    }

    physics_system.Update(delta_time, cCollisionSteps, temp_allocator, job_system);
}

void Physics::update_specific_character(float delta_time, uint64_t client_id_of_character) {

    // Safely access and call the function if the key exists
    auto potential_character_pair = client_id_to_physics_character.find(client_id_of_character);
    if (potential_character_pair != client_id_to_physics_character.end()) {
        JPH::Ref<JPH::CharacterVirtual> requested_character =
            potential_character_pair->second; // Call the function with an argument
        JPH::CharacterVirtual::ExtendedUpdateSettings update_settings;
        requested_character->ExtendedUpdate(
            delta_time, -requested_character->GetUp() * physics_system.GetGravity().Length(), update_settings,
            physics_system.GetDefaultBroadPhaseLayerFilter(Layers::MOVING),
            physics_system.GetDefaultLayerFilter(Layers::MOVING), {}, {}, *temp_allocator);
    } else {
        std::cout << "tried to update specific character in physics, but couldn't find them in the map" << std::endl;
    }
}

void Physics::clean_up_world() {
    JPH::BodyInterface &body_interface = physics_system.GetBodyInterface();

    for (auto body_id : created_body_ids) {
        body_interface.RemoveBody(body_id);
        body_interface.DestroyBody(body_id);
    }

    JPH::UnregisterTypes();

    // de-allocate dynamic memory
    delete temp_allocator;
    delete job_system;
    delete body_activation_listener;
    delete contact_listener;

    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;

    std::cout << "successfully cleaned up world" << std::endl;
}
