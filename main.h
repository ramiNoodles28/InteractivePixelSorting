#pragma once
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#include <iostream>
#include <filesystem>
#include <cassert>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "gl_core_3_3.h"
#include <GL/freeglut.h>
#include <random>
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glut.h"
#include "imgui/imgui_impl_opengl3.h"

#include "util.hpp"
#include "mesh.hpp"


using namespace std;
using namespace glm;
namespace fs = filesystem;

// Global state
GLint width, height;
GLuint shader;			// Shader program
GLuint uniXform;		// Shader location of xform mtx
GLuint vao;				// Vertex array object
GLuint vbuf;			// Vertex buffer
GLsizei vcount;			// Number of vertices
Mesh* mesh;				// Mesh loaded from .obj file
GLuint photoTexID;
bool isMeshMode;

// Camera state
vec3 camCoords;			// Spherical coordinates (theta, phi, radius) of the camera
bool camRot;			// Whether the camera is currently rotating
vec2 camOrigin;			// Original camera coordinates upon clicking
vec2 mouseOrigin;		// Original mouse coordinates upon clicking
vec2 objPos;			// Object's screen position
vec2 objVel;			// Object's screen velocity
float maxSpeed;			// Max speed of object

vec3 camPan = vec3(0.0f, 0.0f, 0.0f); // Current translation offset
bool isPanning = false;               // Is middle click held?
vec3 camPanOrigin;                    // Pan value when click started

vec3 lightPos = vec3(5.0f, 5.0f, 5.0f); // Initial position
bool rightMouseDown = false;
int lastMouseX, lastMouseY;

// pixel sorting things
GLuint overlayShader;
GLuint screenTexID;
GLuint quadVAO, quadVBO;
vector<uint8_t> pixelData;
float minThresh;
float maxThresh;
bool flipSortDir;
bool isVertSort;
enum sortType { HUE, SAT, LUM, 
				RED, GREEN, BLUE };
const char* sortItems[] = { "Hue", "Saturation", "Luminance", 
							"Red", "Green", "Blue" };
sortType sType;
bool useMask;
int randOff;
int noiseAmount;
int maxSpanLength;

// Model Selection
int currModelIdx = 0;
vector<string> modelFiles;
vector<const char*> modelLabels;
struct Rect { float minX, minY, maxX, maxY; };

vector<string> photoFiles;
vector<const char*> photoLabels;
int currPhotoIdx = 0;
int photoW, photoH;

vector<string> skyboxFolders;
vector<const char*> skyboxLabels;
int currSkyboxIdx = 0;

// Skybox
GLuint skyboxShader;
GLint projLoc, viewLoc;
GLuint cubemapTexture;
GLuint skyboxVAO, skyboxVBO;

// Initialization functions
void initState();
void initGLUT(int* argc, char** argv);
void initOpenGL();
void updateAssetList(const string& folderPath, vector<string>& files, vector<const char*>& labels);

void MeshMode();
void PhotoMode();
void createIMGuiWindow();

// Callback functions
void display();
void reshape(GLint width, GLint height);
void keyRelease(unsigned char key, int x, int y);
void mouseBtn(int button, int state, int x, int y);
void mouseWheel(int wheel, int direction, int x, int y);
void mouseMove(int x, int y);
void idle();
void cleanup();
GLuint loadTexture(const char* path);
GLuint loadCubemap(vector<string> faces);
void setupSkybox();
void updateSkybox();

// pixel sorting functions
void sortPixelsHorizontal();
void sortPixelsVertical();
float getHue(uint32_t c);
float getSaturation(uint32_t c);
float getLuminance(uint32_t c);
bool isColorInThresh(uint32_t c);
bool compareColors(uint32_t a, uint32_t b);