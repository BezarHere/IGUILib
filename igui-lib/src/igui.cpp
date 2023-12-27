#include "pch.h"
#include "igui.h"
using namespace igui;

using DrawingStateCache = Interface::DrawingStateCache;

#if IGUI_IMPL == IGUI_IGLIB
template <size_t _SZ>
using Vertex2Array = ig::BaseVertexArray<ig::Vertex2, stacklist<ig::Vertex2, _SZ>>;
#endif

template <typename _T>
inline _T _prereserved( const size_t capacity ) {
	_T p;
	p.reserve( capacity );
	return p;
}

constexpr i32 NodeTSIZE = sizeof( Node ) * 16;
constexpr i32 InterfaceTSIZE = sizeof( Interface );
static Interface::nodes_indices g_ForProcessing = _prereserved<Interface::nodes_indices>(4096); // <- a std::stack is limiting

static inline Rectf anchored_rect_dt( const Vec2f &size_dt, const Rectf &anchors ) {
	return { anchors.x * size_dt.x, anchors.y * size_dt.y, anchors.w * size_dt.x, anchors.h * size_dt.y };
}

class NodeTreeIterator;
class NodeTree
{
	friend NodeTreeIterator;
public:

	inline size_t get_depth() const noexcept {
		return m_depth.size();
	}

private:
	Interface::nodes_indices m_nodes;
	Interface::nodes_indices m_depth;
};

class NodeTreeIterator
{

};

namespace igui
{
	struct Interface::DrawingStateCache
	{
#if IGUI_IMPL == IGUI_IGLIB
		Vertex2Array<4> box_vertices{ ig::PrimitiveType::Quad, 4, ig::BufferUsage::Dynamic };
		Vertex2Array<16> ref_box_vertices{ ig::PrimitiveType::Quad, 4 * 4, ig::BufferUsage::Dynamic }; // 1 qaud for each side
#endif // IGUI_IMPL == IGUI_IGLIB
	};


	Node::Node()
		: m_type{ NodeType::None } {
	}

	Node::Node( NodeType type )
		: m_type{ type } {
	}

	Interface::Interface()
		: m_drawing_sc{ nullptr } {
		// ignore this
		constexpr size_t __stuffsz1 = sizeof( DrawingStateCache );
		constexpr size_t __stuffsz2 = sizeof( Interface::SPDrawingStateCache );
		static_assert(sizeof( DrawingStateCache ) <= sizeof( Interface::SPDrawingStateCache ), "Stack buffer is too small for DrawingStateCache");
	}

	Interface::~Interface() {
		if (m_drawing_sc)
		{
			if (m_drawing_sc == reinterpret_cast<void *>(m_sp_drawing_sc.memory.data()))
			{
				m_drawing_sc->~DrawingStateCache();
			}
			else
			{
				delete m_drawing_sc;
			}
		}
	}

	void Interface::update() {
		if (!g_ForProcessing.empty())
			g_ForProcessing.clear();

		// copy the roots
		g_ForProcessing = m_roots;

		while (!g_ForProcessing.empty())
		{
			// hierarchy stuff
			index_t i = g_ForProcessing.back();
			g_ForProcessing.pop_back();

			Node &node = m_nodes[ i ];

			for (index_t k : node.m_children)
				g_ForProcessing.push_back( k );

			// rects and anchors
			if (node.m_rect_dirty)
			{
				const float width_dt = node.m_rect.w - node.m_old_rect.w;
				const float height_dt = node.m_rect.h - node.m_old_rect.h;

				Rectf anchors;
				for (index_t k : node.m_children)
				{
					anchors = get_node( k ).m_rect;
					get_node( k ).m_rect.x += anchors.x * width_dt;
					get_node( k ).m_rect.y += anchors.y * height_dt;
					get_node( k ).m_rect.w += anchors.w * width_dt;
					get_node( k ).m_rect.h += anchors.h * height_dt;
					get_node( k ).m_rect_dirty = true;
				}

				node.m_old_rect = node.m_rect;
				node.m_rect_dirty = false;
			}



		}

	}

	void Interface::draw( const Window *window, Renderer *renderer ) const {
		if (!g_ForProcessing.empty())
			g_ForProcessing.clear();

		if (!m_drawing_sc)
		{
			m_drawing_sc = new(reinterpret_cast<void *>(m_sp_drawing_sc.memory.data())) DrawingStateCache();
		}

		nodes_indices tree_points; // <- size of this is the depth

		// copy the roots
		g_ForProcessing = m_roots;
		while (!g_ForProcessing.empty())
		{
			// hierarchy stuff

			index_t i = g_ForProcessing.back();
			g_ForProcessing.pop_back();

			const Node &node = m_nodes[ i ];

			for (index_t k : node.m_children)
				g_ForProcessing.push_back( k );

			// drawing the node
			switch (node.m_type)
			{
			case NodeType::ReferenceBox:
				{
					
				}
				break;
			case NodeType::Panel:
				{
					m_drawing_sc->box_vertices.get_vertices()[ 0 ];
				}
				break;
			case NodeType::Button:
				{

				}
				break;
			case NodeType::RadialButton:
				{

				}
				break;
			case NodeType::Checkbox:
				{

				}
				break;
			case NodeType::CheckButton:
				{

				}
				break;
			case NodeType::DropdownMenu:
				{

				}
				break;
			case NodeType::DropdownOption:
				{

				}
				break;
			case NodeType::HSlider:
				{

				}
				break;
			case NodeType::VSlider:
				{

				}
				break;
			case NodeType::HProgressBar:
				{

				}
				break;
			case NodeType::VProgressBar:
				{

				}
				break;
			case NodeType::HScrollbar:
				{

				}
				break;
			case NodeType::VScrollbar:
				{

				}
				break;
			case NodeType::Sprite:
				{

				}
				break;
			case NodeType::Line:
				{

				}
				break;
			case NodeType::SolidColor:
				{

				}
				break;
			case NodeType::Label:
				{

				}
				break;
			case NodeType::TextInput:
				{

				}
				break;
			case NodeType::VSeprator:
				{

				}
				break;
			case NodeType::HSeprator:
				{

				}
				break;
			default:
				break;
			}
		}


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

	DrawingStateCache *Interface::get_drawing_sc() {
		return m_drawing_sc;
	}

	const DrawingStateCache *Interface::get_drawing_sc() const {
		return m_drawing_sc;
	}


	void Node::set_position( Vec2f pos ) {
		m_rect.x = pos.x;
		m_rect.y = pos.y;

		if (!m_rect_dirty)
			m_rect_dirty = true;
	}

	void Node::set_size( Vec2f size ) {
		m_rect.w = size.x;
		m_rect.h = size.y;

		if (!m_rect_dirty)
			m_rect_dirty = true;
	}

	void Node::set_rect( Vec2f pos, Vec2f size ) {
		m_rect.x = pos.x;
		m_rect.y = pos.y;
		m_rect.w = size.x;
		m_rect.h = size.y;

		if (!m_rect_dirty)
			m_rect_dirty = true;
	}

	void Node::set_rect( Rectf rect ) {
		m_rect = rect;

		if (!m_rect_dirty)
			m_rect_dirty = true;
	}

}

