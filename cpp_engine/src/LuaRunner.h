#pragma once

#include "lua.hpp"
#include "Physics.h"
#include <string>
#include <unordered_map>

class LuaRunner {
    public:
        void init();
        void shutdown();
        void LoadScript(SlotKey key, std::string path);
        void UnloadScript(SlotKey key);
        void Tick(SlotKey key, TransformComponent& transfom, PhysicsComponent& physcis, float dt);
        bool IsLoaded(SlotKey key) const { return m_loadedScripts.find(key) != m_loadedScripts.end(); }
        std::string GetScriptPath(SlotKey key) const {
            auto it = m_loadedScripts.find(key);
            return (it != m_loadedScripts.end()) ? it->second : "";
        }
        void ReloadScript(SlotKey key, const std::string& path = "");
    
    private:
        lua_State* L = nullptr;
        std::unordered_map<SlotKey, std::string> m_loadedScripts;
        std::unordered_map<SlotKey, int> m_scriptEnvs;
};