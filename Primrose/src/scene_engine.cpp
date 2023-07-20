#include "scene.hpp"
#include "state.hpp"

using namespace Primrose;

static Primitive toPrim(Primrose::SceneNode* node) {
	switch (node->type()) {
		case NODE_SPHERE:
			return Primitive::Sphere();
		case NODE_BOX:
			return Primitive::Box();
		case NODE_TORUS:
			return Primitive::Torus(((TorusNode*)node)->ringRadius / ((TorusNode*)node)->majorRadius);
		case NODE_LINE:
			return Primitive::Line(((LineNode*)node)->height / ((LineNode*)node)->radius);
		case NODE_CYLINDER:
			return Primitive::Cylinder();
		case NODE_UNION:
		case NODE_INTERSECTION:
		case NODE_DIFFERENCE:
			return Primitive();
	}
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
}
#pragma GCC diagnostic pop

static glm::mat4 toMatrix(Primrose::SceneNode* node) {
	glm::vec3 scale = glm::vec3(1);
	switch (node->type()) {
		case NODE_SPHERE:
			scale = glm::vec3(((SphereNode*)node)->radius);
			break;
		case NODE_BOX:
			scale = ((BoxNode*)node)->size;
			break;
		case NODE_TORUS:
			scale = glm::vec3(((TorusNode*)node)->majorRadius);
			break;
		case NODE_LINE:
			scale = glm::vec3(((LineNode*)node)->radius);
			break;
		case NODE_CYLINDER:
			scale = glm::vec3(((CylinderNode*)node)->radius);
			break;
		case NODE_UNION:
		case NODE_INTERSECTION:
		case NODE_DIFFERENCE:
			break;
	}

	return transformMatrix(node->translate, node->scale * scale, node->angle * M_PI / 180.f, node->axis);
}

static void extractPrims(std::vector<Primitive>& prims, Primrose::SceneNode* node) {
	switch (node->type()) {
		case NODE_SPHERE:
		case NODE_BOX:
		case NODE_TORUS:
		case NODE_LINE:
		case NODE_CYLINDER:
		{
			Primitive p = toPrim(node); // TODO add isPrim function to simplify

			if (std::find(prims.begin(), prims.end(), p) == prims.end()) {
				prims.push_back(p);
			}
		}
		case NODE_UNION:
		case NODE_INTERSECTION:
		case NODE_DIFFERENCE:
			for (const auto &ptr: node->children) {
				extractPrims(prims, ptr.get());
			}
			return;
	}
}

static void extractTransforms(std::vector<Transformation> &transforms,
	SceneNode *node, glm::mat4 matrix = glm::mat4(1)) {

	glm::mat4 nodeMatrix = matrix * toMatrix(node);
	Transformation t = Transformation(nodeMatrix);

	if (std::find(transforms.begin(), transforms.end(), t) == transforms.end()) {
		transforms.push_back(t);
	}

	for (const auto& ptr : node->children) {
		extractTransforms(transforms, ptr.get(), nodeMatrix);
	}
}

static uint foldVectorToOp(std::vector<Operation>& globalOps, std::vector<uint>& foldOps, Operation(*opConstructor)(uint, uint, OP_FLAG)) {
	// TODO add flag for operation to perform on range (i to j) instead of specific (i and j)
	if (foldOps.size() == 0) {
		return -1;
	}

	uint index = foldOps[0];
	for (int i = 1; i < foldOps.size(); ++i) {
		globalOps.push_back(opConstructor(index, foldOps[i], OP_FLAG::NONE));
		index = globalOps.size() - 1;
	}

	return index;
}

static uint createOperations(std::vector<Primitive>& prims, std::vector<Transformation>& transforms,
	std::vector<Operation>& ops, SceneNode* node, glm::mat4 matrix = glm::mat4(1)) {
	if (node->hide) return -1;

	glm::mat4 nodeMatrix = matrix * toMatrix(node);
	Transformation transform = Transformation(nodeMatrix);

	std::vector<uint> childOps; // vector of uints referring to the operations which render each child
	for (const auto& child : node->children) {
		uint index = createOperations(prims, transforms, ops, child.get(), nodeMatrix);
		if (index != -1) childOps.push_back(index);
	}

	if (node->isPrim()) {
		uint primI = std::find(prims.begin(), prims.end(), toPrim(node)) - prims.begin();
		uint transformI = std::find(transforms.begin(), transforms.end(), transform) - transforms.begin();

		// TODO :: combine OP_IDENTITY and OP_TRANSFORM
		ops.push_back(Operation::Transform(transformI));
		ops.push_back(Operation::Identity(primI));

		childOps.push_back(ops.size() - 1);
	}

	if (childOps.size() == 0) {
		return -1;
	} else if (childOps.size() == 1) {
		return childOps[0];
	}

	if (node->type() == NODE_DIFFERENCE) {
		DifferenceNode* dif = (DifferenceNode*)node;
		std::vector<uint> baseOps;
		std::vector<uint> subOps;

		for (int i = 0; i < node->children.size(); ++i) { // split childOps into baseOps and subOps
			if (dif->subtractNodes.contains(node->children[i].get())) {
				subOps.push_back(childOps[i]);
			} else {
				baseOps.push_back(childOps[i]);
			}
		}

		uint base = foldVectorToOp(ops, baseOps, Operation::Union);
		uint sub = foldVectorToOp(ops, subOps, Operation::Union);

		if (base == -1) return -1;
		if (sub == -1) return base;

		ops.push_back(Operation::Difference(base, sub));
		return ops.size() - 1;
	} else if (node->type() == NODE_INTERSECTION) {
		return foldVectorToOp(ops, childOps, Operation::Intersection);
	} else if (node->type() == NODE_UNION || node->isPrim()) {
		return foldVectorToOp(ops, childOps, Operation::Union);
	} else {
		throw std::runtime_error("unknown node type, could not generate buffers");
	}
}

void Primrose::Scene::generateUniforms() {
	std::vector<Primitive> prims;
	std::vector<Transformation> transforms;
	std::vector<Operation> ops;
	for (const auto& node : root) {
		extractPrims(prims, node.get()); // extract all unique primitives
		extractTransforms(transforms, node.get()); // extract all unique transforms

		uint index = createOperations(prims, transforms, ops, node.get());
		if (index != -1) ops.push_back(Operation::Render(index));
	}

	if (prims.size() > 100 || transforms.size() > 100 || ops.size() > 100) {
		throw std::runtime_error("vector exceeded buffer size");
	}

	uniforms.numOperations = ops.size();
	std::copy(ops.begin(), ops.end(), uniforms.operations);
	std::copy(prims.begin(), prims.end(), uniforms.primitives);
	std::copy(transforms.begin(), transforms.end(), uniforms.transformations);
}
