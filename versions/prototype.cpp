#include <GL/glut.h>
#include <vector>
#include <cmath>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>
#include <stdio.h>
#include <stdlib.h>

struct Vertex {
    float x, y, z;
};

struct Face {
    int v1, v2, v3;
};

std::vector<Vertex> vertices;
std::vector<Face> faces;

float translateX = 0.0f;
float translateY = 0.0f;
float translateZ = 0.0f;

float rotateDegree = 1.0f;
float rotateX = 0.0f;
float rotateY = 0.0f;
float rotateZ = 0.0f;

float scaleFactor = 0.1f;
float scaleX = 1.0f;
float scaleY = 1.0f;
float scaleZ = 1.0f;

bool loadOFFFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Cannot open file: " << filename << std::endl;
        return false;
    }
    
    std::string line;
    
    // Read and validate OFF header
    std::getline(file, line);
    if (line != "OFF" && line != "off") {
        std::cerr << "Not a valid OFF file" << std::endl;
        return false;
    }
    
    // Skip empty lines and comments
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        
        // Remove comments from the line
        size_t commentPos = line.find('#');
        if (commentPos != std::string::npos) {
            line = line.substr(0, commentPos);
        }
        
        if (!line.empty()) break;
    }
    
    // Parse counts line
    std::stringstream header(line);
    int numVertices, numFaces, numEdges;
    header >> numVertices >> numFaces >> numEdges;
    
    if (numVertices <= 0 || numFaces <= 0) {
        std::cerr << "Invalid vertex or face count" << std::endl;
        return false;
    }
    
    std::cout << "Loading OFF file: " << numVertices << " vertices, " << numFaces << " faces" << std::endl;
    
    // Read vertices
    vertices.resize(numVertices);
    int verticesRead = 0;
    while (verticesRead < numVertices && std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        
        // Remove comments
        size_t commentPos = line.find('#');
        if (commentPos != std::string::npos) {
            line = line.substr(0, commentPos);
        }
        
        std::stringstream ss(line);
        if (ss >> vertices[verticesRead].x >> vertices[verticesRead].y >> vertices[verticesRead].z) {
            verticesRead++;
        }
    }
    
    if (verticesRead != numVertices) {
        std::cerr << "Failed to read all vertices. Expected: " << numVertices << ", Read: " << verticesRead << std::endl;
        return false;
    }
    
    // Read faces
    faces.resize(numFaces);
    int facesRead = 0;
    while (facesRead < numFaces && std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        
        // Remove comments
        size_t commentPos = line.find('#');
        if (commentPos != std::string::npos) {
            line = line.substr(0, commentPos);
        }
        
        std::stringstream ss(line);
        int numVerticesInFace;
        if (ss >> numVerticesInFace) {
            if (numVerticesInFace == 3) {
                if (ss >> faces[facesRead].v1 >> faces[facesRead].v2 >> faces[facesRead].v3) {
                    facesRead++;
                }
            } else if (numVerticesInFace == 4) {
                // Convert quad to two triangles
                int v1, v2, v3, v4;
                if (ss >> v1 >> v2 >> v3 >> v4) {
                    faces[facesRead++] = {v1, v2, v3};
                    if (facesRead < numFaces) {
                        faces[facesRead++] = {v1, v3, v4};
                    }
                }
            } else {
                std::cerr << "Unsupported face with " << numVerticesInFace << " vertices" << std::endl;
                // Skip this face
                continue;
            }
        }
    }
    
    // Resize faces in case we read more due to quad triangulation
    if (facesRead != numFaces) {
        faces.resize(facesRead);
        std::cout << "Adjusted face count to: " << facesRead << " after triangulation" << std::endl;
    }
    
    file.close();
    std::cout << "Successfully loaded " << vertices.size() << " vertices and " << faces.size() << " faces" << std::endl;
    return true;
}

void normalizeVertices() {
    if (vertices.empty()) return;
    
    // Find min and max coordinates
    float minX = vertices[0].x, maxX = vertices[0].x;
    float minY = vertices[0].y, maxY = vertices[0].y;
    float minZ = vertices[0].z, maxZ = vertices[0].z;
    
    for (const auto& v : vertices) {
        minX = std::min(minX, v.x);
        maxX = std::max(maxX, v.x);
        minY = std::min(minY, v.y);
        maxY = std::max(maxY, v.y);
        minZ = std::min(minZ, v.z);
        maxZ = std::max(maxZ, v.z);
    }
    
    std::cout << "Original bounds - X: [" << minX << ", " << maxX << "], "
              << "Y: [" << minY << ", " << maxY << "], "
              << "Z: [" << minZ << ", " << maxZ << "]" << std::endl;
    
    // Calculate center and scale
    float centerX = (minX + maxX) / 2.0f;
    float centerY = (minY + maxY) / 2.0f;
    float centerZ = (minZ + maxZ) / 2.0f;
    
    float scaleX = maxX - minX;
    float scaleY = maxY - minY;
    float scaleZ = maxZ - minZ;
    float maxScale = std::max(scaleX, std::max(scaleY, scaleZ));
    
    if (maxScale == 0.0f) maxScale = 1.0f; // Prevent division by zero
    
    // Normalize vertices to fit in [-1, 1] range
    for (auto& v : vertices) {
        v.x = (v.x - centerX) / (maxScale * 0.5f);
        v.y = (v.y - centerY) / (maxScale * 0.5f);
        v.z = (v.z - centerZ) / (maxScale * 0.5f);
    }
    
    std::cout << "Vertices normalized to fit in [-1, 1] range" << std::endl;
}

void calculateFaceNormal(const Vertex& v1, const Vertex& v2, const Vertex& v3, float& nx, float& ny, float& nz) {
    float ux = v2.x - v1.x;
    float uy = v2.y - v1.y;
    float uz = v2.z - v1.z;
    
    float vx = v3.x - v1.x;
    float vy = v3.y - v1.y;
    float vz = v3.z - v1.z;
    
    nx = uy * vz - uz * vy;
    ny = uz * vx - ux * vz;
    nz = ux * vy - uy * vx;
    
    float length = sqrt(nx * nx + ny * ny + nz * nz);
    if (length > 0.0001f) {
        nx /= length;
        ny /= length;
        nz /= length;
    }
}

void init() {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);
    
    // Set up lighting
    GLfloat lightPos[] = {5.0f, 5.0f, 5.0f, 1.0f};
    GLfloat lightAmbient[] = {0.3f, 0.3f, 0.3f, 1.0f};
    GLfloat lightDiffuse[] = {0.9f, 0.9f, 0.9f, 1.0f};
    GLfloat lightSpecular[] = {1.0f, 1.0f, 1.0f, 1.0f};
    
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, lightSpecular);
    
    // Set material properties
    GLfloat materialSpecular[] = {1.0f, 1.0f, 1.0f, 1.0f};
    GLfloat materialShininess[] = {50.0f};
    
    glMaterialfv(GL_FRONT, GL_SPECULAR, materialSpecular);
    glMaterialfv(GL_FRONT, GL_SHININESS, materialShininess);
    
    // Set background color
    glClearColor(0.2f, 0.3f, 0.4f, 1.0f);
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    
    // Set camera position
    gluLookAt(5.0, 5.0, 5.0,  // Eye position
              0.0, 0.0, 0.0,  // Look at point
              0.0, 0.0, 1.0); // Up vector
    
    // Apply translation
    glTranslatef(translateX, translateY, translateZ);
    glRotatef(rotateX, 1.0, 0.0, 0.0);
    glRotatef(rotateY, 0.0, 1.0, 0.0);
    glRotatef(rotateZ, 0.0, 0.0, 1.0);
    glScalef(scaleX, scaleY, scaleZ);
    
    // Draw the mesh with different colors for different parts
    glColor3f(0.0f, 1.0f, 1.0f); // Color
    
    glBegin(GL_TRIANGLES);
    for (const auto& face : faces) {
        if (face.v1 >= vertices.size() || face.v2 >= vertices.size() || face.v3 >= vertices.size()) {
            continue; // Skip invalid faces
        }
        
        const Vertex& v1 = vertices[face.v1];
        const Vertex& v2 = vertices[face.v2];
        const Vertex& v3 = vertices[face.v3];
        
        // Calculate face normal
        float nx, ny, nz;
        calculateFaceNormal(v1, v2, v3, nx, ny, nz);
        glNormal3f(nx, ny, nz);
        
        glVertex3f(v1.x, v1.y, v1.z);
        glVertex3f(v2.x, v2.y, v2.z);
        glVertex3f(v3.x, v3.y, v3.z);
    }
    glEnd();
    
    std::cout << translateX << " " << translateY << " " << translateZ << " " << rotateX << " " << rotateY << " " << rotateZ << " " << scaleX << " " << scaleY << " " << scaleZ << std::endl;
    glutSwapBuffers();
}

void reshape(int w, int h) {
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, (double)w / (double)h, 0.1, 100.0);
    glMatrixMode(GL_MODELVIEW);
}

void keyboard(unsigned char key, int x, int y) {
    switch (key) {
        case 27: // ESC key
            exit(0);
            break;
        case 'w': case 'W': translateY += 0.1f; break; // Move up
        case 's': case 'S': translateY -= 0.1f; break; // Move down
        case 'a': case 'A': translateX -= 0.1f; break; // Move left
        case 'd': case 'D': translateX += 0.1f; break; // Move right
        case 'q': case 'Q': translateZ -= 0.1f; break; // Move forward
        case 'e': case 'E': translateZ += 0.1f; break; // Move backward
        case 'r': case 'R': translateX = translateY = translateZ = rotateX = rotateY = rotateZ = 0.0f; // Reset position
        case 'j': case 'J': rotateX -= rotateDegree; break; // RotateX down
        case 'u': case 'U': rotateX += rotateDegree; break; // RotateX up
        case 'k': case 'K': rotateY -= rotateDegree; break; // RotateY down
        case 'i': case 'I': rotateY += rotateDegree; break; // RotateY up
        case 'l': case 'L': rotateZ -= rotateDegree; break; // RotateZ down
        case 'o': case 'O': rotateZ += rotateDegree; break; // RotateZ up
        case 't': case 'T': scaleX -= scaleFactor; break; // scaleX down
        case 'y': case 'Y': scaleX += scaleFactor; break; // scaleX up
        case 'g': case 'G': scaleY -= scaleFactor; break; // scaleY down
        case 'h': case 'H': scaleY += scaleFactor; break; // scaleY up
        case 'b': case 'B': scaleZ -= scaleFactor; break; // scaleZ down
        case 'n': case 'N': scaleZ += scaleFactor; break; // RotateZ up
            break;
    }
    glutPostRedisplay();
}


void idle() {
    glutPostRedisplay();
}

void printInstructions() {
    std::cout << "\n=== OFF Mesh Viewer ===" << std::endl;
    std::cout << "Controls:" << std::endl;
    printf("W/S: Move Up/Down\n");
    printf("A/D: Move Left/Right\n");
    printf("Q/E: Move Forward/Backward\n");
    printf("R: Reset Position\n");
    std::cout << "- ESC: Exit program" << std::endl;
    std::cout << "- Mesh automatically rotates for viewing" << std::endl;
    std::cout << "========================\n" << std::endl;
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(800, 600);
    glutCreateWindow("Assigment");
    
    printInstructions();
    
    // Load the OFF file
    std::string filename = "table_0394.off";
    if (argc > 1) {
        filename = argv[1];
    }
    
    if (!loadOFFFile(filename)) {
        std::cerr << "Failed to load OFF file: " << filename << std::endl;
        std::cerr << "Please ensure the file exists and is in the correct format." << std::endl;
        return -1;
    }
    
    // Normalize vertices
    normalizeVertices();
    
    // Initialize OpenGL
    init();
    
    // Set up callbacks
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutIdleFunc(idle);
    
    std::cout << "Rendering mesh... Use ESC to exit." << std::endl;
    glutMainLoop();
    return 0;
}