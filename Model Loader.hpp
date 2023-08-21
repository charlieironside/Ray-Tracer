#pragma once
#include <fstream>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

struct triangleStruct {
	glm::vec4 p[3];
	glm::vec4 color;
	// pos.w contains volume of bounding volume for triangle
	glm::vec4 boundingVolume;
};

class ModelLoader {
public:
	void load(std::vector<triangleStruct>&triangles) {
		std::fstream read("monkey.txt");
		std::vector<glm::vec3>vertices;
		std::vector<unsigned int>indices;
		
		// flag for weather data is vertex or indices
		char type;
		// three empty variables to temporarily store read data
		float p[3];

		while (read >> type >> p[0] >> p[1] >> p[2]) {
			// if type = v the following 3 data points are vertices
			if (type == 'v') {
				glm::vec3 t;
				t = glm::vec3(p[0], p[1], p[2]);
				vertices.push_back(t);
			}

			// if type = f the following 3 data points form a face/triangle
			if (type == 'f') {
				indices.push_back(int(p[0]));
				indices.push_back(int(p[1]));
				indices.push_back(int(p[2]));
			}

			// case in which type isnt v or f
			else if(type != 'v' && type != 'f')
			{
				printf("TYPE UNREADABLE\n");
			}

			// move fstream to next line of file
			read.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
		}

		// loop through and add triangles read from the file to triangle vector in a triangle struct
		for (int i = 0; i < indices.size(); i+=3) {
			triangleStruct temp;
			temp.p[0] = glm::vec4(vertices[indices[i] - 1], 0);
			temp.p[1] = glm::vec4(vertices[indices[i + 1] - 1], 0);
			temp.p[2] = glm::vec4(vertices[indices[i + 2] - 1], 0);
			temp.color = glm::vec4(1);
			triangles.push_back(temp);
		}
	}

	// returns bounding volume of a triangle
	inline void triangleBoundingVolume(triangleStruct& t) {
		glm::vec3 midPoint;
		midPoint.x = (t.p[0].x + t.p[1].x + t.p[2].x) / 3;
		midPoint.y = (t.p[0].y + t.p[1].y + t.p[2].y) / 3;
		midPoint.z = (t.p[0].z + t.p[1].z + t.p[2].z) / 3;
		t.boundingVolume = glm::vec4(midPoint, 0);

		float a = sqrt(pow(t.p[0].x, 2) + pow(t.p[0].y, 2) + pow(t.p[0].z, 2));
		float b = sqrt(pow(t.p[1].x, 2) + pow(t.p[1].y, 2) + pow(t.p[1].z, 2));
		float c = sqrt(pow(t.p[2].x, 2) + pow(t.p[2].y, 2) + pow(t.p[2].z, 2));
		
		t.boundingVolume.w = (a > b) ? ((a > c) ? a : c) : ((b > c) ? b : c);
	}

	// returns radius of bounding volume of a model and generates bounding volumes for individual triangles
	float boundingVolume(std::vector<triangleStruct>&triangles) {
		float largestVertice = 0;
		// loop through all triangles and calculate their bounding spherical volume
		for (int i = 0; i < triangles.size(); i++) {
			triangleBoundingVolume(triangles[i]);
			if (triangles[i].boundingVolume.w > largestVertice)
				largestVertice = triangles[i].boundingVolume.w;
		}


		return largestVertice;
	}
};
