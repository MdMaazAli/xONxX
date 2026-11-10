#include "UIManager.h"
#include "imgui_internal.h"
#include <iostream>
#include <vector>
#include <cstdlib>
#include "ImGuizmo.h"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <fstream>

#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#endif
#include <cstdlib>
#include <filesystem>
#include <iostream>

// < --------------- SCRIPT ATTACHER CROSS PLATFORM CHECKER --------------- >
void LaunchScriptInEditor(const std::string& relativePath) {
    if (relativePath.empty()) return;

    // Resolve relative path to absolute so the OS knows exactly where it is
    std::filesystem::path fullPath = std::filesystem::absolute(relativePath);
    std::string pathStr = fullPath.string();

#if defined(_WIN32)
    // Try opening with the computer's registered "edit" application (VS Code, PyCharm, Notepad++, etc.)
    HINSTANCE result = ShellExecuteA(NULL, "edit", pathStr.c_str(), NULL, NULL, SW_SHOWNORMAL);
    
    // If no "edit" action is registered (error code <= 32), try the standard "open"
    if ((INT_PTR)result <= 32) {
        result = ShellExecuteA(NULL, "open", pathStr.c_str(), NULL, NULL, SW_SHOWNORMAL);
    }
    
    // Guaranteed fallback: If Windows has zero editor associated with .py, open with notepad
    if ((INT_PTR)result <= 32) {
        ShellExecuteA(NULL, "open", "notepad.exe", ("\"" + pathStr + "\"").c_str(), NULL, SW_SHOWNORMAL);
    }
#elif defined(__APPLE__)
    // macOS default text editor
    std::string cmd = "open -t \"" + pathStr + "\"";
    system(cmd.c_str());
#else
    // Linux default desktop text editor
    std::string cmd = "xdg-open \"" + pathStr + "\" &";
    system(cmd.c_str());
#endif
}

//  < ---------- PYTHON SCRIPT FOLDER CHECK ---------- >
void EnsureScriptExists(const std::string& relativePath, const std::string& modelType, int entityID) {
    std::filesystem::path p(relativePath);
    
    // Create the python_sscripts directory if it doesn't exist on this machine
    if (p.has_parent_path()) {
        std::filesystem::create_directories(p.parent_path());
    }
    // Create the starter script template
    if (!std::filesystem::exists(p)) {
        std::ofstream file(p);
        if (file.is_open()) {
            file << "# ==========================================================\n";
            file << "# " << modelType << " Script for Entity [" << entityID << "]\n";
            file << "# Connected to C++ Engine via FlatBuffers / ZeroMQ\n";
            file << "# ==========================================================\n\n";
            file << "import sys\nimport os\n\n";
            file << "class " << (modelType == "PPO Trainer" ? "PPOAgent" : "DQNAgent") << ":\n";
            file << "    def __init__(self):\n";
            file << "        self.model_type = \"" << modelType << "\"\n\n";
            file << "    def act(self, obs):\n";
            file << "        # 0 = Move Forward, 1 = Jump, 2 = Move Sideways\n";
            file << "        return 0\n\n";
            file << "def step(obs):\n";
            file << "    return 0\n";
            file.close();
        }
    }
}

void SetRigidbody(PhysicsComponent& phyComp) {
    phyComp.Rigidbody = Rigidbody::YES;
}

void RemoveRigidbody(PhysicsComponent& phyComp) {
    phyComp.Rigidbody = Rigidbody::NO;
}

void SetAgentBrain(AgentComponent& agent, int entityID, const std::string& modelType) {
    agent.Mode = AgentMode::TRAINING;
    agent.AgentModule = modelType;

    if (modelType == "PPO Trainer") {
        agent.ScriptPath = "python_scripts/ppo_agent_" + std::to_string(entityID) + ".py";
    } else {
        agent.ScriptPath = "python_scripts/dqn_agent_" + std::to_string(entityID) + ".py";
    }

    // Auto-create the folder and starter script if they don't exist yet
    EnsureScriptExists(agent.ScriptPath, modelType, entityID);
}

void RemoveAgentBrain(AgentComponent& agent) {
    agent.Mode = AgentMode::NONE;
    agent.AgentModule = "None";
    agent.ScriptPath = "";
}

void SetScript(ScriptComponent& script, std::string path) {
    std::filesystem::path p(path);
    std::string ext = p.extension().string();

    ScriptInstance inst;
    if (ext == ".py") inst.Type = ScriptType::PYTHON;
    else if (ext == ".cpp") inst.Type = ScriptType::CPP;
    else if (ext == ".lua") inst.Type = ScriptType::LUA;
    else inst.Type = ScriptType::NONE;

    inst.ScriptPath = path;
    inst.ScriptName = p.filename().string();
    inst.Is = Script::ENABLED;

    script.Instances.push_back(inst);

    std::cout<<"[ATTACHED] SCRIPT AT PATH : "<<path<<std::endl;
}

void RemoveScript(ScriptComponent& script, size_t index = 0) {
    if (index < script.Instances.size()) {
        script.Instances.erase(script.Instances.begin() + index);
    }
}

UIManager::UIManager() : m_firstFrame(true), m_metalTexture(0), m_state(EngineState::EDIT_MODE) {}

UIManager::~UIManager() {
    if (m_metalTexture != 0) {
        glDeleteTextures(1, &m_metalTexture);
        m_metalTexture = 0;
    }
}

void UIManager::GenerateProceduralMetalTexture() {
    int width = 512;
    int height = 512;
    std::vector<unsigned char> data(width * height * 4);
    
    // Generate base noise
    for (int i = 0; i < width * height; ++i) {
        unsigned char noise = 40 + (rand() % 25); // Dark grey noise
        data[i*4 + 0] = noise;
        data[i*4 + 1] = noise;
        data[i*4 + 2] = noise;
        data[i*4 + 3] = 255;
    }
    
    glGenTextures(1, &m_metalTexture);
    glBindTexture(GL_TEXTURE_2D, m_metalTexture);
    
    // Set wrapping to GL_REPEAT so it tiles seamlessly across large windows
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
}

void UIManager::Init() {
    GenerateProceduralMetalTexture();
    ApplyRetroHardwareTheme();
}

void UIManager::DrawWindowBackground(GLuint textureID) {
    if (textureID == 0) return;
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 p_min = ImGui::GetWindowPos();
    ImVec2 size = ImGui::GetWindowSize();
    
    // Manually tile the texture in 512x512 chunks to bypass OpenGL sampler clamp limits
    float tileSize = 512.0f;
    for (float y = 0; y < size.y; y += tileSize) {
        for (float x = 0; x < size.x; x += tileSize) {
            float draw_w = (x + tileSize > size.x) ? (size.x - x) : tileSize;
            float draw_h = (y + tileSize > size.y) ? (size.y - y) : tileSize;
            
            ImVec2 tile_min = ImVec2(p_min.x + x, p_min.y + y);
            ImVec2 tile_max = ImVec2(tile_min.x + draw_w, tile_min.y + draw_h);
            
            ImVec2 uv_min = ImVec2(0.0f, 0.0f);
            ImVec2 uv_max = ImVec2(draw_w / tileSize, draw_h / tileSize);
            
            draw_list->AddImage((void*)(intptr_t)textureID, tile_min, tile_max, uv_min, uv_max, IM_COL32(200, 210, 200, 255));
        }
    }
}

void UIManager::ApplyRetroHardwareTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    style.WindowRounding = 0.0f;
    style.FrameRounding = 0.0f;
    style.GrabRounding = 0.0f;
    style.WindowBorderSize = 2.0f;
    style.FrameBorderSize = 2.0f;
    style.ItemSpacing = ImVec2(10.0f, 8.0f);
    style.ScrollbarRounding = 0.0f;

    colors[ImGuiCol_WindowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_ChildBg]  = ImVec4(0.08f, 0.08f, 0.08f, 0.7f);
    colors[ImGuiCol_PopupBg]  = ImVec4(0.10f, 0.10f, 0.10f, 0.9f);
    colors[ImGuiCol_Border]               = ImVec4(0.35f, 0.40f, 0.35f, 1.0f); 
    colors[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 1.0f);
    colors[ImGuiCol_FrameBg]              = ImVec4(0.12f, 0.12f, 0.12f, 0.8f);
    colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.18f, 0.25f, 0.18f, 0.9f);
    colors[ImGuiCol_FrameBgActive]        = ImVec4(0.15f, 0.35f, 0.15f, 0.9f);
    colors[ImGuiCol_TitleBg]              = ImVec4(0.10f, 0.10f, 0.10f, 0.8f);
    colors[ImGuiCol_TitleBgActive]        = ImVec4(0.15f, 0.22f, 0.15f, 0.8f); 
    colors[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.05f, 0.05f, 0.05f, 0.8f);
    colors[ImGuiCol_Header]               = ImVec4(0.15f, 0.18f, 0.15f, 0.8f);
    colors[ImGuiCol_HeaderHovered]        = ImVec4(0.20f, 0.28f, 0.20f, 0.9f);
    colors[ImGuiCol_HeaderActive]         = ImVec4(0.25f, 0.35f, 0.25f, 0.9f);
    colors[ImGuiCol_Button]               = ImVec4(0.15f, 0.15f, 0.15f, 0.8f);
    colors[ImGuiCol_ButtonHovered]        = ImVec4(0.25f, 0.35f, 0.25f, 0.9f); 
    colors[ImGuiCol_ButtonActive]         = ImVec4(0.10f, 0.45f, 0.10f, 0.9f); 
    colors[ImGuiCol_Text]                 = ImVec4(0.4f, 1.0f, 0.6f, 1.0f);
    colors[ImGuiCol_TextDisabled]         = ImVec4(0.2f, 0.5f, 0.3f, 1.0f);
    colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.05f, 0.05f, 0.05f, 0.5f);
    colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.20f, 0.20f, 0.20f, 0.8f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.30f, 0.35f, 0.30f, 0.9f);
    colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.15f, 0.45f, 0.15f, 0.9f);
}

void UIManager::SetupDockingLayout() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGuiID dockspace_id = ImGui::GetID("ONxX_DockSpace");

    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);

    ImGuiID right_id;
    ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Right, 0.25f, &right_id, &dockspace_id);
    ImGuiID top_id;
    ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Up, 0.08f, &top_id, &dockspace_id);
    ImGuiID bottom_id;
    ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Down, 0.25f, &bottom_id, &dockspace_id);
    ImGuiID left_id;
    ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.08f, &left_id, &dockspace_id);

    ImGuiID left_bottom_id;
    ImGui::DockBuilderSplitNode(left_id, ImGuiDir_Down, 0.4f, &left_bottom_id, &left_id);

    ImGuiID right_bottom_id;
    ImGui::DockBuilderSplitNode(right_id, ImGuiDir_Down, 0.5f, &right_bottom_id, &right_id);

    ImGui::DockBuilderDockWindow("Telemetry", right_id);
    ImGui::DockBuilderDockWindow("Inspector", right_id);
    ImGui::DockBuilderDockWindow("Assets", right_bottom_id);
    ImGui::DockBuilderDockWindow("Top Bar", top_id);
    ImGui::DockBuilderDockWindow("Console", bottom_id);
    ImGui::DockBuilderDockWindow("Hierarchy", left_id);
    ImGui::DockBuilderDockWindow("Model Library", left_bottom_id);
    ImGui::DockBuilderDockWindow("Viewport", dockspace_id);

    ImGui::DockBuilderFinish(dockspace_id);
}

void UIManager::Render(GLFWwindow* window, GLuint fboTextureId, std::vector<TransformComponent>& transforms, std::vector<PhysicsComponent>& physics, std::vector<AgentComponent>& agents, std::vector<ScriptComponent>& scripts, std::vector<EntityMetadata>& coldData, const std::vector<Slot>& slots, const std::unordered_map<uint64_t, SlotKey>& uuidToSlotKey, float framerate, bool& cursorInViewport, const glm::mat4& view, const glm::mat4& projection, uint64_t activeEntityUUID) 
{
    ImGuizmo::BeginFrame();

    int activeEntityID = -1;
    if (activeEntityUUID != 0) {
        auto it = uuidToSlotKey.find(activeEntityUUID);
        if (it != uuidToSlotKey.end() && slots[it->second.index].alive && slots[it->second.index].generation == it->second.generation) {
            activeEntityID = static_cast<int>(it->second.index);
        }
    }

    m_hasNewDrop = false; // Reset event every frame
    m_hasNewClick = false; // Reset click every frame
    m_hasNewRightClick = false; // Reset right-click every frame
    if (m_firstFrame) {
        SetupDockingLayout();
        m_firstFrame = false;
    }

    ImGuiID dockspace_id = ImGui::GetID("ONxX_DockSpace");
    ImGui::DockSpaceOverViewport(dockspace_id, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

    ImGui::Begin("Telemetry", nullptr, ImGuiWindowFlags_NoCollapse);
    DrawWindowBackground(m_metalTexture);
    ImGui::Text("=== TELEMETRY ===");
    ImGui::Separator();
    if (!transforms.empty()) {
        ImGui::Text("POS X: %05.2f", transforms[0].Position.x);
        ImGui::Text("POS Y: %05.2f", transforms[0].Position.y);
        ImGui::Text("POS Z: %05.2f", transforms[0].Position.z);
    }
    ImGui::Separator();
    if (!physics.empty()) {
        ImGui::Text("VEL Y: %05.2f", physics[0].Velocity.y);
    }
    ImGui::Separator();
    ImGui::Text("FPS: %.1f", framerate);
    ImGui::End();

    // ----------------------- INSPECTOR TAB ----------------- //
    ImGui::Begin("Inspector", nullptr, ImGuiWindowFlags_NoCollapse);
    DrawWindowBackground(m_metalTexture);

    if (activeEntityID != -1 && activeEntityID < (int)transforms.size() && slots[activeEntityID].alive) {
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "%s [ID: %d]", coldData[activeEntityID].name.c_str(), activeEntityID);
        ImGui::SameLine();
        ImGui::TextDisabled("(Selected)");

        ImGui::SameLine(ImGui::GetWindowWidth() - 85);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.15f, 0.15f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.2f, 0.2f, 0.9f));
        if (ImGui::Button("Delete", ImVec2(70, 0))) {
            m_deleteRequestUUID = activeEntityUUID;
        }
        ImGui::PopStyleColor(2);

        char nameBuf[64];
        strncpy(nameBuf, coldData[activeEntityID].name.c_str(), sizeof(nameBuf));
        nameBuf[sizeof(nameBuf) - 1] = '\0';
        if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
            coldData[activeEntityID].name = nameBuf;
        }
        ImGui::TextDisabled("UUID: 0x%016llX", (unsigned long long)coldData[activeEntityID].uuid);

        ImGui::Separator();

        // < ------------------- TRANSFORM ------------------- >
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::DragFloat3("Position", &transforms[activeEntityID].Position.x, 0.05f);
            ImGui::DragFloat3("Size", &transforms[activeEntityID].Size.x, 0.05f);
        }

        // < ------------------- RIGIDBODY ------------------- >
        if (physics.size() > (size_t)activeEntityID) {
            auto& phyComp = physics[activeEntityID];
            bool hasRb = (phyComp.Rigidbody != Rigidbody::NO);

            if (ImGui::CollapsingHeader("Rigidbody", ImGuiTreeNodeFlags_DefaultOpen)) {
                if (ImGui::Checkbox("Enable Rigidbody", &hasRb)) {
                    if (hasRb) SetRigidbody(phyComp);
                    else RemoveRigidbody(phyComp);
                }

                if (hasRb) {
                    ImGui::DragFloat("Mass", &phyComp.Mass, 0.1f, 0.01f, 1000.0f);
                }
            }
        }

        // < ------------------- AI BRAIN ------------------- >
        if (agents.size() > (size_t)activeEntityID) {
            auto& agent = agents[activeEntityID];
            bool hasAgent = (agent.Mode != AgentMode::NONE);

            if (ImGui::CollapsingHeader("AI Agent Brain", ImGuiTreeNodeFlags_DefaultOpen)) {
                if (hasAgent) {
                    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "Module: %s", agent.AgentModule.c_str());
                    ImGui::TextDisabled("Script: %s", agent.ScriptPath.c_str());

                    if (ImGui::Button("Edit Brain Script")) {
                        LaunchScriptInEditor(agent.ScriptPath);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Remove Brain")) {
                        RemoveAgentBrain(agent);
                    }

                    if (ImGui::BeginMenu("Change Model")) {
                        if (ImGui::MenuItem("DQN Trainer", nullptr, agent.AgentModule == "DQN Trainer")) {
                            SetAgentBrain(agent, activeEntityID, "DQN Trainer");
                        }
                        if (ImGui::MenuItem("PPO Trainer", nullptr, agent.AgentModule == "PPO Trainer")) {
                            SetAgentBrain(agent, activeEntityID, "PPO Trainer");
                        }
                        ImGui::EndMenu();
                    }
                } else {
                    ImGui::TextDisabled("No AI Brain attached.");
                    if (ImGui::Button("Attach DQN Trainer")) {
                        SetAgentBrain(agent, activeEntityID, "DQN Trainer");
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Attach PPO Trainer")) {
                        SetAgentBrain(agent, activeEntityID, "PPO Trainer");
                    }
                }
            }
        }

        // < ------------------- SCRIPT ------------------- >
        if (scripts.size() > (size_t)activeEntityID) {
            auto& script = scripts[activeEntityID];
            bool hasScript = !script.Instances.empty();

            if (ImGui::CollapsingHeader("Script Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                if (hasScript) {
                    for (size_t s = 0; s < script.Instances.size(); ++s) {
                        auto& inst = script.Instances[s];
                        ImGui::PushID((int)s);

                        const char* langStr = "";
                        if (inst.Type == ScriptType::CPP) {
                            langStr = "[C++]";
                        }
                        else if (inst.Type == ScriptType::LUA) {
                            langStr = "[LUA]";
                        }
                        else {
                            langStr = "[PYTHON]";
                        }
                        ImVec4 langColor = (inst.Type == ScriptType::PYTHON) 
                            ? ImVec4(0.2f, 0.8f, 1.0f, 1.0f) 
                            : ImVec4(1.0f, 0.6f, 0.2f, 1.0f);

                        ImGui::TextColored(langColor, "%s", langStr);
                        ImGui::SameLine();
                        ImGui::Text("%s", inst.ScriptName.c_str());

                        bool isEnabled = (inst.Is == Script::ENABLED);
                        if (ImGui::Checkbox("Enabled", &isEnabled)) {
                            inst.Is = isEnabled ? Script::ENABLED : Script::DISABLED;
                        }

                        ImGui::TextDisabled("Path: %s", inst.ScriptPath.c_str());
                        
                        if (ImGui::Button("Reload Script")) {
                            m_reloadScriptUUID = activeEntityUUID;
                        }
                        if (ImGui::Button("Edit Script")) {
                            LaunchScriptInEditor(inst.ScriptPath);
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Remove Script")) {
                            RemoveScript(script, s);
                            ImGui::PopID();
                            break;
                        }
                        ImGui::Separator();
                        ImGui::PopID();
                    }
                } else {
                    ImGui::TextDisabled("No script attached.");
                    ImGui::TextDisabled("Drag & drop a script from Assets here.");
                }
            }
        }

        // Drag & Drop Target directly inside Inspector
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_FILE")) {
                const char* filepath = (const char*)payload->Data;
                SetScript(scripts[activeEntityID], filepath);
            }
            ImGui::EndDragDropTarget();
        }

    } else {
        ImGui::Dummy(ImVec2(0, 30));
        ImGui::TextDisabled("No entity selected.");
        ImGui::TextDisabled("Select an entity in the Hierarchy or");
        ImGui::TextDisabled("click it in the 3D Viewport to inspect.");
    }

    ImGui::End();

    // ------------------------------------------------------------------------------------------------------------------------
    // ------------------------------------------------------------------------------------------------------------------------
    // ------------------------------------------------------------------------------------------------------------------------

    ImGui::Begin("Top Bar", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);
    DrawWindowBackground(m_metalTexture);
    
    // --- MODE SWITCHER ---
    ImGui::Text("MODE:");
    ImGui::SameLine();
    
    // Helper lambda to draw a glowing button
    auto DrawModeButton = [&](const char* label, EngineState targetState, ImVec4 glowColor) {
        bool isActive = (m_state == targetState);
        if (isActive) {
            ImGui::PushStyleColor(ImGuiCol_Button, glowColor);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, glowColor);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, glowColor);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f)); // Black text on glow
            if (ImGui::Button(label)) m_state = targetState;
            ImGui::PopStyleColor(4);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.1f, 0.1f, 1.0f)); // Dim
            if (ImGui::Button(label)) m_state = targetState;
            ImGui::PopStyleColor(1);
        }
    };

    DrawModeButton("[ EDIT ]", EngineState::EDIT_MODE, ImVec4(0.3f, 1.0f, 0.3f, 1.0f)); // Neon Green
    ImGui::SameLine();
    DrawModeButton("[ TRAIN ]", EngineState::TRAINING_MODE, ImVec4(1.0f, 0.2f, 0.2f, 1.0f)); // Neon Red
    ImGui::SameLine();
    DrawModeButton("[ INFERENCE ]", EngineState::INFERENCE_MODE, ImVec4(0.2f, 0.6f, 1.0f, 1.0f)); // Neon Blue
    
    ImGui::SameLine(ImGui::GetWindowWidth() - 150); // Push to far right
    if (ImGui::Button("GLOBAL RELOAD")) {
        m_globalReloadRequest = true;
    }
    ImGui::End();

    ImGui::Begin("Console", nullptr, ImGuiWindowFlags_NoCollapse);
    DrawWindowBackground(m_metalTexture);
    
    // --- Engraved Watermark ---
    ImDrawList* console_draw = ImGui::GetWindowDrawList();
    const char* watermark = "ONxX";
    ImFont* font = ImGui::GetFont();
    float scale = 8.0f;
    float fontSize = ImGui::GetFontSize() * scale;
    ImVec2 t_size = font->CalcTextSizeA(fontSize, FLT_MAX, -1.0f, watermark);
    ImVec2 curPos = ImGui::GetCursorScreenPos();
    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImVec2 center_pos = ImVec2(curPos.x + (avail.x - t_size.x) * 0.5f, curPos.y + (avail.y - t_size.y) * 0.5f);
    
    // Highlight (simulates light catching the bottom edge of an indentation)
    console_draw->AddText(font, fontSize, ImVec2(center_pos.x + 3.0f, center_pos.y + 3.0f), IM_COL32(100, 110, 100, 80), watermark);
    // Deep shadow (simulates the depth of the stamp)
    console_draw->AddText(font, fontSize, center_pos, IM_COL32(15, 20, 15, 200), watermark);

    // ------------------------------------------------------------------------------------------------------------------------
    // ------------------------------------------------------------------------------------------------------------------------
    // ------------------------------------------------------------------------------------------------------------------------
    
    ImGui::Text("Initializing core modules...");
    ImGui::Text("ZMQ Listener active on port 5555.");
    ImGui::Text("Awaiting neural net commands...");
    ImGui::End();

    ImGui::Begin("Hierarchy", nullptr, ImGuiWindowFlags_NoCollapse);
    DrawWindowBackground(m_metalTexture);
    
    ImGui::Text("SCENE ENTITIES");
    ImGui::Separator();
    
    for (size_t i = 0; i < transforms.size(); ++i) {
        if (!slots[i].alive) continue;
        ImGui::PushID((int)i);
        
        char label[128];
        bool hasBrain = (agents.size() > i && agents[i].AgentModule != "None");
        bool hasScript = (scripts.size() > i && !scripts[i].Instances.empty());

        if (hasBrain && hasScript) {
            if (scripts[i].Instances.size() == 1) {
                snprintf(label, sizeof(label), "[%zu] %s (%s | %s)", i, coldData[i].name.c_str(), agents[i].AgentModule.c_str(), scripts[i].Instances[0].ScriptName.c_str());
            } else {
                snprintf(label, sizeof(label), "[%zu] %s (%s | %zu Scripts)", i, coldData[i].name.c_str(), agents[i].AgentModule.c_str(), scripts[i].Instances.size());
            }
        } else if (hasScript) {
            if (scripts[i].Instances.size() == 1) {
                snprintf(label, sizeof(label), "[%zu] %s (Script: %s)", i, coldData[i].name.c_str(), scripts[i].Instances[0].ScriptName.c_str());
            } else {
                snprintf(label, sizeof(label), "[%zu] %s (%zu Scripts)", i, coldData[i].name.c_str(), scripts[i].Instances.size());
            }
        } else if (hasBrain) {
            snprintf(label, sizeof(label), "[%zu] %s (Brain: %s)", i, coldData[i].name.c_str(), agents[i].AgentModule.c_str());
        } else {
            snprintf(label, sizeof(label), "[%zu] %s", i, coldData[i].name.c_str());
        }
        
        bool isSelected = (activeEntityUUID != 0 && coldData[i].uuid == activeEntityUUID); 
        
        // If the user clicks this text, it returns TRUE for exactly 1 frame
        if (ImGui::Selectable(label, isSelected)) {
            m_newlySelectedHierarchyUUID = coldData[i].uuid;
        }
        
        // < --------------------- Right Click Context Menu for Hierarchy --------------------- >
        if (isSelected && ImGui::BeginPopupContextItem("HierarchyEntityMenu")) {
            if(physics.size() > i){
                PhysicsComponent& phyComp = physics[i];
                bool isRb = (phyComp.Rigidbody != Rigidbody::NO);

                if (!isRb) {
                    if (ImGui::MenuItem("Rigidbody")) {
                        SetRigidbody(phyComp);
                    }
                }
                else{
                    ImGui::Separator();
                    if (ImGui::MenuItem("Remove Rigidbody")) {
                        RemoveRigidbody(phyComp);
                    }
                }
            }
            if (agents.size() > i) {
                auto& agent = agents[i];
                bool isAgent = (agent.Mode != AgentMode::NONE);

                if (!isAgent) {
                    if (ImGui::MenuItem("Convert to Agent Entity (DQN Default)")) {
                        SetAgentBrain(agent, (int)i, "DQN Trainer");
                    }
                } else {
                    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "Brain: %s", agent.AgentModule.c_str());
                    ImGui::TextDisabled("Script: %s", agent.ScriptPath.c_str());
                    ImGui::Separator();

                    if (ImGui::MenuItem("Edit Script")) {
                        LaunchScriptInEditor(agent.ScriptPath);
                    }
                    if (ImGui::BeginMenu("Change Model")) {
                        if (ImGui::MenuItem("DQN Trainer", nullptr, agent.AgentModule == "DQN Trainer")) {
                            SetAgentBrain(agent, (int)i, "DQN Trainer");
                        }
                        if (ImGui::MenuItem("PPO Trainer", nullptr, agent.AgentModule == "PPO Trainer")) {
                            SetAgentBrain(agent, (int)i, "PPO Trainer");
                        }
                        ImGui::EndMenu();
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Remove Agent Component")) {
                        RemoveAgentBrain(agent);
                    }
                }
            }
            if (scripts.size() > i && !scripts[i].Instances.empty()) {
                auto& script = scripts[i];
                for (size_t s = 0; s < script.Instances.size(); ++s) {
                    auto& inst = script.Instances[s];
                    ImGui::PushID((int)s);
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Script: %s", inst.ScriptName.c_str());

                    bool isEnabled = (inst.Is == Script::ENABLED);
                    if (ImGui::Checkbox("Enabled", &isEnabled)) {
                        inst.Is = isEnabled ? Script::ENABLED : Script::DISABLED;
                    }

                    if (ImGui::MenuItem("Edit Script")) {
                        LaunchScriptInEditor(inst.ScriptPath);
                    }
                    if (ImGui::MenuItem("Remove Script")) {
                        RemoveScript(script, s);
                        ImGui::PopID();
                        break;
                    }
                    ImGui::PopID();
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Delete")) {
                m_deleteRequestUUID = coldData[i].uuid;
            }
            ImGui::EndPopup();
        }
        
        // Setup Drag and Drop Target for the Hierarchy Item
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("RL_MODEL")) {
                const char* moduleName = (const char*)payload->Data;
                if (agents.size() > i) {
                    SetAgentBrain(agents[i], (int)i, moduleName);
                }
            }
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("RL_BRAIN")) {
                const char* moduleName = (const char*)payload->Data;
                if (agents.size() > i) {
                    agents[i].Mode = AgentMode::INFERENCE;
                    agents[i].AgentModule = moduleName;
                }
            }
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_FILE")){
                const char* filepath = (const char*)payload->Data;
                if (scripts.size() > i) {
                    SetScript(scripts[i], filepath);
                }
            }
            ImGui::EndDragDropTarget();
        }
        
        ImGui::PopID();
    }

    ImGui::End();

    // ------------------------------------------------------------------------------------------------------------------------
    // ------------------------------------------------------------------------------------------------------------------------
    // ------------------------------------------------------------------------------------------------------------------------

    // ----------------------- ASSETS TAB ----------------- //
    ImGui::Begin("Assets", nullptr, ImGuiWindowFlags_NoCollapse);
    DrawWindowBackground(m_metalTexture);
    
    ImGui::Text("MESHES");
    ImGui::Separator();
    
    ImVec2 meshBtnSize(80.0f, 80.0f);
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    ImGui::PushID("Primitive_Sphere");
    if (ImGui::Button("##SphereBtn", meshBtnSize)) {}
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        const char* payload = "Primitive_Sphere";
        ImGui::SetDragDropPayload("3D_PRIMITIVE", payload, strlen(payload) + 1);
        ImGui::Text("Drop to spawn a new 3D object on the floor...");
        ImGui::EndDragDropSource();
    }
    ImVec2 s_min = ImGui::GetItemRectMin();
    ImVec2 s_max = ImGui::GetItemRectMax();
    ImVec2 s_center = ImVec2(s_min.x + meshBtnSize.x * 0.5f, s_min.y + 30.0f);
    drawList->AddCircle(s_center, 18.0f, IM_COL32(100, 255, 150, 220), 24, 2.0f);
    drawList->AddLine(ImVec2(s_center.x - 18.0f, s_center.y), ImVec2(s_center.x + 18.0f, s_center.y), IM_COL32(80, 200, 120, 150), 1.5f);
    const char* s_text = "Sphere";
    ImVec2 s_tsize = ImGui::CalcTextSize(s_text);
    drawList->AddText(ImVec2(s_min.x + (meshBtnSize.x - s_tsize.x) * 0.5f, s_max.y - s_tsize.y - 6.0f), IM_COL32(220, 240, 220, 255), s_text);
    ImGui::PopID();

    ImGui::SameLine();

    ImGui::PushID("Primitive_Cube");
    if (ImGui::Button("##CubeBtn", meshBtnSize)) {}
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        const char* payload = "Primitive_Cube";
        ImGui::SetDragDropPayload("3D_PRIMITIVE", payload, strlen(payload) + 1);
        ImGui::Text("Drop to spawn a new 3D object on the floor...");
        ImGui::EndDragDropSource();
    }
    ImVec2 c_min = ImGui::GetItemRectMin();
    ImVec2 c_max = ImGui::GetItemRectMax();
    ImVec2 c_center = ImVec2(c_min.x + meshBtnSize.x * 0.5f, c_min.y + 30.0f);
    float cs = 13.0f;
    ImVec2 top[4] = {
        ImVec2(c_center.x, c_center.y - cs),
        ImVec2(c_center.x + cs, c_center.y - cs * 0.5f),
        ImVec2(c_center.x, c_center.y),
        ImVec2(c_center.x - cs, c_center.y - cs * 0.5f)
    };
    drawList->AddPolyline(top, 4, IM_COL32(100, 255, 150, 220), ImDrawFlags_Closed, 2.0f);
    drawList->AddLine(top[2], ImVec2(c_center.x, c_center.y + cs), IM_COL32(100, 255, 150, 220), 2.0f);
    drawList->AddLine(top[1], ImVec2(top[1].x, top[1].y + cs), IM_COL32(100, 255, 150, 220), 2.0f);
    drawList->AddLine(top[3], ImVec2(top[3].x, top[3].y + cs), IM_COL32(100, 255, 150, 220), 2.0f);
    drawList->AddLine(ImVec2(top[3].x, top[3].y + cs), ImVec2(c_center.x, c_center.y + cs), IM_COL32(100, 255, 150, 220), 2.0f);
    drawList->AddLine(ImVec2(top[1].x, top[1].y + cs), ImVec2(c_center.x, c_center.y + cs), IM_COL32(100, 255, 150, 220), 2.0f);
    const char* c_text = "Cube";
    ImVec2 c_tsize = ImGui::CalcTextSize(c_text);
    drawList->AddText(ImVec2(c_min.x + (meshBtnSize.x - c_tsize.x) * 0.5f, c_max.y - c_tsize.y - 6.0f), IM_COL32(220, 240, 220, 255), c_text);
    ImGui::PopID();

    ImGui::SameLine();

    ImGui::PushID("Primitive_Pyramid");
    if (ImGui::Button("##PyramidBtn", meshBtnSize)) {}
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        const char* payload = "Primitive_Pyramid";
        ImGui::SetDragDropPayload("3D_PRIMITIVE", payload, strlen(payload) + 1);
        ImGui::Text("Drop to spawn a new 3D object on the floor...");
        ImGui::EndDragDropSource();
    }
    ImVec2 p_bmin = ImGui::GetItemRectMin();
    ImVec2 p_bmax = ImGui::GetItemRectMax();
    ImVec2 p_center = ImVec2(p_bmin.x + meshBtnSize.x * 0.5f, p_bmin.y + 30.0f);
    float ps = 15.0f;
    ImVec2 apex = ImVec2(p_center.x, p_center.y - ps);
    ImVec2 baseL = ImVec2(p_center.x - ps, p_center.y + ps * 0.6f);
    ImVec2 baseM = ImVec2(p_center.x - ps * 0.2f, p_center.y + ps * 0.8f);
    ImVec2 baseR = ImVec2(p_center.x + ps, p_center.y + ps * 0.5f);
    drawList->AddTriangle(apex, baseL, baseM, IM_COL32(100, 255, 150, 220), 2.0f);
    drawList->AddTriangle(apex, baseM, baseR, IM_COL32(100, 255, 150, 220), 2.0f);
    drawList->AddLine(baseL, baseR, IM_COL32(80, 200, 120, 150), 1.5f);
    const char* p_text = "Pyramid";
    ImVec2 p_tsize = ImGui::CalcTextSize(p_text);
    drawList->AddText(ImVec2(p_bmin.x + (meshBtnSize.x - p_tsize.x) * 0.5f, p_bmax.y - p_tsize.y - 6.0f), IM_COL32(220, 240, 220, 255), p_text);
    ImGui::PopID();

    ImGui::PushID("python_scripts");

    ImGui::Text("Python Scripts");
    ImGui::Separator();

    if (ImGui::BeginChild("Python Scripts", ImVec2(0, 200), true)) {
        m_fileManager.DrawDirectoryNode("python_scripts");
    }
    ImGui::EndChild(); 

    if (m_fileManager.hasDoubleClicked()) {
        LaunchScriptInEditor(m_fileManager.ConsumeDoubleClickedPath());
    }

    ImGui::PopID();

    ImGui::PushID("directory");

    ImGui::Dummy(ImVec2(0, 15));
    ImGui::Text("Directory");
    ImGui::Separator();

    if (ImGui::BeginChild("Directory", ImVec2(0, 200), true)) {
        m_fileManager.DrawDirectoryNode("directory");
    }
    ImGui::EndChild();

    if (m_fileManager.hasDoubleClicked()) {
        LaunchScriptInEditor(m_fileManager.ConsumeDoubleClickedPath());
    }

    ImGui::PopID();

    ImGui::End();

    // ------------------------------------------------------------------------------------------------------------------------
    // ------------------------------------------------------------------------------------------------------------------------
    // ------------------------------------------------------------------------------------------------------------------------

    ImGui::Begin("Model Library", nullptr, ImGuiWindowFlags_NoCollapse);
    DrawWindowBackground(m_metalTexture);
    
    ImGui::Text("MODEL SCRIPTS");
    ImGui::Separator();
    
    ImVec2 modelBtnSize(180.0f, 32.0f);

    // Create draggable modules
    ImGui::Button("DQN Trainer", modelBtnSize);
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        const char* payload = "DQN Trainer";
        ImGui::SetDragDropPayload("RL_MODEL", payload, strlen(payload) + 1);
        ImGui::Text("Attach DQN Trainer to Entity...");
        ImGui::EndDragDropSource();
    }
    
    ImGui::Button("PPO Trainer", modelBtnSize);
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        const char* payload = "PPO Trainer";
        ImGui::SetDragDropPayload("RL_MODEL", payload, strlen(payload) + 1);
        ImGui::Text("Attach PPO Trainer to Entity...");
        ImGui::EndDragDropSource();
    }
    
    ImGui::Dummy(ImVec2(0, 20));
    ImGui::Text("EXTRACTED BRAINS");
    ImGui::Separator();
    
    ImGui::Button("DQN_Brain_v1.onnx", modelBtnSize);
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        const char* payload = "DQN_Brain_v1";
        ImGui::SetDragDropPayload("RL_BRAIN", payload, strlen(payload) + 1);
        ImGui::Text("Plug DQN_Brain_v1 into Entity...");
        ImGui::EndDragDropSource();
    }
    
    ImGui::End();

    // ------------------------------------------------------------------------------------------------------------------------
    // ------------------------------------------------------------------------------------------------------------------------
    // ------------------------------------------------------------------------------------------------------------------------

    ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoCollapse);
    ImVec2 availSpace = ImGui::GetContentRegionAvail();
    float targetAspect = 1920.0f / 1080.0f;
    float panelAspect = availSpace.x / availSpace.y;

    ImVec2 finalSize;
    if (panelAspect > targetAspect) {
        finalSize.y = availSpace.y;
        finalSize.x = availSpace.y * targetAspect;
    } else {
        finalSize.x = availSpace.x;
        finalSize.y = availSpace.x / targetAspect;
    }

    float cursorX = (availSpace.x - finalSize.x) * 0.5f;
    float cursorY = (availSpace.y - finalSize.y) * 0.5f;
    ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + cursorX, ImGui::GetCursorPosY() + cursorY));

    ImGui::Image((void*)(intptr_t)fboTextureId, finalSize, ImVec2(0, 1), ImVec2(1, 0));
    cursorInViewport = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    
    // --- [NEW] ImGuizmo Rendering ---
    ImVec2 p0 = ImGui::GetItemRectMin();
    ImVec2 p1 = ImGui::GetItemRectMax();
    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(p0.x, p0.y, p1.x - p0.x, p1.y - p0.y);
    
    if (activeEntityID != -1 && activeEntityID < transforms.size()) {
        glm::mat4 modelMatrix = glm::translate(glm::mat4(1.0f), transforms[activeEntityID].Position);
        
        ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(projection),
                             ImGuizmo::TRANSLATE, ImGuizmo::WORLD,
                             glm::value_ptr(modelMatrix));
                             
        // Update transforms if manipulated
        if (ImGuizmo::IsUsing()) {
            transforms[activeEntityID].Position = glm::vec3(modelMatrix[3]);
        }
    }

    // ------------------------------------------------------------------------------------------------------------------------
    // ------------------------------------------------------------------------------------------------------------------------
    // ------------------------------------------------------------------------------------------------------------------------
    
    // Setup Drag and Drop Target on the Viewport
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("RL_MODEL")) {
            const char* moduleName = (const char*)payload->Data;
            if(activeEntityID != -1 && activeEntityID < (int)agents.size()){
                SetAgentBrain(agents[activeEntityID], activeEntityID, moduleName);
            }
        }
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("RL_BRAIN")) {
            const char* moduleName = (const char*)payload->Data;
            if (!agents.empty()) {
                agents[0].Mode = AgentMode::INFERENCE;
                agents[0].AgentModule = moduleName;
            }
        }
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("3D_PRIMITIVE")) {
            m_hasNewDrop = true;
            m_dropPayload = (const char*)payload->Data;
            
            ImVec2 mousePos = ImGui::GetMousePos();
            m_dropNDC.x = (2.0f * (mousePos.x - p0.x)) / finalSize.x - 1.0f;
            m_dropNDC.y = 1.0f - (2.0f * (mousePos.y - p0.y)) / finalSize.y;
        }
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_FILE")) {
            const char* filepath = (const char*)payload->Data;
            if (activeEntityID != -1 && activeEntityID < (int)scripts.size()) {
                SetScript(scripts[activeEntityID], filepath);
            }
        }
        ImGui::EndDragDropTarget();
    }
    
    ImVec2 mousePos = ImGui::GetMousePos();
    
    auto calcNDC = [&]() {
        return glm::vec2(
            (2.0f * (mousePos.x - p0.x)) / finalSize.x - 1.0f,
            1.0f - (2.0f * (mousePos.y - p0.y)) / finalSize.y
        );
    };

    if (ImGui::IsWindowHovered()) {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            if (!ImGuizmo::IsOver()) {
                m_hasNewClick = true;
                m_clickNDC = calcNDC();
            }
        }
        
        // Track Right Clicks for Context Menu
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            if (!ImGuizmo::IsOver()) {
                m_hasNewRightClick = true;
                m_rightClickNDC = calcNDC();
            }
        }
    }

    // ------------------------------------------------------------------------------------------------------------------------
    // ------------------------------------------------------------------------------------------------------------------------
    // ------------------------------------------------------------------------------------------------------------------------

    static uint64_t activePopupUUID = 0;
    if (m_popupEntityUUID != 0) {
        activePopupUUID = m_popupEntityUUID;
        ImGui::OpenPopup("ViewportEntityMenu");
        m_popupEntityUUID = 0;
    }

    int activePopupID = -1;
    if (activePopupUUID != 0) {
        auto it = uuidToSlotKey.find(activePopupUUID);
        if (it != uuidToSlotKey.end() && slots[it->second.index].alive && slots[it->second.index].generation == it->second.generation) {
            activePopupID = static_cast<int>(it->second.index);
        }
    }
    
    if (ImGui::BeginPopup("ViewportEntityMenu")) {
        if (activePopupID >= 0 && (size_t)activePopupID < coldData.size() && slots[activePopupID].alive) {
            ImGui::Text("%s [ID: %d]", coldData[activePopupID].name.c_str(), activePopupID);
            ImGui::TextDisabled("UUID: 0x%016llX", (unsigned long long)coldData[activePopupID].uuid);
        } else {
            ImGui::Text("Entity ID: %d", activePopupID);
        }
        ImGui::Separator();
        if (physics.size() > (size_t)activePopupID && activePopupID >= 0) {
            PhysicsComponent& phyComp = physics[activePopupID];
            bool isRb = (phyComp.Rigidbody != Rigidbody::NO);

            if (!isRb) {
                if (ImGui::MenuItem("Rigidbody")) {
                    SetRigidbody(phyComp);
                }
            } else {
                ImGui::Separator();
                if (ImGui::MenuItem("Remove Rigidbody")) {
                    RemoveRigidbody(phyComp);
                }
            }
            ImGui::Separator();
        }
        if (agents.size() > (size_t)activePopupID && activePopupID >= 0) {
            auto& agent = agents[activePopupID];
            bool isAgent = (agent.Mode != AgentMode::NONE);

            if (!isAgent) {
                if (ImGui::MenuItem("Convert to Agent Entity (DQN Default)")) {
                    SetAgentBrain(agent, activePopupID, "DQN Trainer");
                }
            } else {
                ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "Brain: %s", agent.AgentModule.c_str());
                ImGui::TextDisabled("Script: %s", agent.ScriptPath.c_str());
                ImGui::Separator();

                if (ImGui::MenuItem("Edit Script")) {
                    LaunchScriptInEditor(agent.ScriptPath);
                }
                if (ImGui::BeginMenu("Change Model")) {
                    if (ImGui::MenuItem("DQN Trainer", nullptr, agent.AgentModule == "DQN Trainer")) {
                        SetAgentBrain(agent, activePopupID, "DQN Trainer");
                    }
                    if (ImGui::MenuItem("PPO Trainer", nullptr, agent.AgentModule == "PPO Trainer")) {
                        SetAgentBrain(agent, activePopupID, "PPO Trainer");
                    }
                    ImGui::EndMenu();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Remove Agent Component")) {
                    RemoveAgentBrain(agent);
                }
            }
            if (activePopupID != -1 && (size_t)activePopupID < scripts.size() && !scripts[activePopupID].Instances.empty()) {
                auto& script = scripts[activePopupID];
                for (size_t s = 0; s < script.Instances.size(); ++s) {
                    auto& inst = script.Instances[s];
                    ImGui::PushID((int)s);
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Script: %s", inst.ScriptName.c_str());

                    bool isEnabled = (inst.Is == Script::ENABLED);
                    if (ImGui::Checkbox("Enabled", &isEnabled)) {
                        inst.Is = isEnabled ? Script::ENABLED : Script::DISABLED;
                    }

                    if (ImGui::MenuItem("Edit Script")) {
                        LaunchScriptInEditor(inst.ScriptPath);
                    }
                    if (ImGui::MenuItem("Remove Script")) {
                        RemoveScript(script, s);
                        ImGui::PopID();
                        break;
                    }
                    ImGui::PopID();
                }
            }
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Delete")) {
            m_deleteRequestUUID = activePopupUUID;
        }
        ImGui::EndPopup();
    }
    
    ImGui::End();
}
