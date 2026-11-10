#pragma once

#include "imgui.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <unordered_map>
#include "Physics.h"
#include "FileManager.h"

enum class EngineState { EDIT_MODE, TRAINING_MODE, INFERENCE_MODE };

class UIManager {
public:
    UIManager();
    ~UIManager();

    void Init();
    void Render(GLFWwindow* window, 
                GLuint fboTextureId, 
                std::vector<TransformComponent>& transforms, 
                std::vector<PhysicsComponent>& physics, 
                std::vector<AgentComponent>& agents,
                std::vector<ScriptComponent>& scripts,
                std::vector<EntityMetadata>& coldData,
                const std::vector<Slot>& slots,
                const std::unordered_map<uint64_t, SlotKey>& uuidToSlotKey,
                float framerate,
                bool& cursorInViewport,
                const glm::mat4& view,
                const glm::mat4& projection,
                uint64_t activeEntityUUID);

    EngineState GetState() const { return m_state; }

    uint64_t ConsumeHierarchySelection() { 
        uint64_t uuid = m_newlySelectedHierarchyUUID; 
        m_newlySelectedHierarchyUUID = 0; 
        return uuid; 
    }
    
    uint64_t ConsumeDeleteRequest() { 
        uint64_t uuid = m_deleteRequestUUID; 
        m_deleteRequestUUID = 0; 
        return uuid; 
    }

    uint64_t ConsumeReloadRequest(){
        uint64_t uuid = m_reloadScriptUUID;
        m_reloadScriptUUID = 0;
        return uuid;
    }

    bool ConsumeGlobalReloadRequest(){
        bool req = m_globalReloadRequest;
        m_globalReloadRequest = false;
        return req;
    }
    
    // --- [NEW] Drag & Drop Event Outputs for main.cpp ---
    bool HasNewDrop() const { return m_hasNewDrop; }
    std::string GetDropPayload() const { return m_dropPayload; }
    glm::vec2 GetDropNDC() const { return m_dropNDC; }

    // --- [NEW] Mouse Click Event Outputs for main.cpp ---
    bool HasNewClick() const { return m_hasNewClick; }
    glm::vec2 GetClickNDC() const { return m_clickNDC; }
    bool HasNewRightClick() const { return m_hasNewRightClick; }
    glm::vec2 GetRightClickNDC() const { return m_rightClickNDC; }
    
    // --- [NEW] Viewport Popup trigger ---
    void OpenEntityPopup(uint64_t entityUUID) { m_popupEntityUUID = entityUUID; }

private:
    void SetupDockingLayout();
    void ApplyRetroHardwareTheme();
    void GenerateProceduralMetalTexture();
    void DrawWindowBackground(GLuint textureID);

    FileManager m_fileManager;
    
    bool m_firstFrame;
    GLuint m_metalTexture;
    EngineState m_state;

    uint64_t m_newlySelectedHierarchyUUID = 0;
    uint64_t m_deleteRequestUUID = 0;

    //< ---------- [RELOAD] ---------- >
    uint64_t m_reloadScriptUUID = 0;
    bool m_globalReloadRequest = false;
    
    // Drop State
    bool m_hasNewDrop = false;
    std::string m_dropPayload = "";
    glm::vec2 m_dropNDC = glm::vec2(0.0f);

    // Click State
    bool m_hasNewClick = false;
    glm::vec2 m_clickNDC = glm::vec2(0.0f);
    bool m_hasNewRightClick = false;
    glm::vec2 m_rightClickNDC = glm::vec2(0.0f);
    
    // Popup State
    uint64_t m_popupEntityUUID = 0;
};
