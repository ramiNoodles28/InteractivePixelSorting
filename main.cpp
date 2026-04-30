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
	// Initialize global state
	width = 1200;
	height = 900;
	shader = 0;
	uniXform = 0;
	vao = 0;
	vbuf = 0;
	vcount = 0;
	mesh = NULL;
	maxSpeed = 0.0f;
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
	for (auto s = shaders.begin(); s != shaders.end(); ++s)
		glDeleteShader(*s);
	shaders.clear();
	// Locate uniforms
	uniXform = glGetUniformLocation(shader, "xform");

	// Compile and link new overlay shader program
	shaders.push_back(compileShader(GL_VERTEX_SHADER, "sh_v_overlay.glsl"));
	shaders.push_back(compileShader(GL_FRAGMENT_SHADER, "sh_f_overlay.glsl"));
	overlayShader = linkProgram(shaders);
	for (auto s : shaders) glDeleteShader(s);
	shaders.clear();
	
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

	// Attribute 0: Position (vec2)
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
	// Attribute 1: TexCoords (vec2)
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

	// 5. Create the Texture that will hold the sorted pixels
	glGenTextures(1, &screenTexID);
	glBindTexture(GL_TEXTURE_2D, screenTexID);
	// Set filtering so the pixels look sharp or smooth
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glBindVertexArray(0);

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;

	// Setup Platform/Renderer backends
	ImGui_ImplGLUT_Init();
	ImGui_ImplGLUT_InstallFuncs();
	ImGui_ImplOpenGL3_Init("#version 330");

	// Setup Style
	ImGui::StyleColorsDark();

	assert(glGetError() == GL_NO_ERROR);
}

//// display functions /////////////////////////////////////////////////////
void display() {
	static bool firstFrame = true;
	if (firstFrame) {
		// Re-take control of the mouse AFTER ImGui has initialized in the main loop
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
		///////////////
		if (isVertSort) sortPixelsVertical(); else sortPixelsHorizontal();
		///////////////
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glUseProgram(overlayShader);

		// Upload the modified pixel array to our texture
		glBindTexture(GL_TEXTURE_2D, screenTexID);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixelData.data());

		// Draw the full-screen rectangle with our texture on it
		glBindVertexArray(quadVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glBindVertexArray(0);
		glDisable(GL_DEPTH_TEST);

		createIMGuiWindow();

		assert(glGetError() == GL_NO_ERROR);

		// Revert context state
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
}

void PhotoMode() {
	//MeshMode();
	glUseProgram(overlayShader);
	glDisable(GL_DEPTH_TEST); // No depth needed for a flat photo

	// 2. Set up a simple Identity or Ortho matrix
	// If your quad is already -1 to 1, mat4(1.0f) will fill the screen perfectly
	mat4 identity = mat4(1.0f);
	glUniformMatrix4fv(glGetUniformLocation(overlayShader, "xform"), 1, GL_FALSE, value_ptr(identity));

	if (photoTexID == 0) photoTexID = loadTexture("photos/arches.jpg");

	// 3. Bind the photo and draw the quad
	glBindTexture(GL_TEXTURE_2D, photoTexID);
	glBindVertexArray(quadVAO);
	glDrawArrays(GL_TRIANGLES, 0, 6);
}

//// interactive control functions /////////////////////////////////////////
void createIMGuiWindow() {
	// Start the ImGui frame
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGLUT_NewFrame();
	ImGui::NewFrame();

	// Create the Window
	ImGui::Begin("Pixel Sort Settings");
	ImGui::Checkbox("Toggle Mesh/Photo Modes", &isMeshMode);

	if (isMeshMode) {
		if (ImGui::Combo("Model Select", &currModelIdx, modelLabels.data(), (int)modelLabels.size())) {
			if (mesh) {
				delete mesh;
				mesh = nullptr;
			}
		}
	}
	else {
		if (ImGui::Combo("Select Photo", &currPhotoIdx, photoLabels.data(), (int)photoLabels.size())) {
			// Selection changed! 
			if (photoTexID != 0) {
				glDeleteTextures(1, &photoTexID); // Clean up the old one
			}
			photoTexID = loadTexture(photoFiles[currPhotoIdx].c_str());
		}
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
	
	
	ImGui::Checkbox("Flip Sort Direction", &flipSortDir);
	ImGui::Checkbox("Sort Vertically", &isVertSort);

	ImGui::End();

	// Rendering ImGui
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void reshape(GLint width, GLint height) {
	::width = width;
	::height = height;
	glViewport(0, 0, width, height);
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
	ImGui_ImplGLUT_MouseFunc(button, state, x, y); // Let ImGui try to use it
	if (ImGui::GetIO().WantCaptureMouse) return;      // If ImGui used it, stop here
	
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
			rightMouseDown = true;
			lastMouseX = x;
			lastMouseY = y;
		}
		else {
			rightMouseDown = false;
		}
	}

	if (state == GLUT_DOWN && button == GLUT_MIDDLE_BUTTON) {
		isPanning = true;
		camPanOrigin = camPan;
		mouseOrigin = vec2(x, y); // Reuse mouseOrigin
	}
	if (state == GLUT_UP && button == GLUT_MIDDLE_BUTTON) {
		isPanning = false;
	}

}

void mouseWheel(int wheel, int direction, int x, int y) {
	// Feed to ImGui
	ImGuiIO& io = ImGui::GetIO();
	io.MouseWheel += (float)direction;
	if (io.WantCaptureMouse) return;
	// Direction is 1 for up, -1 for down
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
		float dx = (x - lastMouseX) * 0.05f; // Sensitivity
		float dy = (y - lastMouseY) * 0.05f;

		lightPos.x += dx;
		lightPos.y -= dy; // Invert Y because mouse 0 is at the top

		lastMouseX = x;
		lastMouseY = y;
		glutPostRedisplay();
	}
	if (isPanning) {
		vec2 mouseDelta = vec2(x, y) - mouseOrigin;

		// Scale the pan speed based on distance (camCoords.z) 
		// so it doesn't feel too fast/slow when zoomed
		float panScale = camCoords.z * 0.002f;

		camPan.x = camPanOrigin.x + mouseDelta.x * panScale;
		camPan.y = camPanOrigin.y - mouseDelta.y * panScale; // Flip Y because mouse Y is top-down

		glutPostRedisplay();
	}

}

void idle() {
	randOff = (rand() % maxSpanLength);
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
	
	for (int y = 0; y < height; y++) {
		uint32_t* row = (uint32_t*)&pixelData[y * width * 4];

		int x = 0;
		while (x < width) {
			// Find the start of a span (pixel is bright enough)
			while (x < width && !isColorInThresh(row[x])) x++;
			int start = x;

			// Find the end of that span
			if (useMask) while (x < width && isColorInThresh(row[x])) x++;
			else { x += (rand() % noiseAmount) + randOff; x = x >= width ? width : x; }
			int end = x;

			// Sort only that segment
			if (start < end) {
				sort(row + start, row + end, compareColors);
			}
		}
	}
}

void sortPixelsVertical() {
	// iterate through each column
	for (int x = 0; x < width; x++) {
		// Extract the column into a temporary vector
		vector<uint32_t> column(height);
		for (int y = 0; y < height; y++) {
			// Memory math: (y * width) skips rows, (+ x) picks the column
			column[y] = ((uint32_t*)pixelData.data())[y * width + x];
		}

		// perform the span-based sorting on this column vector
		int y = 0;
		while (y < height) {
			while (y < height && !isColorInThresh(column[y])) y++;
			int start = y;
			if (useMask) while (y < height && isColorInThresh(column[y])) y++;
			else { y += (rand() % noiseAmount) + randOff; y = y >= height ? height : y; }
			int end = y;

			if (start < end) {
				sort(column.begin() + start, column.begin() + end, compareColors);
			}
		}

		// Write the sorted column back into the original pixelData
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
	case RED: A = (a & 0x000000FF); B = (b & 0x000000FF); break;
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

	float H;
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
		// Check for common image extensions
		if (folderPath == "photos" && ext == ".jpg" || ext == ".jpeg" || ext == ".png") {
			files.push_back(entry.path().string());
		}
		else if (folderPath == "models" && ext == ".obj") {
			files.push_back(entry.path().string());
		}
	}

	// Prepare labels for the ImGui Combo box
	for (const auto& s : files) {
		labels.push_back(s.c_str());
	}
}

GLuint loadTexture(const char* path) {
	int w, h, channels;
	stbi_set_flip_vertically_on_load(true);
	unsigned char* data = stbi_load(path, &w, &h, &channels, 4); // Force RGBA

	GLuint tex;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);

	if (tex) {
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);
	}
	else cout << "Failed to load texture: " << path << endl;

	stbi_image_free(data);
	return tex;
}

