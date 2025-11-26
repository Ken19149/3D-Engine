#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// JSON Library (Download nlohmann/json.hpp)
#include <nlohmann/json.hpp>

#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <map>

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

using json = nlohmann::json;
using namespace std;

// ==========================================
// 1. SHADERS (Hardcoded for simplicity)
// ==========================================
const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

out vec3 Normal;
out vec3 FragPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;  
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
)";

const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;
in vec3 Normal;
in vec3 FragPos;

uniform vec3 lightPos;
uniform vec3 objectColor;
uniform bool isSelected;

void main() {
    // Simple Lighting
    vec3 lightColor = vec3(1.0, 1.0, 1.0);
    vec3 ambient = 0.1 * lightColor;
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;
    
    vec3 result = (ambient + diffuse) * objectColor;

    // Highlight logic
    if(isSelected) {
        result = result + vec3(0.4, 0.4, 0.0); // Yellow glow
    }

    FragColor = vec4(result, 1.0);
}
)";

// ==========================================
// 2. CLASSES
// ==========================================

// Holds the actual OpenGL buffers. Shared by multiple objects.
struct Mesh {
    unsigned int VAO, VBO;
    int indexCount;
    
    // Generates a simple cube for testing if OBJ fails
    void createCube() {
        float vertices[] = {
            // positions          // normals
            -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
             0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
             0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
             0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
            -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
            -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,

            -0.5f, -0.5f,  0.5f,  0.0f,  0.0f, 1.0f,
             0.5f, -0.5f,  0.5f,  0.0f,  0.0f, 1.0f,
             0.5f,  0.5f,  0.5f,  0.0f,  0.0f, 1.0f,
             0.5f,  0.5f,  0.5f,  0.0f,  0.0f, 1.0f,
            -0.5f,  0.5f,  0.5f,  0.0f,  0.0f, 1.0f,
            -0.5f, -0.5f,  0.5f,  0.0f,  0.0f, 1.0f,
            
            // ... (Add other faces for a full cube) ...
            // For brevity, just 2 faces here. In real code, add all 6 faces.
        };

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        
        indexCount = 36; 
    }
};

// Resource Manager to load meshes once
class ResourceManager {
public:
    std::map<std::string, Mesh*> meshes;

    Mesh* getMesh(std::string name) {
        if (meshes.find(name) == meshes.end()) {
            // Load OBJ here using tinyobjloader
            // For now, return a default CUBE for everything
            Mesh* m = new Mesh();
            m->createCube();
            meshes[name] = m;
            std::cout << "Loaded (Placeholder): " << name << std::endl;
        }
        return meshes[name];
    }
};

// The Scene Node (The most important part)
class Node {
public:
    std::string name;
    std::string type; // "mesh" or "group"
    
    // Transform
    glm::vec3 pos = glm::vec3(0);
    glm::vec3 rot = glm::vec3(0); // Euler angles
    glm::vec3 scale = glm::vec3(1);

    Mesh* mesh = nullptr; // Null if it's a group
    std::vector<Node*> children;
    Node* parent = nullptr;

    bool isSelected = false;

    // Recursive Draw
    void Draw(unsigned int shaderID, glm::mat4 parentTransform) {
        // 1. Calculate Local Matrix
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, pos);
        model = glm::rotate(model, glm::radians(rot.x), glm::vec3(1,0,0));
        model = glm::rotate(model, glm::radians(rot.y), glm::vec3(0,1,0));
        model = glm::rotate(model, glm::radians(rot.z), glm::vec3(0,0,1));
        model = glm::scale(model, scale);

        // 2. Calculate Global Matrix
        glm::mat4 globalTransform = parentTransform * model;

        // 3. Draw Self (if mesh exists)
        if (mesh) {
            unsigned int modelLoc = glGetUniformLocation(shaderID, "model");
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(globalTransform));
            
            unsigned int colLoc = glGetUniformLocation(shaderID, "objectColor");
            unsigned int selLoc = glGetUniformLocation(shaderID, "isSelected");
            
            // Hardcoded color for demo
            glUniform3f(colLoc, 0.5f, 0.5f, 0.5f); 
            glUniform1i(selLoc, isSelected);

            glBindVertexArray(mesh->VAO);
            glDrawArrays(GL_TRIANGLES, 0, mesh->indexCount); // Use glDrawElements if using EBO
        }

        // 4. Draw Children
        for (Node* child : children) {
            child->Draw(shaderID, globalTransform);
        }
    }
};

// ==========================================
// 3. GLOBAL VARIABLES
// ==========================================
ResourceManager resources;
std::vector<Node*> sceneGraph; // Root nodes
Node* selectedNode = nullptr;
// For camera
glm::vec3 cameraPos   = glm::vec3(0.0f, 5.0f, 15.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp    = glm::vec3(0.0f, 1.0f,  0.0f);

// ==========================================
// 4. JSON LOADING HELPER
// ==========================================
Node* parseNode(json& jNode) {
    Node* n = new Node();
    n->name = jNode.value("name", "Unnamed");
    n->type = jNode.value("type", "group");

    // Load Transform
    if(jNode.contains("pos")) {
        n->pos.x = jNode["pos"][0]; n->pos.y = jNode["pos"][1]; n->pos.z = jNode["pos"][2];
    }
    if(jNode.contains("rot")) {
        n->rot.x = jNode["rot"][0]; n->rot.y = jNode["rot"][1]; n->rot.z = jNode["rot"][2];
    }
    if(jNode.contains("scale")) {
        n->scale.x = jNode["scale"][0]; n->scale.y = jNode["scale"][1]; n->scale.z = jNode["scale"][2];
    }

    // Load Mesh
    if (n->type == "mesh" && jNode.contains("model")) {
        n->mesh = resources.getMesh(jNode["model"]);
    }

    // Load Children Recursively
    if (jNode.contains("children")) {
        for (auto& jChild : jNode["children"]) {
            Node* childObj = parseNode(jChild);
            childObj->parent = n;
            n->children.push_back(childObj);
        }
    }

    return n;
}

void loadScene(std::string path) {
    std::ifstream f(path);
    if(!f.is_open()) {
        std::cerr << "Could not open " << path << std::endl;
        return;
    }
    json data = json::parse(f);

    // Setup Camera
    cameraPos.x = data["camera"]["pos"][0];
    cameraPos.y = data["camera"]["pos"][1];
    cameraPos.z = data["camera"]["pos"][2];

    // Build Graph
    for (auto& item : data["root"]) {
        sceneGraph.push_back(parseNode(item));
    }
    std::cout << "Scene Loaded." << std::endl;
}

// ==========================================
// 5. HELPER FUNCTIONS
// ==========================================
unsigned int createShader() {
    // Compile Vertex
    unsigned int vertex = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex, 1, &vertexShaderSource, NULL);
    glCompileShader(vertex);
    
    // Compile Fragment
    unsigned int fragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragment);

    // Link
    unsigned int program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    return program;
}

void processInput(GLFWwindow *window) {
    float speed = 0.05f;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        cameraPos += speed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        cameraPos -= speed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * speed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * speed;

    // Object Transformations (Only if selected)
    if(selectedNode) {
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) selectedNode->pos.x += 0.01f;
        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)  selectedNode->pos.x -= 0.01f;
        // Add rotation logic here...
    }
}

// Helper to flatten graph for selection UI
void getFlatList(Node* n, std::vector<Node*>& list) {
    list.push_back(n);
    for(auto c : n->children) getFlatList(c, list);
}

// ==========================================
// 6. MAIN
// ==========================================
int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "Room Editor", NULL, NULL);
    if (window == NULL) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { return -1; }

    glEnable(GL_DEPTH_TEST);

    unsigned int shaderProgram = createShader();

    // LOAD SCENE
    loadScene("scene.json");

    // UI State
    int uiSelectionIndex = 0;
    bool keyPressed = false;

    while (!glfwWindowShouldClose(window)) {
        processInput(window);

        // --- Selection Logic (Simple Cycle) ---
        if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS && !keyPressed) {
            std::vector<Node*> flatList;
            for(auto n : sceneGraph) getFlatList(n, flatList);
            
            // Deselect old
            if(selectedNode) selectedNode->isSelected = false;
            
            // Increment
            uiSelectionIndex++;
            if(uiSelectionIndex >= flatList.size()) uiSelectionIndex = 0;
            
            // Select new
            selectedNode = flatList[uiSelectionIndex];
            selectedNode->isSelected = true;
            
            std::cout << "Selected: " << selectedNode->name << std::endl;
            keyPressed = true;
        }
        if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_RELEASE) keyPressed = false;


        // --- Render ---
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        // Camera Transforms
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), 800.0f / 600.0f, 0.1f, 100.0f);
        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniform3f(glGetUniformLocation(shaderProgram, "lightPos"), 5.0f, 10.0f, 5.0f);

        // Draw Scene Graph
        glm::mat4 identity = glm::mat4(1.0f);
        for (Node* node : sceneGraph) {
            node->Draw(shaderProgram, identity);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
