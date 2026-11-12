#include <zmq.hpp>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <random>
#include <vector>
#include <unordered_set>
#include <windows.h>
#include <dbghelp.h>
#include <csignal>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Include the FlatBuffers generated schema
#include "state_generated.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "UIManager.h"
#include "Framebuffer.h"
#include "Shader.h"
#include "Camera.h"
#include "SimpleMesh.h"
#include "SimpleMesh2D.h"
#include "Physics.h"
#include "Collision.h"

#include "LuaRunner.h"
#include "FileWatcher.h"

#pragma comment(lib, "dbghelp.lib")

const unsigned int SCR_WIDTH = 1920;
const unsigned int SCR_HEIGHT = 1080;

Camera camera(glm::vec3(0.0f, 6.0f, 12.0f), glm::vec3(0.0f, 1.0f, 0.0f), -90.0f, -25.0f);

float xPos = 0.0f;
float zPos = 0.0f;
glm::vec3 lightPos = glm::vec3(1.0f,1.0f,2.0f);
glm::vec3 lightColor = glm::vec3(0.8f,0.8f,0.2f);

bool mouseFirst = true;
float lastX = 1920.0f/2.0f;
float lastY = 1080.0f/2.0f;

bool altKeyPressed = false;     // Prevent toggle key spamming
bool cursorInViewport = false;

bool isFullscreen = false;
bool f11KeyPressed = false;
int windowedPosX = 100, windowedPosY = 100;
int windowedWidth = 1920, windowedHeight = 1080;

void mouse_callback(GLFWwindow* window, double xPosIn, double yPosIn)
{
    // Only rotate when ALT is held AND
    // the mouse is inside the 3D viewport.
    if (!altKeyPressed || !cursorInViewport)
    {
        mouseFirst = true;
        return;
    }

    float xPosVal = static_cast<float>(xPosIn);
    float yPosVal = static_cast<float>(yPosIn);

    if (mouseFirst)
    {
        lastX = xPosVal;
        lastY = yPosVal;
        mouseFirst = false;
        return;
    }

    float xoffset = xPosVal - lastX;
    float yoffset = lastY - yPosVal;

    lastX = xPosVal;
    lastY = yPosVal;

    camera.ProcessMouseMovement(xoffset, yoffset);
}

void scroll_callback(GLFWwindow* window,double xOffset,double yOffset){
    if (!cursorInViewport)
        return;
    camera.ProcessMouseScroll((float)yOffset);
}

float deltaTime = 0.0f;
float lastFrame = 0.0f;

void processInput(GLFWwindow* window){
    if(glfwGetKey(window,GLFW_KEY_ESCAPE) == GLFW_PRESS){
        glfwSetWindowShouldClose(window,true);
    }
    if (cursorInViewport) {
        if(glfwGetKey(window,GLFW_KEY_W) == GLFW_PRESS){
            camera.ProcessKeyboard(FORWARD,deltaTime);
        }
        if(glfwGetKey(window,GLFW_KEY_S) == GLFW_PRESS){
            camera.ProcessKeyboard(BACKWARD,deltaTime);
        }
        if(glfwGetKey(window,GLFW_KEY_A) == GLFW_PRESS){
            camera.ProcessKeyboard(LEFT,deltaTime);
        }
        if(glfwGetKey(window,GLFW_KEY_D) == GLFW_PRESS){
            camera.ProcessKeyboard(RIGHT,deltaTime);
        }
    }

    // --- CURSOR UNLOCK TOGGLE ---
    // --- ALT = CAMERA ROTATION MODIFIER ---
    altKeyPressed = glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS;
    if (!altKeyPressed){
        mouseFirst = true;
    }

    // --- FULLSCREEN TOGGLE (F11) ---
    if (glfwGetKey(window, GLFW_KEY_F11) == GLFW_PRESS) {
        if (!f11KeyPressed) {
            f11KeyPressed = true;
            isFullscreen = !isFullscreen;

            if(isFullscreen){
                // Backup the current window size and position before going fullscreen
                glfwGetWindowPos(window, &windowedPosX, &windowedPosY);
                glfwGetWindowSize(window, &windowedWidth, &windowedHeight); 
                
                // Get the primary monitor and its resolution
                GLFWmonitor* monitor = glfwGetPrimaryMonitor();
                const GLFWvidmode* mode = glfwGetVideoMode(monitor);
                
                // Switch to fullscreen borderless
                glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
            } 
            else{
                // Restore to the original windowed state
                glfwSetWindowMonitor(window, nullptr, windowedPosX, windowedPosY, windowedWidth, windowedHeight, 0);
            }
        }
    }
    if (glfwGetKey(window, GLFW_KEY_F11) == GLFW_RELEASE) {
        f11KeyPressed = false;
    }
}

void framebuffer_size_callback(GLFWwindow* window,int width,int height){
    glViewport(0,0,width,height);
}

void initModel(Shader& shader, glm::mat4 model, glm::mat4 view, glm::mat4 projection, glm::vec3 lightColor, glm::vec3 lightPosView){
    shader.setMat4("model", model);
    shader.setMat4("view", view);
    shader.setMat4("projection", projection);
    shader.setVec3("lightColor", lightColor);
    shader.setVec3("lightPosView", lightPosView);
}

int main() {

    zmq::context_t context(1);
    zmq::socket_t socket(context, ZMQ_REP);
    socket.bind("tcp://*:5555");
    
    std::cout << "Engine online (FlatBuffers enabled). Waiting for Python actions..." << std::endl;

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "ONxX",NULL,NULL);
    if(window == NULL){
        std::cerr<<"failed to initialize the window"<<endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)){
        std::cerr<<"failed to initialise GLAD"<<endl;
        return -1;
    }
    
    // Offscreen texture size (matches initial viewport resolution) 
    Framebuffer fbo(1920, 1080);
    
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    // Initialize ImGui Context 
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    
    // Bind GLFW and OpenGL backends to ImGui 
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    ImGui::StyleColorsDark(); // UI theme

    UIManager uiManager;
    uiManager.Init();

    glEnable(GL_DEPTH_TEST);
    glViewport(0, 0, 1920, 1080);

    // < ---------- GRID ---------- >
    Shader gridShader("src/gridVert.glsl","src/gridFrag.glsl");
    SimpleMesh grid("grid");

    Shader sphereShader("src/sphrVertShader.glsl", "src/sphrFragShader.glsl");
    SimpleMesh sphrMesh("sphere");
    // ------------------------------------------------------------------------------- > need to change the shader sources
    Shader cubeShader("src/sphrVertShader.glsl", "src/sphrFragShader.glsl");
    SimpleMesh cubeMesh("cube");
    Shader pyramidShader("src/sphrVertShader.glsl", "src/sphrFragShader.glsl");
    SimpleMesh pyramidMesh("pyramid");

    // -------------------------------------------------------------------------------------------------------------------------
    // -------------------------------------------------------------------------------------------------------------------------
    // -------------------------------------------------------------------------------------------------------------------------

    // Hot Simulation Domain ( POD )
    std::vector<MeshComponent> meshes;
    std::vector<TransformComponent> transforms;
    std::vector<PhysicsComponent> physics;
    std::vector<AgentComponent> agents;
    std::vector<ScriptComponent> scripts;

    // Cold Metadata Domain (Authoring UUID, Decoupled Names)
    std::vector<EntityMetadata> coldData;

    // Slot Map Domain (Generational safety, zero memory shifting)
    std::vector<Slot> slots;
    std::vector<uint32_t> freeSlots;
    std::unordered_map<uint64_t, SlotKey> uuidToSlotKey;

    auto AllocateEntity = [&](const EntityMetadata& meta,
                              const MeshComponent& mesh,
                              const TransformComponent& transform,
                              const PhysicsComponent& phys,
                              const AgentComponent& agent,
                              const ScriptComponent& script) -> SlotKey {
        SlotKey key;
        if (!freeSlots.empty()) {
            key.index = freeSlots.back();
            freeSlots.pop_back();
            key.generation = slots[key.index].generation;
            slots[key.index].alive = true;

            meshes[key.index] = mesh;
            transforms[key.index] = transform;
            physics[key.index] = phys;
            agents[key.index] = agent;
            scripts[key.index] = script;
            coldData[key.index] = meta;
        } else {
            key.index = static_cast<uint32_t>(transforms.size());
            key.generation = 1;
            slots.push_back({1, true});

            meshes.push_back(mesh);
            transforms.push_back(transform);
            physics.push_back(phys);
            agents.push_back(agent);
            scripts.push_back(script);
            coldData.push_back(meta);
        }
        uuidToSlotKey[meta.uuid] = key;
        return key;
    };

    auto FreeEntity = [&](uint32_t index) {
        if (index >= slots.size() || !slots[index].alive) return;
        
        uuidToSlotKey.erase(coldData[index].uuid);
        slots[index].alive = false;
        slots[index].generation++; // Invalidate stale SlotKeys
        transforms[index].Is = Entity::INACTIVE;
        freeSlots.push_back(index);
    };

    // SNAPSHOT STATE ECS DIRECTORY (for saving/restoring state)
    std::vector<MeshComponent> snapshotMeshes;
    std::vector<TransformComponent> snapshotTransforms;
    std::vector<PhysicsComponent> snapshotPhysics;
    std::vector<AgentComponent> snapshotAgents;
    std::vector<ScriptComponent> snapshotScripts;
    std::vector<EntityMetadata> snapshotColdData;
    std::vector<Slot> snapshotSlots;
    std::vector<uint32_t> snapshotFreeSlots;
    std::unordered_map<uint64_t, SlotKey> snapshotUuidToSlotKey;

    // entity = 0 ------ > RL_agent
    AllocateEntity(
        { GenerateUUID(), "AGENT ENTITY" },
        { MeshType::SPHERE },
        { Entity::ACTIVE, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f) },
        { Rigidbody::NO, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f },
        { AgentMode::NONE, "None" },
        { }
    );

    // < ------------------- SNAPSHOT ------------------- >
    auto TakeSnapshot = [&]() {
        snapshotMeshes = meshes;
        snapshotTransforms = transforms;
        snapshotPhysics = physics;
        snapshotAgents = agents;
        snapshotScripts = scripts;
        snapshotColdData = coldData;
        snapshotSlots = slots;
        snapshotFreeSlots = freeSlots;
        snapshotUuidToSlotKey = uuidToSlotKey;

        std::cout<<"[SNAPSHOT] TRANSFORM SIZE : "<<transforms.size()<<endl;
    };

    auto RestoreSnapshot = [&]() {
        meshes = snapshotMeshes;
        transforms = snapshotTransforms;
        physics = snapshotPhysics;
        agents = snapshotAgents;
        scripts = snapshotScripts;
        coldData = snapshotColdData;
        slots = snapshotSlots;
        freeSlots = snapshotFreeSlots;
        uuidToSlotKey = snapshotUuidToSlotKey;

        std::cout<<"[RESTORE] TRANSFORM SIZE : "<<transforms.size()<<endl;
    };

    LuaRunner luaRunner;
    luaRunner.init();

    std::string watchDir = std::filesystem::exists("directory") ? "directory" : "cpp_engine/directory";
    FileWatcher fileWatcher;
    fileWatcher.Start(watchDir);
    std::cout << "[WATCHER] Watching directory: " << watchDir << std::endl;
    
    float accTime = 0.0f,dt = 1.0/60.0f;
    
    static uint64_t selectedUUID = 0;

    while(!glfwWindowShouldClose(window)){
        float currFrame = glfwGetTime();
        deltaTime = currFrame-lastFrame;
        lastFrame = currFrame;
        
        // < --------------- STATE MANAGEMENT --------------- >
        static EngineState prevState = EngineState::EDIT_MODE;
        EngineState currState = uiManager.GetState();
        
        if(currState != EngineState::EDIT_MODE) {
            accTime += deltaTime;
        }

        if (prevState != currState) {
            accTime = 0.0f;
            if(prevState == EngineState::EDIT_MODE) {
                TakeSnapshot();
                luaRunner.ResetAll();
            }
            else if(currState == EngineState::EDIT_MODE) {
                RestoreSnapshot();
                luaRunner.ResetAll();
            }
            
            prevState = currState;
        }
        
        processInput(window);

        if (currState == EngineState::TRAINING_MODE || currState == EngineState::INFERENCE_MODE){
            
            zmq::message_t request;
            auto received = socket.recv(request, zmq::recv_flags::dontwait); // Non-blocking check
            
            if(received.has_value()){
                std::string action = request.to_string();
                float reward = 0.7f; 
                bool done = false;

                if(action == "RESET"){
                    RestoreSnapshot();
                    luaRunner.ResetAll();
                    std::cout << "[RESET] Environment reset to starting line." << std::endl;
                }
                else{
                    try{
                        int action_idx = std::stoi(action);
                        std::cout << "[Action received]: " << action_idx << std::endl;

                        // Simple Physics
                        if (action_idx == 0){
                            physics[0].Velocity.x += 0.7f; // MOVE FORWARD
                        }
                        if (action_idx == 1){
                            physics[0].Velocity.y += 7.0f; // JUMP
                        }
                        if (action_idx == 2){
                            physics[0].Velocity.z += 2.0f; // MOVE SIDEWAYS
                        }

                        done = (transforms[0].Position.x > 10.0f);
                    }
                    catch(const std::exception& e){
                        std::cerr<<"Socket parse error:"<<e.what()<<endl;
                    }
                }

                // Pack FlatBuffers Payload
                flatbuffers::FlatBufferBuilder builder(1024);
                RL::Vec3 position(transforms[0].Position.x, transforms[0].Position.y, transforms[0].Position.z);
                RL::Vec3 velocity(physics[0].Velocity.x, physics[0].Velocity.y, physics[0].Velocity.z);
                auto env_state_offset = RL::CreateEnvState(builder, &position, &velocity, reward, done);
                builder.Finish(env_state_offset);

                // Sending reply in binary --- > flatbuffers --- > python 
                uint8_t* buffer_pointer = builder.GetBufferPointer();
                size_t buffer_size = builder.GetSize();

                zmq::message_t reply(builder.GetSize());
                memcpy(reply.data(), buffer_pointer, buffer_size);
                socket.send(reply, zmq::send_flags::none);
            }
            
        }
        
        // -------------------------------------------------------------------------------------------------------------------    
        // -------------------------------------------------------------------------------------------------------------------
        // -------------------------------------------------------------------------------------------------------------------

        // < -------------------- physics loop ------------------ >
        if (currState != EngineState::EDIT_MODE){
            
            while(accTime>=dt){
                // ----------------------------------------------------------------------------------- > BUGS NEED TO FIX
                for (size_t i = 0; i < transforms.size(); ++i) {
                    if (!slots[i].alive) continue;
                    SlotKey key = { (uint32_t)i, slots[i].generation };
        
                    for (size_t j = 0; j < scripts[i].Instances.size(); j++) {
                        auto inst = scripts[i].Instances[j];
                        if (inst.Type == ScriptType::LUA && inst.Is == Script::ENABLED && !inst.ScriptPath.empty()) {
                            if (!luaRunner.IsLoaded(key, inst.ScriptPath)) {
                                luaRunner.LoadScript(key, inst.ScriptPath);
                            }
                            luaRunner.Tick(key,inst.ScriptPath, transforms[i], physics[i], dt);
                        } else if (luaRunner.IsLoaded(key, inst.ScriptPath)) {
                            luaRunner.UnloadScript(key, inst.ScriptPath);
                        }
                    }
                }

                // < ----- gravity ----- >
                PhysicsSystem::Update(transforms, physics, slots, dt);
                accTime -= dt;
            }

        }

        fbo.Bind(); // Direct all 3D drawing commands into our FBO texture attachment
        
        glClearColor(0.2f,0.3f,0.3f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        glm::mat4 projection = glm::mat4(1.0f);
        projection = glm::perspective(radians(camera.Zoom),1920.0f/1080.0f,0.1f,1000.0f);
        
        glm::mat4 view = glm::mat4(1.0f);
        view = camera.GetViewMatrix();
        
        // ALL SHADERS
        sphereShader.use();
        cubeShader.use();
        pyramidShader.use();
        
        // < ------- MESH REGISTRY GO THROUGH BEFORE RENDERING ------- >
        for(size_t i=0; i<transforms.size(); i++){
            if(!slots[i].alive) continue;
            if(meshes[i].Mesh==MeshType::SPHERE){
                glm::mat4 sphrModel = glm::mat4(1.0f);
                sphrModel = glm::translate(sphrModel,transforms[i].Position);
                sphrModel = glm::scale(sphrModel, transforms[i].Size);
                initModel(sphereShader,sphrModel,view,projection,lightColor,lightPos);
                sphrMesh.draw();
            }
            else if(meshes[i].Mesh==MeshType::CUBE){
                glm::mat4 cubeModel = glm::mat4(1.0f);
                cubeModel = glm::translate(cubeModel,transforms[i].Position);
                cubeModel = glm::scale(cubeModel, transforms[i].Size);
                initModel(sphereShader,cubeModel,view,projection,lightColor,lightPos);
                cubeMesh.draw();
            }
            else if(meshes[i].Mesh==MeshType::PYRAMID){
                glm::mat4 pyramidModel = glm::mat4(1.0f);
                pyramidModel = glm::translate(pyramidModel,transforms[i].Position);
                pyramidModel = glm::scale(pyramidModel, transforms[i].Size);
                initModel(sphereShader,pyramidModel,view,projection,lightColor,lightPos);   
                pyramidMesh.draw();
            }
            else if(meshes[i].Mesh==MeshType::MESH);
        }
        
        // < ------- GRID ------- >
        gridShader.use();
        glm::mat4 gridModel = glm::mat4(1.0f);
        initModel(gridShader, gridModel, view, projection, lightColor, lightPos);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        grid.draw();

        // Dynamically get the current size of the GLFW window
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        
        // Restore the viewport to match the actual window dimensions
        fbo.Unbind(display_w, display_h); 
        
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        uiManager.Render(window, fbo.textureId, transforms, physics, agents, scripts, coldData, slots, uuidToSlotKey, io.Framerate, cursorInViewport, view, projection, selectedUUID);
        ImGui::Render();

        // -------------------------------------------------------------------------------------------------------------------    
        // -------------------------------------------------------------------------------------------------------------------
        // -------------------------------------------------------------------------------------------------------------------

        // < ------- ACTIVE/INACTIVE ENTITY HANDLER ------ >
        uint64_t hierarchyClickUUID = uiManager.ConsumeHierarchySelection();
        if (hierarchyClickUUID != 0) {
            auto it = uuidToSlotKey.find(hierarchyClickUUID);
            if (it != uuidToSlotKey.end() && slots[it->second.index].alive) {
                // Turn off the old entity if there was one
                if (selectedUUID != 0) {
                    auto oldIt = uuidToSlotKey.find(selectedUUID);
                    if (oldIt != uuidToSlotKey.end() && slots[oldIt->second.index].alive) {
                        transforms[oldIt->second.index].Is = Entity::INACTIVE;
                    }
                }
                
                // Turn on the new entity
                selectedUUID = hierarchyClickUUID;
                transforms[it->second.index].Is = Entity::ACTIVE;
                
                cout << "HIERARCHY SELECTED ENTITY UUID: 0x" << std::hex << selectedUUID << std::dec << endl;
            }
        }
        
        // < ------- DELETE ENTITY HANDLER ------ >
        uint64_t deleteUUID = uiManager.ConsumeDeleteRequest();
        if (deleteUUID != 0) {
            auto it = uuidToSlotKey.find(deleteUUID);
            if (it != uuidToSlotKey.end()) {
                SlotKey key = it->second;
                if (slots[key.index].alive && slots[key.index].generation == key.generation) {
                    luaRunner.UnloadAllScripts(key);
                    FreeEntity(key.index);
                    if (selectedUUID == deleteUUID) {
                        selectedUUID = 0;
                    }

                    std::cout << "[DELETE] ENTITY UUID: 0x" << std::hex << deleteUUID << std::dec << " FREED (NO SHIFTING)" << endl;
                }
            }
        }

        uint64_t reloadUUID = uiManager.ConsumeReloadRequest();
        if (reloadUUID != 0) {
            auto it = uuidToSlotKey.find(reloadUUID);
            if (it != uuidToSlotKey.end()) {
                SlotKey key = it->second;
                if (slots[key.index].alive && slots[key.index].generation == key.generation) {
                    for (auto inst: scripts[key.index].Instances) {
                        if (inst.Type == ScriptType::LUA) {
                            luaRunner.ReloadScript(key, inst.ScriptPath);
                            std::cout << "[LUA] Reloaded script for entity: " << coldData[key.index].name << std::endl;
                        }
                    }
                }
            }
        }

        if (uiManager.ConsumeGlobalReloadRequest()) {
            for (size_t i = 0; i < transforms.size(); ++i) {
                if (!slots[i].alive) continue;
                for (auto inst: scripts[i].Instances) {
                    if (inst.Type == ScriptType::LUA && inst.Is == Script::ENABLED && !inst.ScriptPath.empty()) {
                        SlotKey key = { (uint32_t)i, slots[i].generation };
                        luaRunner.ReloadScript(key, inst.ScriptPath);
                    }
                }
            }
            std::cout << "[LUA] Global script reload completed!" << std::endl;
        }

        // < ---------- [HOT RELOADING] ---------- >
        auto modifiedFiles = fileWatcher.ConsumeModifiedFiles();
        if (!modifiedFiles.empty()) {
            std::unordered_set<std::string> unqFiles(modifiedFiles.begin(), modifiedFiles.end());

            for (auto changedFile: unqFiles) {
                std::string changedName = std::filesystem::path(changedFile).filename().string();

                for (size_t i = 0; i < transforms.size(); i++){
                    if (!slots[i].alive) continue;
                    for (auto inst: scripts[i].Instances) {
                        if (inst.Type == ScriptType::LUA && inst.Is == Script::ENABLED) {
                            std::string scriptName = std::filesystem::path(inst.ScriptPath).filename().string();
                            if (scriptName == changedName) {
                                SlotKey key = { (uint32_t)i, slots[i].generation};
                                luaRunner.ReloadScript(key, inst.ScriptPath);
                                std::cout << "[WATCHER] Hot-reloaded: " << inst.ScriptPath << " on entity: " << coldData[key.index].name << std::endl;
                            }
                        }
                    } 
                }
            }
        }

        // -------------------------------------------------------------------------------------------------------------------    
        // -------------------------------------------------------------------------------------------------------------------
        // -------------------------------------------------------------------------------------------------------------------
        
        if (currState == EngineState::EDIT_MODE){
            
            // < ---------- RAY CASTING FOR EXISTING ENTITIES ---------- >
            if(uiManager.HasNewClick()){
                glm::vec2 ndc = uiManager.GetClickNDC();
            
                glm::vec4 ray_clip = glm::vec4(ndc.x, ndc.y, -1.0f, 1.0f);
                glm::vec4 ray_eye = glm::inverse(projection)*ray_clip;
                ray_eye = glm::vec4(ray_eye.x, ray_eye.y, -1.0f, 0.0f);
            
                glm::vec3 ray_world = glm::normalize(glm::inverse(view)*ray_eye);

                int closestEntityID = -1;
                float closestDistance = 99999.0f;
                
                for(size_t i=0; i<transforms.size(); i++){
                    if(!slots[i].alive) continue;
                    glm::vec3 halfSize = transforms[i].Size/2.0f;
                    glm::vec3 boxMin = transforms[i].Position-halfSize;
                    glm::vec3 boxMax = transforms[i].Position+halfSize;

                    float hitDistance = 0.0f;

                    if(Collision::RayCastIntersects(camera.Position, ray_world, boxMin, boxMax, hitDistance)){
                        if(hitDistance<closestDistance){
                            closestDistance = hitDistance;
                            closestEntityID = (int)i;
                        }
                    }
                }
                
                if (selectedUUID != 0) {
                    auto oldIt = uuidToSlotKey.find(selectedUUID);
                    if (oldIt != uuidToSlotKey.end() && slots[oldIt->second.index].alive) {
                        transforms[oldIt->second.index].Is = Entity::INACTIVE;
                    }
                }

                if(closestEntityID!=-1 && slots[closestEntityID].alive){
                    // < ----- ACTIVE ENTITY ID ----- >
                    selectedUUID = coldData[closestEntityID].uuid;
                    transforms[closestEntityID].Is = Entity::ACTIVE;
                    cout<<"ENTITY UUID: 0x"<<std::hex<<selectedUUID<<std::dec<<" ACTIVE "<<"SELECTED AT A DISTANCE OF:"<<closestDistance<<endl;
                }
                else{
                    selectedUUID = 0; // Deselect when clicking empty space
                    cout<<"VOID"<<endl;
                }
            
            }
        
            // < ---------- RIGHT CLICK RAY CASTING (CONTEXT MENU) ---------- >
            if (uiManager.HasNewRightClick()) {
                glm::vec2 ndc = uiManager.GetRightClickNDC();

                glm::vec4 ray_clip = glm::vec4(ndc.x, ndc.y, -1.0f, 1.0f);
                glm::vec4 ray_eye = glm::inverse(projection) * ray_clip;
                ray_eye = glm::vec4(ray_eye.x, ray_eye.y, -1.0f, 0.0f);

                glm::vec3 ray_world = glm::normalize(glm::inverse(view) * ray_eye);

                int closestEntityID = -1;
                float closestDistance = 99999.0f;

                for (size_t i = 0; i < transforms.size(); i++) {
                    if (!slots[i].alive) continue;
                    glm::vec3 halfSize = transforms[i].Size / 2.0f;
                    glm::vec3 boxMin = transforms[i].Position - halfSize;
                    glm::vec3 boxMax = transforms[i].Position + halfSize;

                    float hitDistance = 0.0f;
                    if (Collision::RayCastIntersects(camera.Position, ray_world, boxMin, boxMax, hitDistance)) {
                        if (hitDistance < closestDistance) {
                            closestDistance = hitDistance;
                            closestEntityID = (int)i;
                        }
                    }
                }

                if (closestEntityID != -1 && slots[closestEntityID].alive) {
                    if (coldData[closestEntityID].uuid == selectedUUID) {
                        uiManager.OpenEntityPopup(selectedUUID);
                    }
                }
            }
            
            // -------------------------------------------------------------------------------------------------------------------    
            // -------------------------------------------------------------------------------------------------------------------
            // -------------------------------------------------------------------------------------------------------------------
            
            // < ---------- RAYCASTING TO SPAWN NEW ENTITIES ---------- >
            if (uiManager.HasNewDrop()) {
                std::string payloadMesh = uiManager.GetDropPayload();
                glm::vec2 ndc = uiManager.GetDropNDC();
                
                // Convert 2D NDC to 4D Clip Coordinates (pointing into the screen)
                glm::vec4 ray_clip = glm::vec4(ndc.x, ndc.y, -1.0f, 1.0f);
                
                // Un-project using the Inverse Camera Matrices
                glm::vec4 ray_eye = glm::inverse(projection) * ray_clip;
                ray_eye = glm::vec4(ray_eye.x, ray_eye.y, -1.0f, 0.0f); // Make it a direction vector
                
                glm::vec3 ray_world = glm::vec3(glm::inverse(view) * ray_eye);
                ray_world = glm::normalize(ray_world); // This is our 3D Laser!
                
                // Intersect the laser with the Floor (y = 1.0f)
                glm::vec3 spawnPoint = Collision::RayIntersectsFloor(camera.Position, ray_world, 0.0f);
                
                // Create the new Entity at the exact drop location!
                MeshType typeToSpawn = MeshType::SPHERE;
                if(payloadMesh=="Primitive_Sphere"){
                    typeToSpawn = MeshType::SPHERE;
                }
                if(payloadMesh=="Primitive_Cube"){
                    typeToSpawn = MeshType::CUBE;
                }
                if(payloadMesh=="Primitive_Pyramid"){
                    typeToSpawn = MeshType::PYRAMID;
                }
                AllocateEntity(
                    { GenerateUUID(), "Entity" },
                    { typeToSpawn },
                    { Entity::INACTIVE, spawnPoint, glm::vec3(1.0f, 1.0f, 1.0f) },
                    { Rigidbody::NO, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f },
                    { AgentMode::NONE, "None" },
                    { }
                );

                std::cout << "[SPAWN] New Entity created at X:" << spawnPoint.x << " Z:" << spawnPoint.z << std::endl;
            
            }

        }
        else{
            selectedUUID = 0;
        }

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();

        // Caps headless update speed when waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }

    fileWatcher.Stop();
    luaRunner.shutdown();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}