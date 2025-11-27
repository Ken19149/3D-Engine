#include <GL/glut.h>  // Takes care of GL/gl.h and GL/glu.h automatically

// UTILITIES
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <cmath>

// LIBRARIES
#include <nlohmann/json.hpp>
using json = nlohmann::json;

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

// ==========================================
// 1. DATA STRUCTURES
// ==========================================

struct Model {
    std::string name;
    std::vector<float> vertices;
    std::vector<float> normals;
    bool loaded = false;
};

class Node {
public:
    std::string name;
    std::string type;
    
    // Transform
    float x = 0, y = 0, z = 0;
    float rx = 0, ry = 0, rz = 0;
    float sx = 1, sy = 1, sz = 1;

    Model* modelData = nullptr;
    std::vector<Node*> children;
    
    bool isSelected = false;
    bool isAnimated = false;
    float animSpeed = 0.0f;

    void DrawLegacy() {
        glPushMatrix();
        glTranslatef(x, y, z);
        glRotatef(rz, 0, 0, 1); // Z
        glRotatef(ry, 0, 1, 0); // Y
        glRotatef(rx, 1, 0, 0); // X
        glScalef(sx, sy, sz);

        if (modelData && modelData->loaded) {
            if (isSelected) glColor3f(1.0f, 1.0f, 0.0f); // Yellow highlight
            else glColor3f(1.0f, 1.0f, 1.0f);            // White default

            glBegin(GL_TRIANGLES);
            for (size_t i = 0; i < modelData->vertices.size() / 3; i++) {
                if (!modelData->normals.empty()) {
                    glNormal3f(modelData->normals[3*i+0], modelData->normals[3*i+1], modelData->normals[3*i+2]);
                }
                glVertex3f(modelData->vertices[3*i+0], modelData->vertices[3*i+1], modelData->vertices[3*i+2]);
            }
            glEnd();
        }

        for (Node* child : children) child->DrawLegacy();
        glPopMatrix();
    }
};

// ==========================================
// 2. GLOBALS
// ==========================================
std::vector<Node*> sceneGraph;
std::vector<Node*> flatList;
std::vector<Model*> loadedModels; // Pointers to prevent crashing!

Node* selectedNode = nullptr;
int selectionIndex = 0;
bool clockRunning = true;

// Camera (Z-Up default)
float camX = 0.0f, camY = -5.0f, camZ = 2.0f; // Back 5 units, Up 2 units

// ==========================================
// 3. MODEL LOADING
// ==========================================
Model* GetModel(std::string filename) {
    for (auto* m : loadedModels) {
        if (m->name == filename) return m;
    }

    std::cout << "Loading: " << filename << " ... ";
    Model* m = new Model(); // Allocate on heap
    m->name = filename;
    
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    std::string fullPath = "models/" + filename;
    bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, fullPath.c_str(), "models/");

    if (!warn.empty()) std::cout << "WARN: " << warn << std::endl;
    if (!ret) {
        std::cout << "FAILED! " << err << std::endl;
        delete m;
        return nullptr;
    }

    // Flatten
    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            m->vertices.push_back(attrib.vertices[3 * index.vertex_index + 0]);
            m->vertices.push_back(attrib.vertices[3 * index.vertex_index + 1]);
            m->vertices.push_back(attrib.vertices[3 * index.vertex_index + 2]);

            if (index.normal_index >= 0) {
                m->normals.push_back(attrib.normals[3 * index.normal_index + 0]);
                m->normals.push_back(attrib.normals[3 * index.normal_index + 1]);
                m->normals.push_back(attrib.normals[3 * index.normal_index + 2]);
            }
        }
    }
    
    m->loaded = true;
    std::cout << "Success (" << m->vertices.size()/3 << " tris)" << std::endl;
    loadedModels.push_back(m);
    return m;
}

// ==========================================
// 4. JSON LOADER
// ==========================================
Node* parseNode(json& jNode) {
    Node* n = new Node();
    n->name = jNode.value("name", "Unnamed");
    n->type = jNode.value("type", "group");

    if(jNode.contains("pos")) { n->x = jNode["pos"][0]; n->y = jNode["pos"][1]; n->z = jNode["pos"][2]; }
    if(jNode.contains("rot")) { n->rx = jNode["rot"][0]; n->ry = jNode["rot"][1]; n->rz = jNode["rot"][2]; }
    if(jNode.contains("scale")) { n->sx = jNode["scale"][0]; n->sy = jNode["scale"][1]; n->sz = jNode["scale"][2]; }
    
    n->isAnimated = jNode.value("isAnimated", false);
    n->animSpeed = jNode.value("speed", 1.0f);

    if (n->type == "mesh" && jNode.contains("model")) {
        n->modelData = GetModel(jNode["model"]);
    }

    if (jNode.contains("children")) {
        for (auto& jChild : jNode["children"]) {
            Node* childObj = parseNode(jChild);
            n->children.push_back(childObj);
        }
    }
    
    flatList.push_back(n);
    return n;
}

void LoadScene(std::string path) {
    std::ifstream f(path);
    if(!f.is_open()) { std::cerr << "No scene.json found!\n"; return; }
    json data = json::parse(f);

    for (auto& item : data["root"]) {
        sceneGraph.push_back(parseNode(item));
    }
    if(!flatList.empty()) {
        selectedNode = flatList[0];
        selectedNode->isSelected = true;
    }
}

// ==========================================
// 5. GLUT FUNCTIONS (Standard Class Style)
// ==========================================
void init() {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    
    // Basic Light
    GLfloat light_pos[] = { 10.0, 10.0, 10.0, 1.0 };
    glLightfv(GL_LIGHT0, GL_POSITION, light_pos);
    
    glClearColor(0.2f, 0.2f, 0.2f, 1.0f); // Dark Gray Background
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    // Camera: Eye(x,y,z), Target(0,0,0), Up(0,0,1) -> Z is Up
    gluLookAt(camX, camY, camZ,  0, 0, 0,  0, 0, 1);

    // Draw Floor Grid (Optional helper)
    glDisable(GL_LIGHTING);
    glBegin(GL_LINES);
    glColor3f(0.5, 0.5, 0.5);
    for(int i=-10; i<=10; i++) {
        glVertex3f(i, -10, 0); glVertex3f(i, 10, 0);
        glVertex3f(-10, i, 0); glVertex3f(10, i, 0);
    }
    glEnd();
    glEnable(GL_LIGHTING);

    // Draw Scene
    for (Node* n : sceneGraph) {
        n->DrawLegacy();
    }

    glutSwapBuffers();
}

void reshape(int w, int h) {
    if (h == 0) h = 1;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, (float)w / h, 0.1, 100.0);
    glMatrixMode(GL_MODELVIEW);
}

void keyboard(unsigned char key, int x, int y) {
    float speed = 0.1f;
    float rotSpeed = 2.0f;

    switch(key) {
        case 27: exit(0); break; // ESC
        
        // Tab to switch selection
        case 9: // TAB key code
            if(selectedNode) selectedNode->isSelected = false;
            selectionIndex++;
            if(selectionIndex >= flatList.size()) selectionIndex = 0;
            selectedNode = flatList[selectionIndex];
            selectedNode->isSelected = true;
            std::cout << "Selected: " << selectedNode->name << std::endl;
            break;

        case '/': clockRunning = !clockRunning; break;
        case '[': if(selectedNode) selectedNode->animSpeed -= 0.1f; break;
        case ']': if(selectedNode) selectedNode->animSpeed += 0.1f; break;

        // Transformation (QWERTY)
        case 'q': if(selectedNode) selectedNode->x += speed; break;
        case 'a': if(selectedNode) selectedNode->x -= speed; break;
        case 'w': if(selectedNode) selectedNode->y += speed; break;
        case 's': if(selectedNode) selectedNode->y -= speed; break;
        case 'e': if(selectedNode) selectedNode->z += speed; break;
        case 'd': if(selectedNode) selectedNode->z -= speed; break;

        case 'r': if(selectedNode) selectedNode->rx += rotSpeed; break;
        case 'f': if(selectedNode) selectedNode->rx -= rotSpeed; break;
        case 't': if(selectedNode) selectedNode->ry += rotSpeed; break;
        case 'g': if(selectedNode) selectedNode->ry -= rotSpeed; break;
        case 'y': if(selectedNode) selectedNode->rz += rotSpeed; break;
        case 'h': if(selectedNode) selectedNode->rz -= rotSpeed; break;
    }
    glutPostRedisplay();
}

void specialKeys(int key, int x, int y) {
    float camSpeed = 0.5f;
    switch(key) {
        case GLUT_KEY_UP:    camY += camSpeed; break;
        case GLUT_KEY_DOWN:  camY -= camSpeed; break;
        case GLUT_KEY_LEFT:  camX -= camSpeed; break;
        case GLUT_KEY_RIGHT: camX += camSpeed; break;
        case GLUT_KEY_PAGE_UP: camZ += camSpeed; break;
        case GLUT_KEY_PAGE_DOWN: camZ -= camSpeed; break;
    }
    glutPostRedisplay();
}

void idle() {
    if (clockRunning) {
        for(auto n : flatList) {
            if(n->isAnimated) n->rz += n->animSpeed;
        }
        glutPostRedisplay();
    }
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(1024, 768);
    glutCreateWindow("GLUT Room Editor");

    init();
    LoadScene("scene.json");

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(specialKeys); // For Arrow Keys
    glutIdleFunc(idle);

    std::cout << "Controls:\nTAB: Select Object\nArrows: Move Camera\nQWE/ASD: Move Object\nRTY/FGH: Rotate Object\n";
    
    glutMainLoop();
    return 0;
}
