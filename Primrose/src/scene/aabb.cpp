#include "scene/aabb.hpp"
#include "log.hpp"

using namespace Primrose;

AABB::AABB() {
	min = glm::vec3(NAN);
	max = glm::vec3(NAN);
}

AABB AABB::fromPoint(glm::vec3 p) {
	AABB aabb;
	aabb.min = p;
	aabb.max = p;
	return aabb;
}

AABB AABB::fromPoints(std::vector<glm::vec3> points) {
	AABB aabb;
	for (const auto& p : points) {
		aabb.addPoint(p);
	}
	return aabb;
}

AABB AABB::fromLocalPoints(std::vector<glm::vec3> points, glm::mat4 modelMatrix) {
	if (points.size() == 0) {
		error("Attempted to create AABB from 0 points");
	}

	AABB aabb = fromPoint(modelMatrix * glm::vec4(points[0], 1));
	for (int i = 1; i < points.size(); ++i) {
		aabb.addPoint(modelMatrix * glm::vec4(points[i], 1));
	}
	return aabb;
}

bool AABB::isEmpty() {
	return glm::all(glm::isnan(min)) || glm::all(glm::isnan(max));
}

void AABB::addPoint(glm::vec3 p) {
	if (isEmpty()) {
		min = p;
		max = p;
		return;
	}

	min.x = std::min(min.x, p.x);
	min.y = std::min(min.y, p.y);
	min.z = std::min(min.z, p.z);
	max.x = std::max(max.x, p.x);
	max.y = std::max(max.y, p.y);
	max.z = std::max(max.z, p.z);
}

void AABB::applyTransform(glm::mat4 transform) {
	auto corners = getCorners();
	AABB newAabb = fromLocalPoints({corners.begin(), corners.end()}, transform);
	min = newAabb.min;
	max = newAabb.max;
}

void AABB::unionWith(Primrose::AABB aabb) {
	if (isEmpty()) {
		min = aabb.min;
		max = aabb.max;
		return;
	};
	if (aabb.isEmpty()) return;

	addPoint(aabb.min);
	addPoint(aabb.max);
}

namespace {
	struct Range {
		float min;
		float max;

		bool isEmpty() {
			return std::isnan(min) || std::isnan(max);
		}
	};

	Range intersectRanges(Range a, Range b) {
		// https://scicomp.stackexchange.com/a/26260
		if (b.min > a.max || a.min > b.max) {
			return Range(NAN, NAN);
		} else {
			return Range(std::max(a.min, b.min), std::min(a.max, b.max));
		}
	}
}

void AABB::intersectWith(Primrose::AABB aabb) {
	if (isEmpty()) return;
	if (aabb.isEmpty()) {
		min = glm::vec3(NAN);
		max = glm::vec3(NAN);
		return;
	}

	Range xRange = intersectRanges(Range(min.x, max.x), Range(aabb.min.x, aabb.max.x));
	Range yRange = intersectRanges(Range(min.y, max.y), Range(aabb.min.y, aabb.max.y));
	Range zRange = intersectRanges(Range(min.z, max.z), Range(aabb.min.z, aabb.max.z));

	if (xRange.isEmpty() || yRange.isEmpty() || zRange.isEmpty()) {
		min = glm::vec3(NAN);
		max = glm::vec3(NAN);
		return;
	};

	min.x = xRange.min;
	max.x = xRange.max;
	min.y = yRange.min;
	max.y = yRange.max;
	min.z = zRange.min;
	max.z = zRange.max;
}

std::vector<glm::vec3> AABB::getCorners() {
	if (isEmpty()) return {};

	return {
		glm::vec3(min.x, min.y, min.z),
		glm::vec3(min.x, min.y, max.z),
		glm::vec3(min.x, max.y, min.z),
		glm::vec3(min.x, max.y, max.z),
		glm::vec3(max.x, min.y, min.z),
		glm::vec3(max.x, min.y, max.z),
		glm::vec3(max.x, max.y, min.z),
		glm::vec3(max.x, max.y, max.z)
	};
}

bool AABB::contains(glm::vec3 p) {
	if (isEmpty()) return false;

	return min.x <= p.x && p.x <= max.x
		&& min.y <= p.y && p.y <= max.y
		&& min.z <= p.z && p.z <= max.z;
}

vk::AabbPositionsKHR AABB::toVkStruct() {
	return vk::AabbPositionsKHR(min.x, min.y, min.z, max.x, max.y, max.z);
}
