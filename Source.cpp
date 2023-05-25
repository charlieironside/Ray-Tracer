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

std::vector<sphereStruct>lights;
std::vector<sphereStruct>spheres;
std::vector<triangleStruct>triangles;

int main()
{
	// Setup
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	// Time before next frame
	glfwSwapInterval(0);

	// Create window object
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


	// Loads Glad.c
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to create glfw window";
		return -1;
	}

	int max_compute_work_group_count[3];
	int max_compute_work_group_size[3];
	int max_compute_work_group_invocations;

	for (int idx = 0; idx < 3; idx++) {
		glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, idx, &max_compute_work_group_count[idx]);
		glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, idx, &max_compute_work_group_size[idx]);
	}
	glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &max_compute_work_group_invocations);

	std::cout << "OpenGL Limitations: " << std::endl;
	std::cout << "maximum number of work groups in X dimension " << max_compute_work_group_count[0] << std::endl;
	std::cout << "maximum number of work groups in Y dimension " << max_compute_work_group_count[1] << std::endl;
	std::cout << "maximum number of work groups in Z dimension " << max_compute_work_group_count[2] << std::endl;

	std::cout << "maximum size of a work group in X dimension " << max_compute_work_group_size[0] << std::endl;
	std::cout << "maximum size of a work group in Y dimension " << max_compute_work_group_size[1] << std::endl;
	std::cout << "maximum size of a work group in Z dimension " << max_compute_work_group_size[2] << std::endl;

	std::cout << "Number of invocations in a single local work group that may be dispatched to a compute shader " << max_compute_work_group_invocations << std::endl;

	ComputeShader computeShader("compute.GLSL");
	Shader shaders("vertex.GLSL", "fragment.GLSL");

	// output image from compute shader
	unsigned int texture;

	glGenTextures(1, &texture);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, TEXTURE_WIDTH, TEXTURE_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);

	glBindImageTexture(0, texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);

	sphereStruct t;

	// add to lights
	t.pos = glm::vec4(0, 2, 1.5, 0);
	t.col = glm::vec4(0, 1, 0, 0.1);
	lights.push_back(t);
	
	t.pos = glm::vec4(0, 0, 2, 0);
	t.col = glm::vec4(0, 0, 1, 0.1);
	lights.push_back(t);

	t.pos = glm::vec4(2, 0, 1.5, 0);
	t.col = glm::vec4(1, 0, 0, 0.1);
	lights.push_back(t);

	ModelLoader loader;
	loader.load(triangles);
	float boundingVolume = loader.boundingVolume(triangles);

	std::cout << triangles.size();

	unsigned int lightSSBO, sphereSSBO, triangleSSBO;
	glGenBuffers(1, &lightSSBO);
	glGenBuffers(1, &sphereSSBO);
	glGenBuffers(1, &triangleSSBO);

	if (lights.size() > 0) {
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, lightSSBO);
		glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(sphereStruct) * lights.size(), &lights[0], GL_DYNAMIC_DRAW);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, lightSSBO);
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

	while (!glfwWindowShouldClose(window))
	{
		currentFrame = glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;

		camera.updatePosition(window, deltaTime);

		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
			glfwSetWindowShouldClose(window, true);
		
		/*glBindBuffer(GL_SHADER_STORAGE_BUFFER, lightSSBO);
		glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, 40 * lights.size(), &lights[0]);*/

		// Set background colour
		//glClearColor(0.5, 0.5, 0.5, 1.0);

		computeShader.use();

		// fov
		computeShader.setFloat("fov", camera.fov);
		// camera pos
		computeShader.setVec3("camera.position", camera.position);
		// cameea target
		computeShader.setVec3("camera.target", camera.target);
		// camera up
		computeShader.setVec3("camera.up", camera.up);
		// camera right
		computeShader.setVec3("camera.right", camera.right);

		computeShader.setInt("sphereCount", spheres.size());
		computeShader.setInt("lightCount", lights.size());
		computeShader.setInt("triangleCount", triangles.size());
		
		computeShader.setVec3("backgroundColor", glm::vec3(0));
		computeShader.setFloat("bVolume", boundingVolume);

		// 32 invocations per work group for Nvidia
		glDispatchCompute((int)TEXTURE_WIDTH/8, (int)TEXTURE_HEIGHT/4, 1);
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		shaders.use();

		renderQuad();

		glfwSwapBuffers(window);
		glfwPollEvents();
	}
	glDeleteTextures(1, &texture);

	glfwTerminate();
	return 0;
}

unsigned int quadVAO = 0;
unsigned int quadVBO;
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
		// setup plane VAO
		glGenVertexArrays(1, &quadVAO);
		glGenBuffers(1, &quadVBO);
		glBindVertexArray(quadVAO);
		glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	}
	glBindVertexArray(quadVAO);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindVertexArray(0);
}

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

	camera.updateTarget(xoffset, yoffset);
}