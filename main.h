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
GLuint shader;							// Shader program
GLuint uniXform;						// Shader location of xform mtx
GLuint vao;								// Vertex array object
GLuint vbuf;							// Vertex buffer
GLsizei vcount;							// Number of vertices
Mesh* mesh;								// Mesh loaded from .obj file
GLuint photoTexID;
bool isMeshMode;

// Camera state
vec3 camCoords;							// Spherical coordinates (theta, phi, radius) of the camera
bool camRot;							// Whether the camera is currently rotating
vec2 camOrigin;							// Original camera coordinates upon clicking
vec2 mouseOrigin;						// Original mouse coordinates upon clicking
vec2 objPos;							// Object's screen position

vec3 camPan = vec3(0.0f, 0.0f, 0.0f);	// Current translation offset
bool isPanning = false;					// Is middle click held?
vec3 camPanOrigin;						// Pan value when click started

vec3 lightPos = vec3(5.0f, 5.0f, 5.0f); // light initial position
float lightAngle = 0.0f;				// light horizontal rotation
float lightHeight = 5.0f;				// light vertical position
float lightDistance = 7.0f;				// how far the light is from the center
bool rightMouseDown = false;			// moving the light source?
int lastMouseX, lastMouseY;				// for light pos calc

// pixel sorting things
GLuint overlayShader;					// screen space shader
GLuint screenTexID;						// tex id of image
GLuint quadVAO, quadVBO;				// quad object for image
vector<uint8_t> pixelData;				// pixels of image to be sorted
float minThresh;						// min luminance val to be sorted
float maxThresh;						// max luminance val to be sorted
bool flipSortDir;						// flips sorting direction on axis
bool isVertSort;						// are we sorting vertically (slower)
enum sortType { HUE, SAT, LUM,
				RED, GREEN, BLUE };		// what are we sorting by?
const char* sortItems[] = { "Hue", "Saturation", "Luminance", 
							"Red", "Green", "Blue" };
sortType sType;
bool useMask;							// are we using the luminance mask for sorting?
bool spanjitter;						// do we want spans to change every frame?
int randOff;							// spans' random offset
int noiseAmount;						// amount of variance between span length
int maxSpanLength;						// max length of any span

// Asset Selection
int currModelIdx = 0;					// current model index
vector<string> modelFiles;				// all model file paths
vector<const char*> modelLabels;		// model labels for UI

vector<string> photoFiles;				// all photo file paths
vector<const char*> photoLabels;		// photo labels for UI
int currPhotoIdx = 0;					// current photo index
int photoW, photoH;						// photo width and height

vector<string> skyboxFolders;			// all skybox folder paths
vector<const char*> skyboxLabels;		// skybox labels for UI
int currSkyboxIdx = 0;					// current skybox index

// Skybox
GLuint skyboxShader;					// skybox shader
GLint projLoc, viewLoc;					// projection and view locations
GLuint cubemapTexture;					// skybox texture
GLuint skyboxVAO, skyboxVBO;			// skybox objects

// Initialization functions
void initState();
void initGLUT(int* argc, char** argv);
void initOpenGL();

// Rendering functions
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

// Asset management functions
void updateAssetList(const string& folderPath,
	vector<string>& files, vector<const char*>& labels);
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