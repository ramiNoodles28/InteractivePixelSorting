#include "main.h"

int main(int argc, char** argv) {
	try {
		// Initialize
		initState();
		initGLUT(&argc, argv);
		initOpenGL();

	} catch (const exception& e) {
		// Handle any errors
		cerr << "Fatal error: " << e.what() << endl;
		cleanup();
		return -1;
	}

	// Execute main loop
	glutMainLoop();

	return 0;
}

//// initializing functions //////////////////////////////////////////////////
void initState() {
	// initialize global state
	width = 1600;
	height = 1000;
	shader = 0;
	uniXform = 0;
	vao = 0;
	vbuf = 0;
	vcount = 0;
	mesh = NULL;
	isMeshMode = true;
	photoTexID = 0;
	// pixel sorting stuff
	sType = LUM;
	minThresh = 50;
	maxThresh = 200;
	isVertSort = false;
	flipSortDir = false;
	srand(time(0));
	maxSpanLength = 16;
	noiseAmount = 5;

	// updating photo/model lists
	updateAssetList("photos", photoFiles, photoLabels);
	updateAssetList("models", modelFiles, modelLabels);
	updateAssetList("skybox", skyboxFolders, skyboxLabels);

	camCoords = vec3(0.0, 0.0, 1.0);
	camRot = false;
}

void initGLUT(int* argc, char** argv) {
	// Set window and context settings
	glutInit(argc, argv);
	glutInitWindowSize(width, height);
	glutInitContextVersion(3, 3);
	glutInitContextProfile(GLUT_CORE_PROFILE);
	glutInitDisplayMode(GLUT_RGBA | GLUT_DEPTH | GLUT_DOUBLE);
	// Create the window
	glutCreateWindow("Pixel Sorting");
	// GLUT callbacks
	glutDisplayFunc(display);
	glutReshapeFunc(reshape);
	glutKeyboardUpFunc(keyRelease);
	glutMouseFunc(mouseBtn);
	glutMouseWheelFunc(mouseWheel);
	glutMotionFunc(mouseMove);
	glutPassiveMotionFunc(mouseMove);
	glutIdleFunc(idle);
	glutCloseFunc(cleanup);
}

void initOpenGL() {
	// Set clear color and depth
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClearDepth(1.0f);
	glEnable(GL_DEPTH_TEST);

	// Compile and link shader program
	vector<GLuint> shaders;
	shaders.push_back(compileShader(GL_VERTEX_SHADER, "sh_v.glsl"));
	shaders.push_back(compileShader(GL_FRAGMENT_SHADER, "sh_f.glsl"));
	shader = linkProgram(shaders);
	// Release shader sources
	for (auto s : shaders) glDeleteShader(s);
	shaders.clear();
	// Locate uniforms
	uniXform = glGetUniformLocation(shader, "xform");

	// Compile and link overlay shader program
	shaders.push_back(compileShader(GL_VERTEX_SHADER, "sh_v_overlay.glsl"));
	shaders.push_back(compileShader(GL_FRAGMENT_SHADER, "sh_f_overlay.glsl"));
	overlayShader = linkProgram(shaders);
	for (auto s : shaders) glDeleteShader(s);
	shaders.clear();

	// Compile and link skybox shader program
	shaders.push_back(compileShader(GL_VERTEX_SHADER, "sh_v_skybox.glsl"));
	shaders.push_back(compileShader(GL_FRAGMENT_SHADER, "sh_f_skybox.glsl"));
	skyboxShader = linkProgram(shaders);
	for (auto s : shaders) glDeleteShader(s);
	shaders.clear();

	projLoc = glGetUniformLocation(skyboxShader, "projection");
	viewLoc = glGetUniformLocation(skyboxShader, "view");
	updateSkybox();
	setupSkybox();
	
	// set up screen space quad for pixel sorting
	float quadVertices[] = { //image plane that pixels are sorted on
		// positions (x, y)   // texCoords (u, v)
		-1.0f,  1.0f,         0.0f, 1.0f,
		-1.0f, -1.0f,         0.0f, 0.0f,
		 1.0f, -1.0f,         1.0f, 0.0f,

		-1.0f,  1.0f,         0.0f, 1.0f,
		 1.0f, -1.0f,         1.0f, 0.0f,
		 1.0f,  1.0f,         1.0f, 1.0f
	};
	glGenVertexArrays(1, &quadVAO);
	glGenBuffers(1, &quadVBO);
	glBindVertexArray(quadVAO);
	glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

	// texture to pixel sort
	glGenTextures(1, &screenTexID);
	glBindTexture(GL_TEXTURE_2D, screenTexID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glBindVertexArray(0);

	// setup ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;

	ImGui_ImplGLUT_Init();
	ImGui_ImplGLUT_InstallFuncs();
	ImGui_ImplOpenGL3_Init("#version 330");

	// dark mode
	ImGui::StyleColorsDark();

	assert(glGetError() == GL_NO_ERROR);
}

//// display functions /////////////////////////////////////////////////////
void display() {
	static bool firstFrame = true;
	if (firstFrame) {
		// re-take control of the mouse after ImGui has initialized in the main loop
		glutMouseFunc(mouseBtn);
		glutMotionFunc(mouseMove);

		glutMouseWheelFunc(mouseWheel);
		glutPassiveMotionFunc(mouseMove);
		firstFrame = false;
	}
	
	try {
	
		// Clear the back buffer
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		// Get ready to draw
		if (isMeshMode) MeshMode(); else PhotoMode();

		pixelData.resize(width * height * 4);
		glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixelData.data());
		/////// PIXEL SORTING!! ////////
		if (isVertSort) sortPixelsVertical(); else sortPixelsHorizontal();
		////////////////////////////////
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glUseProgram(overlayShader);

		// upload the modified pixel array to texture
		glBindTexture(GL_TEXTURE_2D, screenTexID);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixelData.data());

		// draw the fullscreen rectangle with our texture on it
		glBindVertexArray(quadVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glBindVertexArray(0);
		glDisable(GL_DEPTH_TEST);

		createIMGuiWindow();
		assert(glGetError() == GL_NO_ERROR);

		// revert context state
		glUseProgram(0);

		// Display the back buffer
		glutSwapBuffers();
		glutPostRedisplay();


	} catch (const exception& e) {
		cerr << "Fatal error: " << e.what() << endl;
		glutLeaveMainLoop();
	}
}

void MeshMode() {
	glEnable(GL_DEPTH_TEST);
	glUseProgram(shader);
	
	mat4 xform;
	float aspect = (float)width / (float)height;

	mat4 proj = perspective(45.0f, aspect, 0.1f, 100.0f); // Create perspective projection matrix
	// Create view transformation matrix
	mat4 view = translate(mat4(1.0f), vec3(0.0, 0.0, -camCoords.z)); // Move camera back by radius (z)
	view = translate(view, vec3(camPan.x, camPan.y, 0.0f)); // Apply the Pan (translation) here
	mat4 rot = rotate(mat4(1.0f), radians(camCoords.y), vec3(1.0, 0.0, 0.0)); // Apply Rotations
	rot = rotate(rot, radians(camCoords.x), vec3(0.0, 1.0, 0.0));
	mat4 move = translate(mat4(1.0f), vec3(objPos.x, objPos.y, 0.0f)); // Object movement (if any)

	glUniform3fv(glGetUniformLocation(shader, "lightPos"), 1, value_ptr(lightPos));
	glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, value_ptr(move));

	xform = proj * view * rot * move;

	//// load model
	if (!mesh) mesh = new Mesh(modelFiles[currModelIdx].c_str());
	// Scale and center mesh using bounding box
	pair<vec3, vec3> meshBB = mesh->boundingBox();
	mat4 fixBB = scale(mat4(1.0f), vec3(1.0f / length(meshBB.second - meshBB.first)));
	fixBB = glm::translate(fixBB, -(meshBB.first + meshBB.second) / 2.0f);
	// Concatenate all transformations and upload to shader
	xform = xform * fixBB;
	glUniformMatrix4fv(uniXform, 1, GL_FALSE, value_ptr(xform));
	// Draw the mesh
	mesh->draw();

	// setup skybox rendering
	glDepthFunc(GL_LEQUAL); 
	glUseProgram(skyboxShader);

	glUniformMatrix4fv(projLoc, 1, GL_FALSE, value_ptr(proj));
	glUniformMatrix4fv(viewLoc, 1, GL_FALSE, value_ptr(view*rot));

	glBindVertexArray(skyboxVAO);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);

	glDepthFunc(GL_LESS);
}

void PhotoMode() {
	glUseProgram(overlayShader);
	glDisable(GL_DEPTH_TEST); 
	if (photoTexID == 0) photoTexID = loadTexture("photos/arches.jpg");

	glUniformMatrix4fv(glGetUniformLocation(overlayShader, "xform"), 1, GL_FALSE, value_ptr(mat4(1.0f)));

	// bind the photo and draw the quad
	glBindTexture(GL_TEXTURE_2D, photoTexID);
	glBindVertexArray(quadVAO);
	glDrawArrays(GL_TRIANGLES, 0, 6);
}

//// interactive control functions /////////////////////////////////////////
void createIMGuiWindow() {
	// start the ImGui frame
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGLUT_NewFrame();
	ImGui::NewFrame();

	// create GUI window
	ImGui::Begin("Pixel Sort Settings");
	ImGui::Checkbox("Toggle Mesh/Photo Modes", &isMeshMode);

	if (isMeshMode) {
		if (ImGui::Combo("Model Select", &currModelIdx, modelLabels.data(), (int)modelLabels.size())) {
			if (mesh) {
				delete mesh;
				mesh = nullptr;
			}
		}
		if (ImGui::Combo("Select Skybox", &currSkyboxIdx, skyboxLabels.data(), (int)skyboxLabels.size())) {
			updateSkybox();
		}
	}
	else {
		if (ImGui::Combo("Select Photo", &currPhotoIdx, photoLabels.data(), (int)photoLabels.size())) {
			if (photoTexID != 0) {
				glDeleteTextures(1, &photoTexID); 
			}
			photoTexID = loadTexture(photoFiles[currPhotoIdx].c_str());
		}
		
	}
	if (ImGui::Button("Refresh Asset Lists")) {
		updateAssetList("photos", photoFiles, photoLabels);
		updateAssetList("models", modelFiles, modelLabels);
		updateAssetList("skybox", skyboxFolders, skyboxLabels);
	}
	
	static int currentSort = 2;
	if (ImGui::Combo("Sort Mode", &currentSort, sortItems, 6)) {
		sType = static_cast<sortType>(currentSort);
	}

	ImGui::SliderFloat("Min Threshold", &minThresh, 0.0f, 255.0f);
	ImGui::SliderFloat("Max Threshold", &maxThresh, 0.0f, 255.0f);

	ImGui::Checkbox("Use Mask for sorting", &useMask);
	if (!useMask) {
		ImGui::SliderInt("Max Span Length", &maxSpanLength, 2, 256);
		ImGui::SliderInt("Noise Amount", &noiseAmount, 2, 100);
	}
	ImGui::Checkbox("Jitter Toggle", &spanjitter);
	ImGui::Checkbox("Flip Sort Direction", &flipSortDir);
	ImGui::Checkbox("Sort Vertically", &isVertSort);

	ImGui::End();

	// rendering ImGui
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void reshape(GLint w, GLint h) {
	width = w;
	height = h;
	glViewport(0, 0, w, h);
	pixelData.resize(width * height * 4);
}

void keyRelease(unsigned char key, int x, int y) {
	switch (key) {
	case 27:	// Escape key
		glutLeaveMainLoop();;
		break;
	}
}

void mouseBtn(int button, int state, int x, int y) {
	ImGui_ImplGLUT_MouseFunc(button, state, x, y);
	if (ImGui::GetIO().WantCaptureMouse) return;
	
	if (state == GLUT_DOWN && button == GLUT_LEFT_BUTTON) {
		// Activate rotation mode
		camRot = true;
		camOrigin = vec2(camCoords);
		mouseOrigin = vec2(x, y);
	}
	if (state == GLUT_UP && button == GLUT_LEFT_BUTTON) {
		// Deactivate rotation
		camRot = false;
	}

	if (button == GLUT_RIGHT_BUTTON) {
		if (state == GLUT_DOWN) {
			// activate light movement mode
			rightMouseDown = true;
			lastMouseX = x;
			lastMouseY = y;
		}
		else {
			rightMouseDown = false;
			// deactivate light movement mode
		}
	}

	if (state == GLUT_DOWN && button == GLUT_MIDDLE_BUTTON) {
		// activate panning mode
		isPanning = true;
		camPanOrigin = camPan;
		mouseOrigin = vec2(x, y); 
	}
	if (state == GLUT_UP && button == GLUT_MIDDLE_BUTTON) {
		// deactivate panning mode
		isPanning = false;
	}
}

void mouseWheel(int wheel, int direction, int x, int y) {
	ImGuiIO& io = ImGui::GetIO();
	io.MouseWheel += (float)direction;
	if (io.WantCaptureMouse) return;
	float zoomSpeed = 0.5f;
	camCoords.z = std::clamp(camCoords.z - (direction * zoomSpeed), 0.1f, 50.0f);

	glutPostRedisplay();
}

void mouseMove(int x, int y) {
	ImGui_ImplGLUT_MotionFunc(x, y);
	if (ImGui::GetIO().WantCaptureMouse) return;

	if (camRot) {
		// Convert mouse delta into degrees, add to rotation
		float rotScale = glm::min(width / 450.0f, height / 270.0f);
		vec2 mouseDelta = vec2(x, y) - mouseOrigin;
		vec2 newAngle = camOrigin + mouseDelta / rotScale;
		newAngle.y = std::clamp(newAngle.y, -90.0f, 90.0f);
		while (newAngle.x > 180.0f) newAngle.x -= 360.0f;
		while (newAngle.y < -180.0f) newAngle.y += 360.0f;
		if (length(newAngle - vec2(camCoords)) > FLT_EPSILON) {
			camCoords.x = newAngle.x;
			camCoords.y = newAngle.y;
			glutPostRedisplay();
		}
	}
	if (rightMouseDown) {
		float dx = (x - lastMouseX) * 0.01f;
		float dy = (y - lastMouseY) * 0.05f;

		lightAngle += dx;
		lightHeight -= dy;

		lightPos.x = sin(lightAngle) * lightDistance;
		lightPos.z = cos(lightAngle) * lightDistance;
		lightPos.y = lightHeight;

		lastMouseX = x;
		lastMouseY = y;
		glutPostRedisplay();
	}
	if (isPanning) {
		vec2 mouseDelta = vec2(x, y) - mouseOrigin;
		float panScale = camCoords.z * 0.002f;

		camPan.x = camPanOrigin.x + mouseDelta.x * panScale;
		camPan.y = camPanOrigin.y - mouseDelta.y * panScale;

		glutPostRedisplay();
	}

}

void idle() {
	if (spanjitter)	randOff = (rand() % noiseAmount);
}

void cleanup() {
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGLUT_Shutdown();
	ImGui::DestroyContext();

	// Release all resources
	if (shader) { glDeleteProgram(shader); shader = 0; }
	uniXform = 0;
	if (vao) { glDeleteVertexArrays(1, &vao); vao = 0; }
	if (vbuf) { glDeleteBuffers(1, &vbuf); vbuf = 0; }
	vcount = 0;
	if (mesh) { delete mesh; mesh = NULL; }
}

//// pixel sorting functions //////////////////////////////////////////////
void sortPixelsHorizontal() {
	// sort spans by row
	for (int y = 0; y < height; y++) {
		uint32_t* row = (uint32_t*)&pixelData[y * width * 4];

		int x = 0;
		while (x < width) {
			// find the start of a span (pixel is bright enough)
			while (x < width && !isColorInThresh(row[x])) x++;
			int start = x;

			// find the end of that span
			// if using mask, go until pixel is outside of threshold
			if (useMask) while (x < width && isColorInThresh(row[x])) x++;
			// if not using mask, length is determined by computed span length
			else { x += (rand() % maxSpanLength) + randOff; x = x >= width ? width : x; }
			int end = x;

			// sort only that segment
			if (start < end) {
				sort(row + start, row + end, compareColors);
			}
		}
	}
}

void sortPixelsVertical() {
	// iterate through each column
	for (int x = 0; x < width; x++) {
		// extract the column into a temporary vector
		vector<uint32_t> column(height);
		for (int y = 0; y < height; y++) {
			column[y] = ((uint32_t*)pixelData.data())[y * width + x];
		}

		int y = 0;
		while (y < height) {
			// find the start of a span (pixel is bright enough)
			while (y < height && !isColorInThresh(column[y])) y++;
			int start = y;

			// find the end of that span
			// if using mask, go until pixel is outside of threshold
			if (useMask) while (y < height && isColorInThresh(column[y])) y++;
			// if not using mask, length is determined by computed span length
			else { y += (rand() % noiseAmount) + randOff; y = y >= height ? height : y; }
			int end = y;

			// sort only that segment
			if (start < end) {
				sort(column.begin() + start, column.begin() + end, compareColors);
			}
		}

		// write the sorted column back into the original pixelData
		for (int y = 0; y < height; y++) {
			((uint32_t*)pixelData.data())[y * width + x] = column[y];
		}
	}
}

bool isColorInThresh(uint32_t c) {
	float L = getLuminance(c);
	return (L >= minThresh && L <= maxThresh);
}

bool compareColors(uint32_t a, uint32_t b) {
	uint32_t A, B;
	switch (sType) {
	case RED: A = (a & 0xFF); B = (b & 0xFF); break;
	case GREEN: A = (a >> 8) & 0xFF; B = (b >> 8) & 0xFF; break;
	case BLUE: A = (a >> 16) & 0xFF; B = (b >> 16) & 0xFF; break;
	case HUE: return flipSortDir ? (getHue(a) < getHue(b)) : (getHue(a) > getHue(b));
	case SAT: return flipSortDir ? (getSaturation(a) < getSaturation(b)) : (getSaturation(a) > getSaturation(b));;
	case LUM: return flipSortDir ? (getLuminance(a) < getLuminance(b)) : (getLuminance(a) > getLuminance(b));;
	default: return a < b;
	}
	return flipSortDir ? (A < B) : (B < A);
}

float getLuminance(uint32_t c) {
	return	  0.2126f * (float)(c & 0xFF)
		+ 0.7152f * (float)((c >> 8) & 0xFF)
		+ 0.0722f * (float)((c >> 16) & 0xFF);
}

float getSaturation(uint32_t c) {
	float r = (float)(c & 0xFF);
	float g = (float)((c >> 8) & 0xFF);
	float b = (float)((c >> 16) & 0xFF);
	float invI = 3.0f / (r + g + b);
	float m = fminf(r, fminf(g, b));
	return 1.0f - m * invI;

}

float getHue(uint32_t c) {
	float r = (float)(c & 0xFF);
	float g = (float)((c >> 8) & 0xFF);
	float b = (float)((c >> 16) & 0xFF);
	float cmax = fmaxf(r, fmaxf(g, b));
	float cmin = fminf(r, fminf(g, b));
	float diff = cmax - cmin;
	if (diff <= 0.0f) return 0.0f;

	float H = 0;
	float invDiff = 1.0f / diff;

	if (cmax == r) { H = (g - b) * invDiff; if (H < 0.0f) H += 6.0f; }
	else if (cmax == g) H = (b - r) * invDiff + 2.0f;
	else if (cmax == b) H = (r - g) * invDiff + 4.0f;

	return H * 60.0f;
}


//// asset management functions //////////////////////////////////////////
void updateAssetList(const string& folderPath, vector<string>& files, 
								vector<const char*>& labels) {
	files.clear();
	labels.clear();

	if (!fs::exists(folderPath)) return;

	for (const auto& entry : fs::directory_iterator(folderPath)) {
		string ext = entry.path().extension().string();
		if (folderPath == "photos" && ext == ".jpg" || ext == ".jpeg" || ext == ".png") {
			files.push_back(entry.path().string());
		} 
		else if (folderPath == "models" && ext == ".obj") {
			files.push_back(entry.path().string());
		} 
		else if (folderPath == "skybox") {
			files.push_back(entry.path().string());
		}
	}

	// prepare labels for the ImGui dropdown
	for (const auto& s : files) {
		labels.push_back(s.c_str());
	}
}

GLuint loadTexture(const char* path) {
	int w, h, channels;
	stbi_set_flip_vertically_on_load(true);
	unsigned char* data = stbi_load(path, &w, &h, &channels, 4); // Force RGBA

	photoW = w; photoH = h;

	// trying to scale window to image sizes (not working D: )
	if (data) {
		//glutReshapeWindow(w/10, h/10);
	}

	GLuint tex;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);

	if (data) {
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);
	}
	else cout << "Failed to load texture: " << path << endl;

	stbi_image_free(data);
	return tex;
}

GLuint loadCubemap(vector<string> faces) {
	GLuint texID;
	glGenTextures(1, &texID);
	glBindTexture(GL_TEXTURE_CUBE_MAP, texID);

	int w, h, nrChannels;
	for (unsigned int i = 0; i < faces.size(); i++) {
		unsigned char* data = stbi_load(faces[i].c_str(), &w, &h, &nrChannels, 0);
		if (data) {
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
				0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
			stbi_image_free(data);
		}
		else {
			cout << "Cubemap face failed to load at path: " << faces[i] << endl;
		}
	}
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	return texID;
}

void setupSkybox() {
	glGenVertexArrays(1, &skyboxVAO);
	glGenBuffers(1, &skyboxVBO);

	float skyboxVertices[] = {
		// positions          
		-1.0f,  1.0f, -1.0f,
		-1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,

		-1.0f, -1.0f,  1.0f,
		-1.0f, -1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f,  1.0f,
		-1.0f, -1.0f,  1.0f,

		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,

		-1.0f, -1.0f,  1.0f,
		-1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f, -1.0f,  1.0f,
		-1.0f, -1.0f,  1.0f,

		-1.0f,  1.0f, -1.0f,
		 1.0f,  1.0f, -1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		-1.0f,  1.0f,  1.0f,
		-1.0f,  1.0f, -1.0f,

		-1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f,  1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f,  1.0f,
		 1.0f, -1.0f,  1.0f
	};

	glBindVertexArray(skyboxVAO);
	glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

	glBindVertexArray(0);
}

void updateSkybox() {
	if (cubemapTexture != 0) {
		glDeleteTextures(1, &cubemapTexture);
	}

	vector<string> skyboxPaths = {
			skyboxFolders[currSkyboxIdx] + "\\posx.jpg",
			skyboxFolders[currSkyboxIdx] + "\\negx.jpg",
			skyboxFolders[currSkyboxIdx] + "\\posy.jpg",
			skyboxFolders[currSkyboxIdx] + "\\negy.jpg",
			skyboxFolders[currSkyboxIdx] + "\\posz.jpg",
			skyboxFolders[currSkyboxIdx] + "\\negz.jpg"
	};

	cubemapTexture = loadCubemap(skyboxPaths);
}