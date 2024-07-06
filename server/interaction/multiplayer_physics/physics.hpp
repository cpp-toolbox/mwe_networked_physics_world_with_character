#ifndef PHYSICS_H
#define PHYSICS_H

#ifndef JPH_DEBUG_RENDERER
#define JPH_DEBUG_RENDERER
#endif

#include "jolt_implementation.hpp"
#include "../../model_loading/model_loading.hpp"
#include "Jolt/Physics/Character/CharacterVirtual.h"
#include "../../thread_safe_queue.hpp"
#include "../../networked_input_snapshot/networked_input_snapshot.hpp"

class Physics {
  public:
    Physics();
    ~Physics();

    ThreadSafeQueue<NetworkedInputSnapshot> input_snapshot_queue;
    JPH::PhysicsSystem physics_system;
    void update(float delta_time);

    JPH::BodyID sphere_id; // should be removed in a real program
    std::unordered_map<uint64_t, JPH::Ref<JPH::CharacterVirtual>> client_id_to_physics_character;
    // JPH::Ref<JPH::CharacterVirtual> character;

    void load_model_into_physics_world(Model *model);
    void create_character(uint64_t client_id);
    void delete_character(uint64_t client_id);
    void update_specific_character(float delta_time, uint64_t client_id_of_character);

  private:
    void initialize_engine();
    void initialize_world_objects();
    void clean_up_world();

    const unsigned int cMaxBodies = 1024;
    const unsigned int cNumBodyMutexes = 0;
    const unsigned int cMaxBodyPairs = 1024;
    const unsigned int cMaxContactConstraints = 1024;
    const int cCollisionSteps = 1;

    const float character_height = 2.0f;
    const float character_radius = 0.5f;

    JPH::TempAllocatorImpl *temp_allocator;
    JPH::JobSystemThreadPool *job_system;
    MyBodyActivationListener *body_activation_listener;
    MyContactListener *contact_listener;

    BPLayerInterfaceImpl broad_phase_layer_interface;
    ObjectVsBroadPhaseLayerFilterImpl object_vs_broadphase_layer_filter;
    ObjectLayerPairFilterImpl object_vs_object_layer_filter;

    std::vector<JPH::BodyID> created_body_ids;
};

#endif
