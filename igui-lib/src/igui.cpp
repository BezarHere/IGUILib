#include "pch.h"
#include "igui.h"
using namespace igui;

#include "internal.h"

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

constexpr size_t NodeTSIZE = sizeof( Node );
constexpr size_t InterfaceTSIZE = sizeof( Interface );
static_assert(NodeTSIZE < 256, "A node should be smaller then 256B, but it's NOT, fix");
static_assert(InterfaceTSIZE < 8192, "What are you doing? an interface should be smaller then 8KB");

class Interface::NodeTree
{
public:
	struct ParentNodeConnection
	{
		index_t next_node;
		Vec2f position; // <- global position
		Color modulate;
	};

	inline NodeTree( const Interface::nodes_indices &roots, const Interface::nodes_collection &nodes )
		: m_nodes{ roots }, m_nodes_ref{ nodes }, m_connections{} {
	}

	// might be wrong
	inline size_t get_depth() const noexcept {
		return m_connections.size();
	}

	inline operator bool() const noexcept {
		return !m_nodes.empty();
	}

	inline const ParentNodeConnection *get_connection() const {
		if (m_connections.empty())
			return nullptr;

		return &m_connections.back();
	}

	inline index_t next_node() {
		// this function must not be called with no nodes

		const index_t node_index = m_nodes.back();
		m_nodes.pop_back();
		const Node &node = m_nodes_ref[ node_index ];

		const index_t next_node_index = m_nodes.empty() ? npos : m_nodes.back();

		if (!node.m_children.empty())
		{
			// adding two connections with the same next_node will create all sorts of bugs
			if (!m_connections.empty() && m_connections.back().next_node == next_node_index)
			{
				// update the last connection
				m_connections.back().position.x += node.m_rect.x;
				m_connections.back().position.y += node.m_rect.y;
				// TODO: modulate
			}
			else
			{
				// add a new connection
				m_connections.emplace_back( next_node_index,
																		Vec2f( node.m_rect.x, node.m_rect.y ),
																		Color( 1.f, 1.f, 1.f, 1.f ) );
			}


			for (index_t k : node.m_children)
			{
				m_nodes.push_back( k );
			}
		}
		// no children to add means that the next node is exposed
		// check if the ParentNodeConnection is bound to it
		else if (!m_connections.empty() && next_node_index != npos)
		{
			pop_pncs();
		}

		return node_index;
	}

private:
	inline void pop_pncs() {
		if (m_connections.back().next_node == m_nodes.back())
			m_connections.pop_back();
	}

private:
	typename const Interface::nodes_collection &m_nodes_ref;
	Interface::nodes_indices m_nodes;
	vector<ParentNodeConnection> m_connections;
};

class Interface::InputRecord
{
public:
	static constexpr size_t RecordSize = 512;
	static constexpr size_t key_offset = 32;
	static constexpr size_t mbutton_offset = 1;
	static constexpr size_t mscroll_offset = 0;

	struct InputValue
	{
		union
		{
			bool active;
			struct
			{
				int16_t x, y;
			};

		} state;
		index_t update_tick;
	};

	inline void update() {
		m_tick++;
	}

	inline void input( InputEvent event ) {
		const ig::InputEvent input = event.input;

		switch (event.type)
		{
		case ig::InputEventType::Key:
			{
				if (m_values[ static_cast<size_t>(input.key.keycode) + key_offset ].state.active != (input.key.action != ig::InputAction::Released))
				{
					m_values[ static_cast<size_t>(input.key.keycode) + key_offset ].update_tick = m_tick;
					m_values[ static_cast<size_t>(input.key.keycode) + key_offset ].state.active = (input.key.action != ig::InputAction::Released);
				}
			}
			break;
		case ig::InputEventType::MouseButton:
			{
				if (m_values[ static_cast<size_t>(input.mouse_button.button) + mbutton_offset ].state.active != (input.mouse_button.action != ig::InputAction::Released))
				{
					m_values[ static_cast<size_t>(input.mouse_button.button) + mbutton_offset ].update_tick = m_tick;
					m_values[ static_cast<size_t>(input.mouse_button.button) + mbutton_offset ].state.active = (input.mouse_button.action != ig::InputAction::Released);
				}
			}
			break;
		case ig::InputEventType::MouseScrollWheel:
			{
				m_values[ mscroll_offset ].update_tick = m_tick;
				m_values[ mscroll_offset ].state.x = input.mouse_scroll.x;
				m_values[ mscroll_offset ].state.y = input.mouse_scroll.y;
			}
			break;
		default:
			break;
		}
	}

	inline bool is_pressed( const KeyCode key ) {
		return m_values[ static_cast<size_t>(key) + key_offset ].state.active;
	}

	inline bool is_just_released( const KeyCode key ) {
		return m_values[ static_cast<size_t>(key) + key_offset ].update_tick == m_tick && !m_values[ static_cast<size_t>(key) + key_offset ].state.active;
	}

	inline bool is_just_pressed( const KeyCode key ) {
		return m_values[ static_cast<size_t>(key) + key_offset ].update_tick == m_tick && m_values[ static_cast<size_t>(key) + key_offset ].state.active;
	}


	inline bool is_pressed( const MouseButton button ) {
		return m_values[ static_cast<size_t>(button) + mbutton_offset ].state.active;
	}

	inline bool is_just_released( const MouseButton button ) {
		return m_values[ static_cast<size_t>(button) + mbutton_offset ].update_tick == m_tick && !m_values[ static_cast<size_t>(button) + mbutton_offset ].state.active;
	}

	inline bool is_just_pressed( const MouseButton button ) {
		return m_values[ static_cast<size_t>(button) + mbutton_offset ].update_tick == m_tick && m_values[ static_cast<size_t>(button) + mbutton_offset ].state.active;
	}

	
	Vec2f mouse_pos; // transformed
	Vec2f raw_mouse_pos; // raw
private:
	size_t m_tick;
	stacklist<InputValue, RecordSize> m_values;
};

namespace igui
{
	using IndexBuffer = ig::Index8Buffer;


	struct Interface::DrawingStateCache
	{
		DrawingStateCache() {
			constexpr ig::Vertex2 BaseVertex2 = { ig::Vector2f(), ig::ColorfTable::White, ig::Vector2f() };
			constexpr std::array<ig::Vector2f, 4> BoxDirTable = { ig::Vector2f( 0.f, 0.f ), ig::Vector2f( 1.f, 0.f ), ig::Vector2f( 1.f, 1.f ), ig::Vector2f( 0.f, 1.f ) };

			for (size_t i = 0; i < box_vertices.get_vertices().size(); i++)
			{
				box_vertices.get_vertices()[ i ].pos = BoxDirTable[ i ] * 32.f;
				box_vertices.get_vertices()[ i ].clr = ig::ColorfTable::White;
				box_vertices.get_vertices()[ i ].uv = BoxDirTable[ i ];
			}

		}

		inline void start( Renderer *const r ) {
			m_renderer = r;
		}

		inline void gen_box( const float x, const float y, const float w, const float h ) {
			box_vertices[ 0 ].pos.x = x;
			box_vertices[ 0 ].pos.y = y;

			box_vertices[ 1 ].pos.x = x + w;
			box_vertices[ 1 ].pos.y = y;

			box_vertices[ 2 ].pos.x = x + w;
			box_vertices[ 2 ].pos.y = y + h;

			box_vertices[ 3 ].pos.x = x;
			box_vertices[ 3 ].pos.y = y + h;

			if (m_dirty_color)
			{
				for (size_t i = 0; i < box_vertices.get_vertices().size(); i++)
				{
					box_vertices[ i ].clr = m_active_style_element.color;
				}
			}

			box_vertices.update();
			m_dirty_color = false;
		}

		inline void gen_frame( const Rectf &outer, const Rectf &inner ) {
			const Rectf::vector_type outer_end = outer.end();
			const Rectf::vector_type inner_end = inner.end();
			// quad 0
			frame_vertices[ 0 ].pos = { outer.x, outer.y };
			frame_vertices[ 1 ].pos = { inner.x, inner.y };
			frame_vertices[ 2 ].pos = { inner_end.x, inner.y };
			frame_vertices[ 3 ].pos = { outer_end.x, outer.y };

			// quad 1
			frame_vertices[ 4 ].pos = inner_end;
			frame_vertices[ 5 ].pos = outer_end;

			// quad 2
			frame_vertices[ 6 ].pos = { inner.x, inner_end.y };
			frame_vertices[ 7 ].pos = { outer.x, outer_end.y };

			// quad 3
			frame_vertices[ 8 ].pos = frame_vertices[ 1 ].pos;
			frame_vertices[ 9 ].pos = frame_vertices[ 0 ].pos;

			if (m_dirty_color)
			{
				for (size_t i = 0; i < frame_vertices.get_vertices().size(); i++)
				{
					frame_vertices[ i ].clr = m_active_style_element.color;
				}
			}

			frame_vertices.update();
			m_dirty_color = false;
		}

		inline void apply_style_element( const StyleElement &element ) {
			if (element.color != m_active_style_element.color)
			{
				m_active_style_element.color = element.color;
				m_dirty_color = true;
			}

			if (element.texture != m_active_style_element.texture)
			{
				m_active_style_element.texture = element.texture;
				m_renderer->bind_texture( element.texture->get_handle() );
			}
		}

		inline void draw_box() const {
			m_renderer->get_canvas().draw( box_vertices.get_buffer(), index_buffers.box );
		}

		inline void draw_frame() const {
			m_renderer->get_canvas().draw( frame_vertices.get_buffer(), index_buffers.frame );
		}

		Vertex2Array<4> box_vertices{ ig::PrimitiveType::Triangle, 4, ig::BufferUsage::Stream }; // quad
		Vertex2Array<10> frame_vertices{ ig::PrimitiveType::Triangle, 4 * 2 + 2, ig::BufferUsage::Stream }; // quad strip
		const struct {
			static constexpr IndexBuffer::element_type BoxI[ 6 * 1 ] = { 0, 1, 2, 2, 3, 0 };
			static constexpr IndexBuffer::element_type FrameI[ 6 * 4 ] = {
				0, 1, 2, 2, 3, 0, // quad 0
				3, 2, 4, 4, 5, 3, // quad 1
				5, 4, 6, 6, 7, 5, // quad 2
				7, 6, 8, 8, 9, 7 // quad 3
			};

			const IndexBuffer box = { std::size( BoxI ), ig::VBufferUsage::StaticDraw, BoxI };
			const IndexBuffer frame = { std::size( FrameI ), ig::VBufferUsage::StaticDraw, FrameI };
		} index_buffers;
	private:
		StyleElement m_active_style_element = {};
		bool m_dirty_color = true;
		Renderer *m_renderer = nullptr;
	};


	Node::Node()
		: m_type{ NodeType::None }, m_index{ npos } {
	}

	Node::Node( NodeType type )
		: m_type{ type }, m_index{ npos } {
	}

	Interface::Interface()
		: m_drawing_sc{ nullptr }, m_ticks{ 0 } {
		// ignore this
		constexpr size_t DrawingStateCacheSIZE = sizeof( DrawingStateCache );
		constexpr size_t SPDrawingStateCacheSIZE = sizeof( Interface::SPDrawingStateCache );
		static_assert(DrawingStateCacheSIZE <= SPDrawingStateCacheSIZE, "Stack buffer is too small for DrawingStateCache");
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
		NodeTree tree{ m_roots, m_nodes };

		for (index_t i = 0; tree; i = tree.next_node())
		{
			Node &node = m_nodes[ i ];

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

		m_ticks++;
	}

	void Interface::draw( Renderer *renderer ) const {
		using ParentNodeConnection = NodeTree::ParentNodeConnection;
		static const ParentNodeConnection DefaultPNC = { 0 };

		if (!m_drawing_sc)
		{
			m_drawing_sc = new(reinterpret_cast<void *>(m_sp_drawing_sc.memory.data())) DrawingStateCache();
		}

		m_drawing_sc->start( renderer );

		m_input->raw_mouse_pos = Vec2f( renderer->get_window().get_mouse_position() );
		m_input->mouse_pos = renderer->get_canvas().transform2d() * m_input->raw_mouse_pos;

		NodeTree tree{ m_roots, m_nodes };


		for (index_t i = 0; tree; i = tree.next_node())
		{
			const Node &node = m_nodes[ i ];
			const ParentNodeConnection *con = ([]( const ParentNodeConnection *c ) { return c ? c : &DefaultPNC; })(tree.get_connection());
			const Vec2f node_global_pos = { con->position.x + node.m_rect.x, con->position.y + node.m_rect.y };
			const Rectf node_global_rect = { node_global_pos.x, node_global_pos.y, node.m_rect.w, node.m_rect.h };

			// drawing the node
			switch (node.m_type)
			{
			case NodeType::Panel:
				{
					m_drawing_sc->apply_style_element(
						m_style.panel.background.value_or( m_style.base.background.value() )
					);

					m_drawing_sc->gen_box( node_global_pos.x, node_global_pos.y, node_global_rect.w, node_global_rect.h );
					m_drawing_sc->draw_box();

					const auto boarder = m_style.panel.boarder.value_or( m_style.base.boarder.value() );
					if (boarder.style.has_value())
					{
						m_drawing_sc->apply_style_element( boarder.style.value() );

						m_drawing_sc->gen_frame( { node_global_pos.x - boarder.left,
																			 node_global_pos.y - boarder.top,
																			 node_global_rect.w + boarder.left + boarder.right,
																			 node_global_rect.h + boarder.top + boarder.bottom },
																		 node_global_rect );
						m_drawing_sc->draw_frame();
					}
				}
				break;
			case NodeType::Button:
				{
					m_drawing_sc->apply_style_element(
						m_style.button.background.value_or( m_style.base.background.value() )
					);

					m_drawing_sc->gen_box( node_global_pos.x, node_global_pos.y, node_global_rect.w, node_global_rect.h );
					m_drawing_sc->draw_box();

					const auto boarder = m_style.panel.boarder.value_or( m_style.base.boarder.value() );
					if (boarder.style.has_value())
					{
						m_drawing_sc->apply_style_element( boarder.style.value() );

						m_drawing_sc->gen_frame( { node_global_pos.x - boarder.left,
																			 node_global_pos.y - boarder.top,
																			 node_global_rect.w + boarder.left + boarder.right,
																			 node_global_rect.h + boarder.top + boarder.bottom },
																		 node_global_rect );
						m_drawing_sc->draw_frame();
					}

				}
				break;
			default:
				break;
			}


		}


	}

	void Interface::input( InputEvent event ) {

	}

	index_t Interface::add_node( const Node &node, index_t parent ) {
		// use add_branch() for this case, not add_node()
		if (node.m_parent != InvalidIndex || !node.m_children.empty())
			return InvalidIndex;

		const size_t node_index = m_nodes.size();
		m_nodes.push_back( node );

		if (parent == InvalidIndex)
		{
			m_roots.push_back( node_index );
		}
		else
		{
			m_nodes[ parent ].m_children.push_back( node_index );
			m_nodes[ node_index ].m_parent = parent;
		}

		m_nodes[ node_index ].m_index = node_index;
		return node_index;
	}

	void Interface::remove_node( const index_t node_index ) {
		// invalid index
		if (node_index >= m_nodes.size())
			return;

		// gather and sort family
		vector<index_t> family = get_family( node_index );

		// is this needed?
		if (family.empty())
			return;

		// sorting so we start to erase elements closer to the back for performance reasons
		// also, if this isn't sorted, some tobe-deleted nodes will change their index
		// should be sorted from BIGGEST to SMALLEST
		std::sort( family.begin(), family.end(), []( const index_t a, const index_t b ) { return a < b; } );

		const index_t lowest_removed_node = family.back();

		// remove the family
		for (const index_t n : family)
		{
			m_nodes[ n ].m_index = npos;
			m_nodes[ n ].m_parent = npos;
			m_nodes[ n ].m_children.clear();
			m_nodes.erase( m_nodes.cbegin() + n );
		}

		// post removal of target nodes
		const size_t nodes_count = m_nodes.size();
		const size_t family_size = family.size();

		// update node's indices
		for (index_t i = lowest_removed_node; i < nodes_count; i++)
		{
			m_nodes[ i ].m_index = i;
		}


		// TODO ->>>
		const auto drop_steps = [ &family, family_size ]( const index_t index ) {
			for (size_t i = family_size - 1; i > 0; i--)
			{
				if (family[ i ] >= index)
					return family_size - i;
			}
			return family[ 0 ] < index ? family_size : family_size - 1;
			};

		// update node's children/parents
		for (index_t i = 0; i < nodes_count; i++)
		{
			if (m_nodes[ i ].m_parent > lowest_removed_node)
			{
				const index_t droped_value = drop_steps( m_nodes[ i ].m_parent );
				if (droped_value > m_nodes[ i ].m_parent)
				{
					//! BUG BUG, this block shouldn't be reached
				}
				m_nodes[ i ].m_parent -= droped_value;
			}

			for (index_t &child_index : m_nodes[ i ].m_children)
			{
				const index_t droped_value = drop_steps( child_index );
				if (droped_value > child_index)
				{
					//! if this block is executed, we might fucked up with the drop_steps lambda
				}
				child_index -= droped_value;
			}
		}

		// should we validate?
		//validate();
	}

	index_t Interface::transfer_branch( const this_type &old, index_t node_index, index_t parent ) {
		return InvalidIndex;
	}

	void Interface::validate() {
		const size_t nodes_count = m_nodes.size();

		// checking nodes
		for (index_t node_index = 0; node_index < nodes_count; node_index++)
		{
			if (m_nodes[ node_index ].m_index != node_index)
			{
				//! something fucked up happened
			}

			if (m_nodes[ node_index ].m_parent != npos && m_nodes[ node_index ].m_parent >= nodes_count)
			{
				//! invalid index for parent
			}

			// checking children
			for (const index_t child_index : m_nodes[ node_index ].m_children)
			{
				if (child_index >= nodes_count)
				{
					//! invalid child index
				}

				if (child_index == node_index)
				{
					//! node is it's own child
				}

				if (m_nodes[ child_index ].m_parent != node_index)
				{
					//! child thinks he is an orphan/root
				}

			}

		}

		// checking root nodes with extra steps
		for (const index_t root_node_index : m_roots)
		{
			if (root_node_index >= nodes_count)
			{
				//! invalid index
			}

			if (m_nodes[ root_node_index ].m_parent != npos)
			{
				//! how is it a root with a parent?
			}

		}

	}

	vector<index_t> &&Interface::get_family( const index_t node_index ) const {
		vector<index_t> indices{};
		vector<index_t> tobe_processed{};
		tobe_processed.push_back( node_index );

		const size_t nodes_count = m_nodes.size();
		size_t limit = 0;
		while (!tobe_processed.empty())
		{
#ifdef _DEBUG
			if (++limit > 1000'000)
			{
				// maybe a child has the parent as a child, creating a cycle reference
				break;
			}
#endif // _DEBUG
			const size_t n = tobe_processed.back();
			tobe_processed.pop_back();

			// invalid node index
			if (n >= nodes_count)
				continue;

			if (!m_nodes[ n ].m_children.empty())
			{
				tobe_processed.insert( tobe_processed.cend(), m_nodes[ n ].m_children.cbegin(), m_nodes[ n ].m_children.cend() );
			}

			indices.push_back( n );
		}


		return std::move( indices );
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

