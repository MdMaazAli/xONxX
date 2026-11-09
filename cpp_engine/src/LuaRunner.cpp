#include "LuaRunner.h"
#include <iostream>
#include <filesystem>

void LuaRunner::init(){
    L = luaL_newstate();
    luaL_openlibs(L);
}

void LuaRunner::shutdown(){
    if (L) {
        lua_close(L);
        L = nullptr;
    }
}

void LuaRunner::LoadScript(SlotKey key, std::string path){
    UnloadScript(key);

    std::string resolvedPath = path;
    if (!std::filesystem::exists(resolvedPath)) {
        if (std::filesystem::exists("cpp_engine/" + path)) {
            resolvedPath = "cpp_engine/" + path;
        } else if (path.rfind("cpp_engine/", 0) == 0 && std::filesystem::exists(path.substr(11))) {
            resolvedPath = path.substr(11);
        } else if (path.rfind("cpp_engine\\", 0) == 0 && std::filesystem::exists(path.substr(11))) {
            resolvedPath = path.substr(11);
        }
    }

    // < ---------- [LOAD] THE SCRIPT AS A COMPILED CHUNK ---------- >  STACK : [CHUNK]
    int result = luaL_loadfile(L, resolvedPath.c_str()); // loads - > compiles - > .lua files and leaves the chunks in stack, later sets env to that chunk

    if (result != 0) {
        std::cerr<<"[LUA_RUNNER] ERROR LOADING SCRIPTS : "<<lua_tostring(L, -1)<<std::endl;
        lua_pop(L, 1);
        return;
    }

    lua_newtable(L); // STACK : [CHUNK, ENV]
    lua_newtable(L); // STACK : [CHUNK, ENV, META]
    lua_getglobal(L, "_G"); // STACK : [CHUNK, ENV, META, _G ( GLOBAL ENV VAR )]
    lua_setfield(L, -2, "__index"); // meta.__index = _G (allows print, math, etc.) sets _G to meta - > [POP] _G
    lua_setmetatable(L, -2);        // setmetatable(env, meta) - > sets metatable to env - > [POP] META

    // [SAVE] REFRENCE TO ENV IN LUA REGISTRY ALLOWS US TO ACCESS ENV PER ENTITY
    lua_pushvalue(L, -1); // [DUPLICATE] ENV
    int envRef = luaL_ref(L, LUA_REGISTRYINDEX);

    lua_setfenv(L, -2); // BIND ENV TO CHUNK             
    if (lua_pcall(L, 0, 0, 0) != 0) {
        std::cerr << "[LUA_RUNNER] RUNTIME ERROR: " << lua_tostring(L, -1) << std::endl;
        lua_pop(L, 1);
        luaL_unref(L, LUA_REGISTRYINDEX, envRef);
        return;
    }

    m_loadedScripts[key] = path;
    m_scriptEnvs[key] = envRef;
    
    // all the functions defined in a lua script now are in lua's global namespaces table now we call it in lua's internal stack
    // it communicates with cpp via this internal stack

    lua_rawgeti(L, LUA_REGISTRYINDEX, envRef); // Push entity's env table
    lua_getfield(L, -1, "init");               // Push env.init
    if (lua_isfunction(L, -1)) {
        if (lua_pcall(L, 0, 0, 0) != 0) {
            std::cerr << "[LUA_RUNNER] ERROR IN INIT: " << lua_tostring(L, -1) << std::endl;
            lua_pop(L, 1);
        }
    } else {
        lua_pop(L, 1); // Pop non-function
    }
    lua_pop(L, 1);
}

void LuaRunner::UnloadScript(SlotKey key){
    auto it = m_scriptEnvs.find(key);
    if (it != m_scriptEnvs.end()) {
        luaL_unref(L, LUA_REGISTRYINDEX, it->second); 
        m_scriptEnvs.erase(it);
    }
    m_loadedScripts.erase(key);
}

void LuaRunner::Tick(SlotKey key, TransformComponent& transform, PhysicsComponent& physcis, float dt) {
    auto it = m_scriptEnvs.find(key);
    if (it == m_scriptEnvs.end()) return;
    
    lua_rawgeti(L, LUA_REGISTRYINDEX, it->second); // Push env table
    lua_getfield(L, -1, "update");                 // Push env.update
    lua_remove(L, -2);
    
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);
        return;
    }

    // < ---------- LUA ENTITY TABLE ---------- >
    lua_newtable(L); // arg1
    lua_pushnumber(L, transform.Position.x); lua_setfield(L, -2, "x");
    lua_pushnumber(L, transform.Position.y); lua_setfield(L, -2, "y");
    lua_pushnumber(L, transform.Position.z); lua_setfield(L, -2, "z");

    lua_pushnumber(L, transform.Size.x); lua_setfield(L, -2, "sx");
    lua_pushnumber(L, transform.Size.y); lua_setfield(L, -2, "sy");
    lua_pushnumber(L, transform.Size.z); lua_setfield(L, -2, "sz");

    lua_pushnumber(L, physcis.Velocity.x); lua_setfield(L, -2, "vx");
    lua_pushnumber(L, physcis.Velocity.y); lua_setfield(L, -2, "vy");
    lua_pushnumber(L, physcis.Velocity.z); lua_setfield(L, -2, "vz");

    lua_pushnumber(L, dt); // arg2

    lua_pushvalue(L, -2); // [DUPLICATE] ENTITY - > LATER ON READBACK - > UPDATE
    lua_insert(L, -4);
    
    if (lua_pcall(L, 2, 0, 0) != 0) {
        std::cerr << "[LUA_RUNNER] ERROR IN UPDATE : " << lua_tostring(L, -1) << std::endl;
        lua_pop(L, 2); 
        return;
    }

    // READING BACK VALUES AFTER UPDATE
    lua_getfield(L, -1, "x");  transform.Position.x = (float)lua_tonumber(L, -1); lua_pop(L, 1);
    lua_getfield(L, -1, "y");  transform.Position.y = (float)lua_tonumber(L, -1); lua_pop(L, 1);
    lua_getfield(L, -1, "z");  transform.Position.z = (float)lua_tonumber(L, -1); lua_pop(L, 1);
    
    lua_getfield(L, -1, "sx");  transform.Size.x = (float)lua_tonumber(L, -1); lua_pop(L, 1);
    lua_getfield(L, -1, "sy");  transform.Size.y = (float)lua_tonumber(L, -1); lua_pop(L, 1);
    lua_getfield(L, -1, "sz");  transform.Size.z = (float)lua_tonumber(L, -1); lua_pop(L, 1);

    lua_getfield(L, -1, "vx"); physcis.Velocity.x  = (float)lua_tonumber(L, -1); lua_pop(L, 1);
    lua_getfield(L, -1, "vy"); physcis.Velocity.y  = (float)lua_tonumber(L, -1); lua_pop(L, 1);
    lua_getfield(L, -1, "vz"); physcis.Velocity.z  = (float)lua_tonumber(L, -1); lua_pop(L, 1);

    lua_pop(L, 1);
}

void LuaRunner::ReloadScript(SlotKey key, const std::string& path) {
    std::string targetPath = path.empty() ? GetScriptPath(key) : path;
    if (!targetPath.empty()) {
        LoadScript(key, targetPath);
    }
}