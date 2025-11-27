#include <GL/glut.h>
#include <iostream>
#include <vector>
#include <string>
#include <cmath>

// LIBRARIES
#include <nlohmann/json.hpp>
using json = nlohmann::json;

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// ==========================================
// 1. STRUCTURES
// ==========================================

struct Model {
    std::string name;
    std::vector<float> vertices;    // x, y, z
    std::vector<float> normals;     // nx, ny, nz
    std::vector<float> texcoords;   // u, v
    GLuint textureID = 0;           // OpenGL Texture ID
    bool loaded = false;
};

struct Object {
    std::string name;
    // Transform
    float x = 0, y = 0, z = 0;
    float rx = 0, ry = 0, rz = 0;
    float sx = 1, sy = 1, sz = 1;

    // Animation
    bool spinAnimation = false;     // Does this object spin?
    float spinSpeed = 0.0f;

    Model* model = nullptr;
};

// ==========================================
// 2. GLOBALS
// ==========================================
std::vector<Object*> sceneObjects;  // Flat list
std::vector<Model*> loadedModels;   // Resource cache

Object* selectedObject = nullptr;
int selectionIndex = 0;

// Camera (Orbit)
float cameraAngle = 0.0f;   // Rotation around the room
float cameraHeight = 3.0f;  // Height (Lifted up slightly)
float cameraDist = 8.0f;    // Distance (Close enough to see inside)

bool isAnimating = true;

// ==========================================
// 3. TEXTURE LOADING
// ==========================================
GLuint LoadTextureFromFile(const char* filename) {
    std::string fullPath = "models/" + std::string(filename);
    
    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char *data = stbi_load(fullPath.c_str(), &width, &height, &nrChannels, 0);
    
    if (!data) {
        std::cout << "Failed to load texture: " << fullPath << " (using white fallback)" << std::endl;
        return 0;
    }

    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    
    stbi_image_free(data);
    return textureID;
}

// ==========================================
// 4. MODEL LOADING
// ==========================================
Model* GetModel(std::string filename) {
    for (auto* m : loadedModels) {
        if (m->name == filename) return m;
    }

    std::cout << "Loading Model: " << filename << "... ";
    Model* m = new Model();
    m->name = filename;

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    std::string fullPath = "models/" + filename;
    
    // Using standard loader (trusting your texture paths)
    bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, fullPath.c_str(), "models/");

    if (!warn.empty()) std::cout << "WARN: " << warn << std::endl;
    if (!ret) {
        std::cout << "FAILED! " << err << std::endl;
        delete m;
        return nullptr;
    }

    if (!materials.empty() && !materials[0].diffuse_texname.empty()) {
        std::string texName = materials[0].diffuse_texname;
        std::cout << "[Texture: " << texName << "] ";
        m->textureID = LoadTextureFromFile(texName.c_str());
    }

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

            if (index.texcoord_index >= 0) {
                m->texcoords.push_back(attrib.texcoords[2 * index.texcoord_index + 0]);
                m->texcoords.push_back(attrib.texcoords[2 * index.texcoord_index + 1]);
            }
        }
    }

    m->loaded = true;
    std::cout << "Done. (" << m->vertices.size()/3 << " tris)" << std::endl;
    loadedModels.push_back(m);
    return m;
}

// ==========================================
// 5. SCENE SETUP
// ==========================================
void AddObj(std::string name, std::string modelName, 
            float x, float y, float z, 
            float rx, float ry, float rz, 
            float sx, float sy, float sz,
            bool isAnimated = false) 
{
    Object* obj = new Object();
    obj->name = name;
    obj->model = GetModel(modelName); 
    
    obj->x = x; obj->y = y; obj->z = z;
    obj->rx = rx; obj->ry = ry; obj->rz = rz;
    obj->sx = sx; obj->sy = sy; obj->sz = sz;

    if (isAnimated) {
        obj->spinAnimation = true;
        obj->spinSpeed = 1.0f;
    }

    sceneObjects.push_back(obj);
}

void LoadScene() {
    // Using your exact coordinates
    AddObj("big_sofa",    "big_sofa.obj",    -1.854, 0.030, 0.198,     90.0, 0.0, 90.0,        1.0, 1.0, 1.0,      false);
    AddObj("bookshelf",   "bookshelf.obj",   -2.053, -1.771, 0.030,    90.0, 0.0, 90.0,        0.013, 0.013, 0.013, false);
    AddObj("cactus",      "cactus.obj",      -0.155, -0.131, 0.503,    -2.361, 0.209, -90.0,   0.006, 0.006, 0.006, false);
    AddObj("carpet",      "carpet.obj",      -0.039, 0.244, 0.046,     0.0, 0.0, 0.0,          1.193, 1.183, 1.0,  false);
    AddObj("clock",       "clock.obj",       -2.262, -1.811, 2.082,    90.0, 0.0, 0.0,         0.591, 0.591, 0.591, true);
    AddObj("lamp",        "lamp.obj",        -1.829, 1.863, 0.088,     0.0, 0.0, 0.0,          0.179, 0.179, 0.026, false);
    AddObj("shelf",       "shelf.obj",       -2.181, 0.072, 1.499,     90.0, 0.0, 0.0,         0.002, 0.002, 0.002, false);
    AddObj("sofa",        "sofa.obj",        -0.077, 1.839, 0.336,     0.0, 0.0, 0.0,          0.022, 0.024, 0.029, false);
    AddObj("table",       "table.obj",       -0.285, -0.104, 0.048,    0.0, 0.0, 0.0,          1.328, 1.334, 1.0,  false);
    AddObj("tv",          "tv.obj",          2.026, 0.132, 0.720,      0.0, 0.0, 180.0,        0.032, 0.032, 0.032, false);
    AddObj("walls",       "walls.obj",       -0.178, 2.213, 1.590,     -90.0, 90.0, 0.0,       0.383, 0.583, 1.0,  false);

    if(!sceneObjects.empty()) {
        selectedObject = sceneObjects[0]; 
    }
}

// ==========================================
// 6. GLUT FUNCTIONS
// ==========================================
void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    // ORBIT CAMERA MATH (Z-Up Compatible)
    float camX = cameraDist * sin(cameraAngle);
    float camY = cameraDist * cos(cameraAngle); 
    
    // Look from (X, Y, Height) -> to (0,0,0) -> Z is UP
    gluLookAt(camX, camY, cameraHeight,  0, 0, 0,  0, 0, 1);

    // DEBUG: DRAW RED CUBE AT ORIGIN
    // If you see this, your display works!
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glPushMatrix();
    glColor3f(1.0, 0.0, 0.0); // Red
    glutWireCube(0.5);        // Small Cube
    glPopMatrix();
    glEnable(GL_LIGHTING);

    // Draw Objects
    for (Object* obj : sceneObjects) {
        if (!obj->model || !obj->model->loaded) continue;

        glPushMatrix();
        
        // Transforms
        glTranslatef(obj->x, obj->y, obj->z);
        glRotatef(obj->rx, 1, 0, 0);
        glRotatef(obj->ry, 0, 1, 0);
        glRotatef(obj->rz, 0, 0, 1);
        glScalef(obj->sx, obj->sy, obj->sz);

        // Selection Highlight
        if (obj == selectedObject) {
            float pulse = (sin(glutGet(GLUT_ELAPSED_TIME) * 0.005f) + 1.0f) * 0.2f + 0.8f;
            glColor3f(pulse, pulse, 0.5f); 
        } else {
            glColor3f(1, 1, 1);
        }

        // Texture Binding
        if (obj->model->textureID != 0) {
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, obj->model->textureID);
        } else {
            glDisable(GL_TEXTURE_2D);
        }

        // Draw Mesh
        glBegin(GL_TRIANGLES);
        int numVerts = obj->model->vertices.size() / 3;
        for (int i = 0; i < numVerts; i++) {
            // Normal
            if (!obj->model->normals.empty())
                glNormal3f(obj->model->normals[3*i+0], obj->model->normals[3*i+1], obj->model->normals[3*i+2]);
            
            // Texture Coord
            if (!obj->model->texcoords.empty())
                glTexCoord2f(obj->model->texcoords[2*i+0], obj->model->texcoords[2*i+1]);

            // Vertex
            glVertex3f(obj->model->vertices[3*i+0], obj->model->vertices[3*i+1], obj->model->vertices[3*i+2]);
        }
        glEnd();

        glPopMatrix();
    }

    glutSwapBuffers();
}

void keyboard(unsigned char key, int x, int y) {
    if (!selectedObject) return;
    float speed = 0.2f;
    float rSpeed = 5.0f;

    switch(key) {
        case 27: exit(0); break; // ESC
        case 9: // TAB - Cycle Selection
            selectionIndex = (selectionIndex + 1) % sceneObjects.size();
            selectedObject = sceneObjects[selectionIndex];
            std::cout << "Selected: " << selectedObject->name << std::endl;
            break;
        
        case ' ': isAnimating = !isAnimating; break; // Pause Animation

        // Position
        case 'w': selectedObject->y += speed; break;
        case 's': selectedObject->y -= speed; break;
        case 'a': selectedObject->x -= speed; break;
        case 'd': selectedObject->x += speed; break;
        case 'q': selectedObject->z += speed; break;
        case 'e': selectedObject->z -= speed; break;

        // Rotation
        case 'r': selectedObject->rx += rSpeed; break;
        case 'f': selectedObject->rx -= rSpeed; break;
        case 't': selectedObject->ry += rSpeed; break;
        case 'g': selectedObject->ry -= rSpeed; break;
        case 'y': selectedObject->rz += rSpeed; break;
        case 'h': selectedObject->rz -= rSpeed; break;
        
        // Scale
        case 'u': selectedObject->sx += 0.001; selectedObject->sy += 0.001; selectedObject->sz += 0.001; break;
        case 'j': selectedObject->sx -= 0.001; selectedObject->sy -= 0.001; selectedObject->sz -= 0.001; break;
    }
    glutPostRedisplay();
}

void specialKeys(int key, int x, int y) {
    // Camera Orbit Controls
    switch(key) {
        case GLUT_KEY_LEFT:  cameraAngle -= 0.1f; break;
        case GLUT_KEY_RIGHT: cameraAngle += 0.1f; break;
        case GLUT_KEY_UP:    cameraDist -= 0.5f; break; // Zoom In
        case GLUT_KEY_DOWN:  cameraDist += 0.5f; break; // Zoom Out
    }
    glutPostRedisplay();
}

void idle() {
    if (isAnimating) {
        for (Object* obj : sceneObjects) {
            if (obj->spinAnimation) {
                obj->rz += obj->spinSpeed;
            }
        }
        glutPostRedisplay();
    }
}

void init() {
    glEnable(GL_DEPTH_TEST);
    
    // LIGHTING SETUP
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL); 
    
    // IMPORTANT: Make sure we see inside of rooms
    glDisable(GL_CULL_FACE); 
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);

    GLfloat light_pos[] = { 5.0, 5.0, 10.0, 1.0 };
    glLightfv(GL_LIGHT0, GL_POSITION, light_pos);
    
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
}

void reshape(int w, int h) {
    if (h == 0) h = 1;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, (float)w / h, 0.1, 100.0);
    glMatrixMode(GL_MODELVIEW);
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(1024, 768);
    glutCreateWindow("Final Room Project");

    init();
    LoadScene();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape); // Added reshape callback
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(specialKeys);
    glutIdleFunc(idle);
    
    std::cout << "CONTROLS:\nArrows: Orbit Camera\nTAB: Select Object\nWASD/QE: Move Object\nRF/TG/YH: Rotate Object\nSpace: Pause Animation\n";

    glutMainLoop();
    return 0;
}
