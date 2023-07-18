#version 460 core
// specifies amount of invocations per word group
layout(local_size_x = 8, local_size_y = 4, local_size_z = 1) in;
// texture to write to
layout(rgba32f, binding = 0) uniform image2D screen;

#define PI 3.14159
#define MAX_RAY 10000

// different shape types
#define _type_light 1
#define _type_sphere 2
#define _type_triangle 3

struct _sphere{
	vec4 position;
	// color.w contains radius
	vec4 color;
};

struct _hit {
	// index of item
	int i;
	// type of item
	int T;
	// intersection
	vec3 inter;
};

struct _ray{
	float t;
	// origin and second point on ray
	vec3 origin, p2;
	vec3 color;
};

struct cam {
	vec3 position, target, right, up;
};

struct _plane {
	vec3 normal, d;
};

struct _triangle {
	vec4 p[3];
	vec4 color;
	vec4 bVolume;
};

// buffers
layout(std430, binding = 1) buffer lightData {
	_sphere lights[];
};

layout(std430, binding = 2) buffer sphereData {
	_sphere spheres[];
};

layout(std430, binding = 3) buffer triangleData{
	_triangle model[];
};

// simulation variables
uniform float fov;
uniform int sphereCount;
uniform int lightCount;
uniform int triangleCount;
uniform float time;
uniform float bVolume;

uniform vec3 backgroundColor;

_hit rayCast(_ray ray);
bool rayCast2(_ray ray, int exeptionIndex, int exeptionType);

// camera
uniform cam camera;

// quadratic formula
float quadraticFormula(float discriminant, vec3 rayp2, vec3 rayOrigin, vec3 spherePosition) {
	float a = dot(rayp2, rayp2);
	float b = 2 * dot(rayp2, rayOrigin - spherePosition);
	float t = (-b / (2 * a)) - (sqrt(discriminant) / (2 * a));
	
	return t;
}

// solve discriminant
// args; ray, sphere position, shpere radius
float sphereRayCast(_ray r, vec3 sp, float rad) {
	vec3 q = r.origin - sp;
	float a = dot(r.p2, r.p2);
	float b = 2 * dot(r.p2, q);
	float c = dot(q, q) - rad * rad;
	return (b * b) - (4 * a * c);
}

float toDegrees(float r) {
	return r * 180 / PI;
}

float findAngle(vec3 a, vec3 b) {
	float dotp = dot(a, b);
	return acos(dotp);
}

// calculate all lighting from spheres and add to pixel value
void sphereLighting(_ray r, unsigned int sphereIndex, vec3 inter, inout vec4 pix) {
	vec3 surfaceNormal = normalize(inter - vec3(spheres[sphereIndex].position));
	// run ray cast 
	for (int j = 0; j < lightCount; j++) {
		vec3 lightRay = normalize(vec3(lights[j].position) - inter);
		
		r.color = vec3(lights[j].color) * vec3(spheres[sphereIndex].color) * dot(surfaceNormal, lightRay);

		pix += vec4(r.color, 1);
	}
}

// find plane equation
_plane findPlaneEquation(vec3 p1, vec3 p2, vec3 p3) {
	_plane plane;
	// two vectors that exist on plane
	vec3 vector1 = p1 - p2;
	vec3 vector2 = p1 - p3;
	// surface normal of plane
	plane.normal = normalize(cross(vector1, vector2));
	// substitute to find d
	plane.d = p1;
	return plane;
}

// ray plane intersection
float rayPlaneIntersection(_plane plane, _ray ray) {
	float bottomHalf = dot(ray.p2, plane.normal);
	if (bottomHalf == 0) return -1.0f;
	float topHalf = dot(plane.d - ray.origin, plane.normal);
	float t = topHalf / bottomHalf;
	if (t > 0) {
		return t;
	}
	else
		return -1.0f;
}

// ray triangle intersection using angle
bool pointInTriangle(_triangle tri, vec3 intersect) {
	vec3 lines[3];
	float angles[3];
	for (int i = 0; i < 3; i++) {
		lines[i] = normalize(vec3(tri.p[i]) - intersect);
	}
	angles[0] = toDegrees(findAngle(lines[0], lines[1]));
	angles[1] = toDegrees(findAngle(lines[1], lines[2]));
	angles[2] = toDegrees(findAngle(lines[2], lines[0]));

	//pixel = vec4((angles[0] + angles[1] + angles[2]) / 360, 0, 0, 1);

	if (angles[0] + angles[1] + angles[2] > 359)
		return true;
	else
		return false;
}

// return >= 0 if ray intersects triangle and i = intersection
float triangleRayCast(_triangle triangle, _ray ray) {
	vec3 tri1 = vec3(triangle.p[0]);
	vec3 tri2 = vec3(triangle.p[1]);
	vec3 tri3 = vec3(triangle.p[2]);

	_plane plane = findPlaneEquation(tri1, tri2, tri3);
	float t = rayPlaneIntersection(plane, ray);
	
	if (t >= 0) {
		vec3 intersection = ray.origin + ray.p2 * t;

		if (pointInTriangle(triangle, intersection)) {
			return t;
		}
	}
	else
		return -1.0;
}

// function for triangles
void triangleLighting(_hit hit, _ray ray, inout vec4 pixel) {
	_triangle t = model[hit.i];
	_plane plane = findPlaneEquation(vec3(t.p[0]), vec3(t.p[1]), vec3(t.p[2]));

	vec4 color;

	for (int i = 0; i < lightCount; i++) {
		_ray lightRay;
		lightRay.origin = hit.inter;
		lightRay.p2 = normalize(vec3(lights[i].position) - hit.inter);

		float light = dot(lightRay.p2, -plane.normal);
		if (dot(ray.p2, plane.normal) < 0)
			light *= -1;
		light = max(light, 0);

		vec4 color = lights[i].color * t.color;
		pixel += vec4(color * light);
	}

}

_hit rayCast(_ray ray, bool doTriangles) {
	_hit hitDesc;

	float t = MAX_RAY, T = 0;
	int closestIndex = -1;
	int closestType = 0;

	// loop for all lights
	for (int i = 0; i < lightCount; i++) {
		float discriminant = sphereRayCast(ray, vec3(lights[i].position), lights[i].color.w);
		if (discriminant >= 0) {
			T = quadraticFormula(discriminant, ray.p2, ray.origin, vec3(lights[i].position));
			if (T < t && T > 0) {
				t = T;
				// record index and type of intersection
				closestIndex = i;
				closestType = _type_light;
				hitDesc.inter = ray.origin + ray.p2 * t;
			}
		}
	}

	// loop for all lights
	for (int i = 0; i < sphereCount; i++) {
		float discriminant = sphereRayCast(ray, vec3(spheres[i].position), spheres[i].color.w);
		if (discriminant >= 0) {
			T = quadraticFormula(discriminant, ray.p2, ray.origin, vec3(spheres[i].position));
			if (T < t && T > 0) {
				t = T;
				// record index and type of intersection
				closestIndex = i;
				closestType = _type_sphere;
				hitDesc.inter = ray.origin + ray.p2 * t;
			}
		}
	}

	// loop for triangles
	if (doTriangles) {
		for (int i = 0; i < triangleCount; i++) {
			// bvh test
			if (sphereRayCast(ray, vec3(model[i].bVolume), model[i].bVolume.w) > 0) {
				T = triangleRayCast(model[i], ray);
				if (T >= 0) {
					if (T < t && T > 0) {
						t = T;
						closestIndex = i;
						closestType = _type_triangle;
						hitDesc.inter = ray.origin + ray.p2 * T;
					}
				}
			}
		}
	}

	hitDesc.T = closestType;
	hitDesc.i = closestIndex;

	return hitDesc;
}

// calculate the lighting for pixel
void lighting(_hit h, _ray ray, inout vec4 pixel) {
	switch (h.T) {
		case _type_light:
			pixel = vec4(vec3(lights[h.i].color), 1);
			break;

		case _type_sphere:
			sphereLighting(ray, h.i, h.inter, pixel);
			break;

		case _type_triangle:
			triangleLighting(h, ray, pixel);
			break;

		case 0:
			pixel = vec4(backgroundColor, 1);
	}
}

void main() { 
	float aspectRatio = 1;

	// contains color of pixel being worked on
	vec4 pixel = vec4(0);
	// coordinates of what pixel this invocation is working on
	ivec2 pixel_coords = ivec2(gl_GlobalInvocationID.xy);
	// screen dimensions
	ivec2 dims = imageSize(screen);
	// transform from -1 to 1
	float x = (float(pixel_coords.x * 2 - dims.x) / dims.x);
	float y = (float(pixel_coords.y * 2 - dims.y) / dims.y);

	float height = -tan(fov * 0.5);
	float width = height * aspectRatio;

	// create the ray
	_ray ray;
	ray.t = 1000000000;
	ray.origin = camera.position;

	ray.p2 = x * camera.right + y * camera.up;
	ray.p2 += camera.target;
	ray.p2 = normalize(ray.p2);
	_hit hit;

	bool bVolumeTest = true;
	// bounding volume test
	if (sphereRayCast(ray, vec3(0), bVolume) > 0) {
		bVolumeTest = true;
	}

	hit = rayCast(ray, bVolumeTest);
	lighting(hit, ray, pixel);


	imageStore(screen, pixel_coords, pixel);
}
