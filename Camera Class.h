#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

class CameraClass {
public:
	float fov = 90, speed = 4;
	float yaw = 90, pitch = 0;
	float sensitivity = 0.1;
	glm::vec3 position;
	glm::vec3 target = glm::vec3(0, 0, 1);
	glm::vec3 right;
	glm::vec3 up;
	const glm::vec3 worldUp = glm::vec3(0, 1, 0);

	CameraClass(glm::vec3 pos = glm::vec3(0)) {
		position = pos;
		updateCameraVectors();
	}

	void updateCameraVectors() {
		right = glm::normalize(glm::cross(worldUp, target));
		up = glm::normalize(glm::cross(target, right));
	}

	void updateTarget(float offsetx, float offsety) {
		offsetx *= sensitivity;
		offsety *= sensitivity;

		yaw -= offsetx;
		pitch += offsety;							// BUGGED

		if (pitch >= 90)
			pitch = 89;
		if (pitch <= -90)
			pitch = -89;

		target.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
		target.y = sin(glm::radians(pitch));
		target.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
		target = glm::normalize(target);

		updateCameraVectors();
	}

	void updatePosition(GLFWwindow* window, float deltaTime) {
		if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
			position += target * speed * deltaTime;
		}
		if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
			position -= target * speed * deltaTime;
		}
		if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
			position -= right * speed * deltaTime;
		}
		if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
			position += right * speed * deltaTime;
		}
		if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
			position += worldUp * speed * deltaTime;
		}
		if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS) {
			position -= worldUp * speed * deltaTime;
		}
		
		updateCameraVectors();
	}
};
