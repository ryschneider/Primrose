#ifndef PRIMROSE_AABB_HPP
#define PRIMROSE_AABB_HPP

#include <glm/glm.hpp>
#include <vector>
#include <vulkan/vulkan.hpp>

namespace {
	struct Line {
		glm::vec3 pos;
		glm::vec3 dir;
		float length;
	};

	struct Plane {
		glm::vec3 normal;
		float d; // d*normal is a point on the plane
	};
}

namespace Primrose {
	class AABB {
	public:
		AABB();
		static AABB fromPoint(glm::vec3 p);
		static AABB fromPoints(std::vector<glm::vec3> points);
		static AABB fromLocalPoints(std::vector<glm::vec3> points, glm::mat4 modelMatrix);

		const glm::vec3& getMin();
		const glm::vec3& getMax();

		bool isEmpty();

		void addPoint(glm::vec3 p); // extends aabb to include point
		void applyTransform(glm::mat4 transform); // apply transformation to aabb

		void unionWith(AABB aabb);
		void intersectWith(AABB aabb);

		vk::AabbPositionsKHR toVkStruct();

	private:
		std::vector<glm::vec3> getCorners();

		bool contains(glm::vec3 p);

		glm::vec3 min = glm::vec3(NAN);
		glm::vec3 max = glm::vec3(NAN);
	};
}

#endif
