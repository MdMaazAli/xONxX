#pragma once

#include "lua.hpp"
#include "Physics.h"
#include <string>
#include <unordered_map>

class LuaRunner {
    public:
        void init();
        void shutdown();
        void LoadScript(SlotKey key, const std::string& path);
        void UnloadScript(SlotKey key, const std::string& path);
        void UnloadAllScripts(SlotKey key);
        void Tick(SlotKey key,const std::string& path, TransformComponent& transfom, PhysicsComponent& physcis, float dt);

        bool IsLoaded(SlotKey key, const std::string& path) const { 
            auto it = m_scriptEnvs.find(key);
            if (it == m_scriptEnvs.end()) return false;
            return it->second.find(path) != it->second.end();
        }

        void ReloadScript(SlotKey key, const std::string& path = "");
    
    private:
        lua_State* L = nullptr;
        std::unordered_map<SlotKey, std::unordered_map<std::string, int>> m_scriptEnvs;
};