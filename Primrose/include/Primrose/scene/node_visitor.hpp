#ifndef PRIMROSE_NODE_VISITOR_HPP
#define PRIMROSE_NODE_VISITOR_HPP

#include "node.hpp"
#include "construction_node.hpp"
#include "primitive_node.hpp"

namespace Primrose {
	class NodeVisitor {
	public:
		virtual void visit(SphereNode*) = 0;
		virtual void visit(BoxNode*) = 0;
		virtual void visit(TorusNode*) = 0;
		virtual void visit(LineNode*) = 0;
		virtual void visit(CylinderNode*) = 0;
		virtual void visit(UnionNode*) = 0;
		virtual void visit(IntersectionNode*) = 0;
		virtual void visit(DifferenceNode*) = 0;
	};
}

#endif
