#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <iostream>

#include "Camera Class.h"
#include "vertices.h"
#include "Shader.h"
#include "Compute Shader.h"
#include "Model Loader.hpp"

void framebuffer_size_callback(GLFWwindow *window, int width, int height);
void mouse_callback(GLFWwindow *window, double xpos, double ypos);
void renderQuad();

const int SCRN_WIDTH = 1000, SCRN_HEIGHT = 1024;
const int TEXTURE_WIDTH = 1024, TEXTURE_HEIGHT = 1024;
bool firstMouse = true;
float lastX = 0, lastY = 0;
CameraClass camera(glm::vec3(0, 0, 3));

float deltaTime, lastFrame = 0, currentFrame;

struct sphereStruct {
	glm::vec4 pos;
	// color.w contains radius
	glm::vec4 col;
};

unsigned int lightCount = 0, sphereCount = 0;

// vectors will be filled then sent to gpu
std::vector<sphereStruct>lights;
std::vector<sphereStruct>spheres;
std::vector<triangleStruct>triangles;

int main()
{
	// glfw setup to show window
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	// time before displaying next frame
	glfwSwapInterval(0);

	// create window object
	GLFWwindow* window = glfwCreateWindow(SCRN_WIDTH, SCRN_HEIGHT, "Ray Tracer", NULL, NULL);
	if (window == NULL)
	{
		std::cout << "Falid to create glfw object\n";
		glfwTerminate();
		return -1;
	}

	glfwMakeContextCurrent(window);
	glfwSetCursorPosCallback(window, mouse_callback);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);


	// loads Glad.c
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to create glfw window";
		return -1;
	}

	// dimensions of work groups
	int max_compute_work_group_count[3];
	int max_compute_work_group_size[3];
	int max_compute_work_group_invocations;

	for (int idx = 0; idx < 3; idx++) {
		glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, idx, &max_compute_work_group_count[idx]);
		glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, idx, &max_compute_work_group_size[idx]);
	}
	glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &max_compute_work_group_invocations);

	// display maximum dimensions for work groups
	std::cout << "OpenGL Limitations: " << std::endl;
	std::cout << "maximum number of work groups in X dimension " << max_compute_work_group_count[0] << std::endl;
	std::cout << "maximum number of work groups in Y dimension " << max_compute_work_group_count[1] << std::endl;
	std::cout << "maximum number of work groups in Z dimension " << max_compute_work_group_count[2] << std::endl;

	std::cout << "maximum size of a work group in X dimension " << max_compute_work_group_size[0] << std::endl;
	std::cout << "maximum size of a work group in Y dimension " << max_compute_work_group_size[1] << std::endl;
	std::cout << "maximum size of a work group in Z dimension " << max_compute_work_group_size[2] << std::endl;

	std::cout << "Number of invocations in a single local work group that may be dispatched to a compute shader " << max_compute_work_group_invocations << std::endl;

	// initialize shader objects
	ComputeShader computeShader("compute.GLSL");
	Shader shaders("vertex.GLSL", "fragment.GLSL");

	// output image from compute shader
	unsigned int texture;

	// generate texture buffer objects
	glGenTextures(1, &texture);
	glActiveTexture(GL_TEXTURE0);
	
	// specify setting of texture objects
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, TEXTURE_WIDTH, TEXTURE_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);

	glBindImageTexture(0, texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);

	// temporary objects to fill vectors with
	sphereStruct t;

	// fill temporary object with a light object
	t.pos = glm::vec4(0, 2, 1.5, 0);
	t.col = glm::vec4(0, 1, 0, 0.1);
	// add temporary object to lights vector
	lights.push_back(t);
	
	t.pos = glm::vec4(0, 0, 2, 0);
	t.col = glm::vec4(0, 0, 1, 0.1);
	lights.push_back(t);

	t.pos = glm::vec4(2, 0, 1.5, 0);
	t.col = glm::vec4(1, 0, 0, 0.1);
	lights.push_back(t);

	ModelLoader loader;
	// load model data from file into "triangles"
	loader.load(triangles);
	// build simple BVH around "triangles" model
	float boundingVolume = loader.boundingVolume(triangles);

	// initialize shader storage buffer objects
	unsigned int lightSSBO, sphereSSBO, triangleSSBO;
	// create buffer for lights, spheres and model data
	glGenBuffers(1, &lightSSBO);
	glGenBuffers(1, &sphereSSBO);
	glGenBuffers(1, &triangleSSBO);

	// first check that buffer isnt empty
	if (lights.size() > 0) {
		// bind lights as the current buffer
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, lightSSBO);
		// specify layout of data, type, size of buffer in bytes, first element, access specifier
		glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(sphereStruct) * lights.size(), &lights[0], GL_DYNAMIC_DRAW);
		// bind buffer to 1st buffer
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, lightSSBO);
		// remove buffer binding
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	}
	
	if (spheres.size() > 0) {
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, sphereSSBO);
		glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(sphereStruct) * spheres.size(), &spheres[0], GL_DYNAMIC_DRAW);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, sphereSSBO);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	}

	if (triangles.size() > 0) {
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, triangleSSBO);
		glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(triangleStruct) * triangles.size(), &triangles[0], GL_DYNAMIC_DRAW);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, triangleSSBO);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	}

	// render loop
	while (!glfwWindowShouldClose(window))
	{
		// calculate time between frames
		currentFrame = glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;

		// call function to handle keyboard inputs
		camera.updatePosition(window, deltaTime);

		// close window if escape is pressed
		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
			glfwSetWindowShouldClose(window, true);

		// code to update one of the SSBO mid frame
		/*glBindBuffer(GL_SHADER_STORAGE_BUFFER, lightSSBO);
		glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, 40 * lights.size(), &lights[0]);*/

		// activate compute shaders
		computeShader.use();

		// send uniform variables to gpu
		computeShader.setFloat("fov", camera.fov);
		computeShader.setVec3("camera.position", camera.position);
		computeShader.setVec3("camera.target", camera.target);
		computeShader.setVec3("camera.up", camera.up);
		computeShader.setVec3("camera.right", camera.right);

		computeShader.setInt("sphereCount", spheres.size());
		computeShader.setInt("lightCount", lights.size());
		computeShader.setInt("triangleCount", triangles.size());
		
		computeShader.setVec3("backgroundColor", glm::vec3(0));
		computeShader.setFloat("bVolume", boundingVolume);

		// specify dimensions of invocations, 32 invocations per work group for Nvidia
		glDispatchCompute((int)TEXTURE_WIDTH/8, (int)TEXTURE_HEIGHT/4, 1);
		// pause render loop until compute shaders have finished operating on textrue
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// activate shaders to draw texture to quad
		shaders.use();

		// render quad and display texture ontop
		renderQuad();

		// swap previouse frame buffer with current one
		glfwSwapBuffers(window);
		glfwPollEvents();
	}
	glDeleteTextures(1, &texture);

	glfwTerminate();
	return 0;
}

unsigned int quadVAO = 0;
unsigned int quadVBO;
// draw simple quad to render texture to
void renderQuad()
{
	if (quadVAO == 0)
	{
		float quadVertices[] = {
			// positions        // texture Coords
			-1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
			-1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
			 1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
			 1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
		};
		// initialize VAO
		glGenVertexArrays(1, &quadVAO);
		glGenBuffers(1, &quadVBO);
		glBindVertexArray(quadVAO);
		// bind VAO
		glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
		// declare data going to buffer
		glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		// define the layout of vertex data
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	}
	glBindVertexArray(quadVAO);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindVertexArray(0);
}

// handle mouse input
void mouse_callback(GLFWwindow *window, double xposIn, double yposIn)
{
	float xpos = static_cast<float>(xposIn);
	float ypos = static_cast<float>(yposIn);

	if (firstMouse)
	{
		lastX = xpos;
		lastY = ypos;
		firstMouse = false;
	}

	float xoffset = xpos - lastX;
	float yoffset = lastY - ypos;

	lastX = xpos;
	lastY = ypos;

	// update camera vectors
	camera.updateTarget(xoffset, yoffset);
}
