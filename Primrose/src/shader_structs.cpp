#include "shader_structs.hpp"

#include <glm/gtx/matrix_decompose.hpp>

namespace Primrose {
	std::map<PRIM, std::string> PRIM_NAMES = {
		{ PRIM::P1, "P1" },
		{ PRIM::P2, "P2" },
		{ PRIM::P3, "P3" },
		{ PRIM::P4, "P4" },
		{ PRIM::P5, "P5" },
		{ PRIM::P6, "P6" },
		{ PRIM::P7, "P7" },
		{ PRIM::P8, "P8" },
		{ PRIM::P9, "P9" },
		{ PRIM::SPHERE, "SPHERE" },
		{ PRIM::BOX, "BOX" },
		{ PRIM::TORUS, "TORUS" },
		{ PRIM::LINE, "LINE" },
		{ PRIM::CYLINDER, "CYLINDER" },
	};
	std::map<MAT, std::string> MAT_NAMES = {
		{ MAT::M1, "M1" },
		{ MAT::M2, "M2" },
		{ MAT::M3, "M3" },
	};
	std::map<OP, std::string> OP_NAMES = {
		{ OP::UNION, "UNION" },
		{ OP::INTERSECTION, "INTERSECTION" },
		{ OP::DIFFERENCE, "DIFFERENCE" },
		{ OP::IDENTITY, "IDENTITY" },
		{ OP::TRANSFORM, "TRANSFORM" },
		{ OP::RENDER, "RENDER" },
	};
	std::map<OP_FLAG, std::string> OP_FLAG_NAMES = {
		{ OP_FLAG::NONE, "NONE" },
	};
	std::map<UI, std::string> UI_NAMES = {
		{ UI::IMAGE, "IMAGE" },
		{ UI::PANEL, "PANEL" },
		{ UI::TEXT, "TEXT" },
	};
}

glm::vec3 Primrose::getTranslate(glm::mat4 transform) {
	return glm::vec3(transform[3]);
}

glm::vec3 Primrose::getScale(glm::mat4 transform) {
	return glm::vec3(
		glm::length(transform[0]),
		glm::length(transform[1]),
		glm::length(transform[2]));
}

float Primrose::getSmallScale(glm::mat4 matrix) {
	glm::vec3 scale = getScale(matrix);
	return std::min(scale.x, std::min(scale.y, scale.z));
}

glm::mat4 Primrose::transformMatrix(glm::vec3 translate, glm::vec3 scale, float rotAngle, glm::vec3 rotAxis) {
	return glm::translate(translate)
		   * glm::scale(scale)
		   * glm::rotate(rotAngle, glm::normalize(rotAxis));
}

namespace {
	std::vector<std::string> _lines;

	static void linesHelper(std::vector<std::string>& line, int& lineLength) {

		if (lineLength > 80) {
			lineLength = 0;
			if (line.size() > 1) {
				_lines.push_back(fmt::format("{}", fmt::join(line.begin(), line.end() - 1, ", ")));
				line = {line.back()};
				lineLength += line.back().length();
			} else {
				line.push_back(line[0]);
				line.resize(0);
				lineLength = 0;
			}
		}
	}

	static std::string linesDone(std::vector<std::string>& line, int& lineLength) {
		if (line.size() > 0) {
			_lines.push_back(fmt::format("{}", fmt::join(line, ", ")));
		}
		line.resize(0);
		lineLength = 0;

		std::string res = "";
		if (_lines.size() > 0) {
			res = fmt::format("{}\n", fmt::join(_lines, ",\n\t"));
		}

		_lines.resize(0);
		return res;
	}

	static std::string vecToString(glm::vec3 vec) {
		return fmt::format("[{:.4},{:.4},{:.4}]", vec.x, vec.y, vec.z);
	}
}

std::string Primrose::MarchUniforms::toString() {
	std::string out = "";

	out += fmt::format("camPos: {}\n", vecToString(camPos));
	out += fmt::format("camDir: {}\n", vecToString(camDir));
	out += fmt::format("camUp: {}\n", vecToString(camUp));
	out += fmt::format("screenHeight: {:.4}\n", screenHeight);
	out += fmt::format("focalLength: {:.4}\n", focalLength);
	out += fmt::format("invZoom: {:.4}\n", invZoom);
	out += fmt::format("numOperations: {}\n", numOperations);

	std::vector<std::string> line;
	int lineLength = 0;
	int maxPrim = -1;
	int maxTransform = -1;

	std::string header;
	header = fmt::format("operations[{}]:", numOperations);
	for (int i = 0; i < numOperations; ++i) {
		const Operation& op = operations[i];

		// find number of prims/transforms referenced
		switch (op.type) {
			case OP::IDENTITY:
				if (static_cast<int>(op.i) > maxPrim) maxPrim = op.i;
				break;
			case OP::TRANSFORM:
				if (static_cast<int>(op.i) > maxTransform) maxTransform = op.i;
				break;
			case OP::UNION:
			case OP::INTERSECTION:
			case OP::DIFFERENCE:
			case OP::RENDER:
				break;
		}

		std::string params = "";
		switch (op.type) {
			case OP::UNION:
			case OP::INTERSECTION:
			case OP::DIFFERENCE:
				params = fmt::format("{} {}", op.i, op.j);
				break;
			case OP::IDENTITY:
			case OP::TRANSFORM:
			case OP::RENDER:
				params = fmt::format("{}", op.i);
				break;
		}

		if (op.flags == OP_FLAG::NONE) {
			line.push_back(fmt::format("({}: {} {})", i, OP_NAMES[op.type], params));
		} else {
			std::vector<std::string> flags;
			for (const auto& pair : OP_FLAG_NAMES) {
				if (op.flags & pair.first) {
					flags.push_back(pair.second);
				}
			}
			line.push_back(fmt::format("({}: {} {} {})", i, OP_NAMES[op.type], params, fmt::join(flags, " | ")));
		}
		lineLength += line.back().length();

		linesHelper(line, lineLength);
	}
	std::string lines;
	lines = linesDone(line, lineLength);
	if (header.length() + lines.length() > 80) {
		header += "\n\t";
	} else {
		header += " ";
	}
	out += header + lines;


	header = fmt::format("primitives[{}]:", maxPrim+1);
	for (int i = 0; i <= maxPrim; ++i) {
		const Primitive& p = primitives[i];

		switch (p.type) {
			case PRIM::SPHERE:
			case PRIM::BOX:
			case PRIM::CYLINDER:
				line.push_back(fmt::format("({}: {})", i, PRIM_NAMES[p.type]));
				break;
			case PRIM::TORUS:
			case PRIM::LINE:
				line.push_back(fmt::format("({}: {} {})", i, PRIM_NAMES[p.type], p.a));
				break;
			case PRIM::P1:
			case PRIM::P2:
			case PRIM::P3:
			case PRIM::P4:
			case PRIM::P5:
			case PRIM::P6:
			case PRIM::P7:
			case PRIM::P8:
			case PRIM::P9:
				line.push_back("PRIM TYPE UNKNOWN");
				break;
		}
		lineLength += line.back().length();

		linesHelper(line, lineLength);
	}
	lines = linesDone(line, lineLength);
	if (header.length() + lines.length() > 80) {
		header += "\n\t";
	} else if (lines.length() > 0) {
		header += " ";
	}
	out += header + lines;


	header = fmt::format("transformations[{}]:", maxTransform+1);
	for (int i = 0; i <= maxTransform; ++i) {
		const Transformation& p = transformations[i];

		glm::mat4 matrix = glm::inverse(p.invMatrix);
		glm::vec3 pos;
		glm::vec3 scale;
		glm::quat rot;
		glm::vec3 _;
		glm::vec4 __;
		glm::decompose(matrix, scale, rot, pos, _, __);

		std::vector<std::string> parts;
		if (pos != glm::vec3(0)) parts.push_back(fmt::format("pos={}", vecToString(pos)));
		if (scale != glm::vec3(1)) parts.push_back(fmt::format("scale={}", vecToString(scale)));
		if (glm::angle(rot) != 0) {
			parts.push_back(fmt::format("angle={:.4}", glm::angle(rot)));
			parts.push_back(fmt::format("axis={}", vecToString(glm::axis(rot))));
		}
		if (parts.size() == 0) parts.push_back("identity");

		line.push_back(fmt::format("({}: {})", i, fmt::join(parts, " ")));
		lineLength += line.back().length();

		linesHelper(line, lineLength);
	}
	lines = linesDone(line, lineLength);
	if (header.length() + lines.length() > 80) {
		header += "\n\t";
	} else {
		header += " ";
	}
	out += header + lines;

	return out;
}
