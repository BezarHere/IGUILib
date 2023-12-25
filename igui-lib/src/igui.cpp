#include "pch.h"
#include "igui.h"
using namespace igui;

// for intelli-sense
constexpr i32 NodeTSIZE = sizeof( Node ) * 16;

namespace igui
{
	Node::Node()
		: m_type{ NodeType::None } {
	}

	Node::Node( NodeType type )
		: m_type{ type } {
	}

	void Interface::update() {
	}

	void Interface::draw( const Window *window, Renderer *renderer ) const {
	}

	index_t Interface::add_node( const Node &node, index_t parent ) {
		if (node.m_parent != InvalidIndex || !node.m_children.empty())
			return InvalidIndex;

		const size_t ln = m_nodes.size();
		m_nodes.push_back( node );
		if (parent == InvalidIndex)
		{
			m_roots.push_back( ln );
		}
		else
		{
			m_nodes[ parent ].m_children.push_back( ln );
			m_nodes[ ln ].m_parent = parent;
		}

		return ln;
	}

	index_t Interface::transfer_branch( const this_type &old, index_t node_index, index_t parent ) {
		return InvalidIndex;
	}

}

