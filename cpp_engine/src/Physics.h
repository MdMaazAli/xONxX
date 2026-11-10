#ifndef PHYSICS_H
#define PHYSICS_H

#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <cstdint>
#include <random>

// Generational SlotKey for O(1) safe runtime handles without array shifting
struct SlotKey {
    uint32_t index = 0;
    uint32_t generation = 1;

    uint64_t ToRaw() const {
        return (static_cast<uint64_t>(generation) << 32) | index;
    }
    static SlotKey FromRaw(uint64_t raw) {
        return SlotKey{ static_cast<uint32_t>(raw & 0xFFFFFFFF), static_cast<uint32_t>(raw >> 32) };
    }
    bool operator==(const SlotKey& other) const { return index == other.index && generation == other.generation; }
    bool operator!=(const SlotKey& other) const { return !(*this == other); }
    explicit operator bool() const { return generation != 0; }
};

struct Slot {
    uint32_t generation = 1;
    bool alive = false;
};

// Cold Domain metadata (Authoring UUID, Decoupled Name)
struct EntityMetadata {
    uint64_t uuid = 0;
    std::string name = "Entity";
};

inline uint64_t GenerateUUID() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis;
    uint64_t val = dis(gen);
    return (val == 0) ? 1 : val;
}

namespace std {
    template<> struct hash<SlotKey> {
        size_t operator()(const SlotKey& k) const noexcept {
            return std::hash<uint64_t>()(k.ToRaw());
        }
    };
}

// Entity Component System(ECS) ----- > basically making a stride of all the necesssary data
// what is a stride you may ask it is nothing but an array of different data bundeled together so that it makes it easier to fetch as it can be stored inside a cache
// like in this case for player(any mesh) we need some things like its Pos,Vel or M etc if its bundeled togetehr CPU would 
// require less time to get em as we are mostlikely to use their properties together like say the Pos changes and its due to Vel
//  meaning we need both Pos and Vel to be fetched so budling them together helps (it fetches directly from L1/L2 caches)

enum class Entity{INACTIVE, ACTIVE};
enum class AgentMode{NONE, TRAINING, INFERENCE};
enum class MeshType{MESH, SPHERE, CUBE, PYRAMID};
enum class Rigidbody{YES, NO};
enum class ScriptType{PYTHON, CPP, LUA, NONE};
enum class Script{DISABLED, ENABLED};

struct AgentComponent{
    AgentMode Mode = AgentMode::NONE;
    std::string AgentModule = "None";
    std::string ScriptPath = "";
};

struct MeshComponent{
    MeshType Mesh = MeshType::SPHERE;
    // -------------------------------------- > materials 
};

// Plain Old Data(POD)
struct TransformComponent{
    Entity Is = Entity::INACTIVE;
    glm::vec3 Position;
    glm::vec3 Size;
};

struct PhysicsComponent{
    Rigidbody Rigidbody = Rigidbody::NO;
    glm::vec3 Velocity;
    float Mass;
};

struct ScriptInstance{
    ScriptType Type = ScriptType::NONE;
    std::string ScriptPath = "";
    std::string ScriptName = "None";
    Script Is = Script::ENABLED;
};

struct ScriptComponent{
    std::vector<ScriptInstance> Instances;
    bool empty() const { return Instances.empty(); }
    size_t Size() const { return Instances.size(); }
};

// Physics System class which handle most of the physics related stuff
class PhysicsSystem{
    public:
        static void Update(std::vector<TransformComponent>& transforms, std::vector<PhysicsComponent>& physics, const std::vector<Slot>& slots, float dt){
            // seq loop over the stride(clearing it out)
            for(size_t i=0; i<transforms.size(); i++){
                if(!slots[i].alive) continue;
                transforms[i].Position += physics[i].Velocity*dt;
                
                if(physics[i].Rigidbody == Rigidbody::YES){
                    if(transforms[i].Position.y>0.0f){
                        physics[i].Velocity.y -= 9.81f*dt;
                    }
                    
                    if(transforms[i].Position.y<0.0f){
                        transforms[i].Position.y = 0.0f;
                        
                        if(physics[i].Velocity.y<0.0f){
                            physics[i].Velocity.y = 0.0f;
                        }
                    }
                }
            }
        }
};

#endif // PHYSICS_H