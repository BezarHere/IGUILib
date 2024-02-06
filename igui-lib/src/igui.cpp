#include "pch.h"
#include "igui.h"
using namespace igui;

#include "internal.h"

using DrawingStateCache = Interface::DrawingStateCache;

#if IGUI_IMPL == IGUI_IGLIB
template <size_t _SZ>
using Vertex2Array = ig::BaseVertexArray<ig::Vertex2, stacklist<ig::Vertex2, _SZ>>;
#endif

enum class InputActionState
{
	None = 0,
	Hovered,
	Actived,
	Down = Actived,
	Deactivated,
	Up = Deactivated,
};

enum class MouseCapturedState
{
	Free,
	CapturedToFamily,
	Captured,
};

template <typename _OPTIONAL>
static FORCEINLINE const _OPTIONAL::value_type &get_value_cascade( const _OPTIONAL &val ) {
	return val.value();
}

#pragma warning(push)
#pragma warning(disable:4172)
// NOTE: Optimizing this is a priority
template <typename _OPTIONAL, typename... _OPTIONAL_REST>
static FORCEINLINE const _OPTIONAL::value_type &get_value_cascade( const _OPTIONAL &val, const _OPTIONAL_REST &...rest ) {
	return val.value_or( get_value_cascade( rest... ) );
}
#pragma warning(pop)


template <typename _OPTIONAL>
static FORCEINLINE _OPTIONAL::value_type &get_value_cascade( _OPTIONAL &val ) {
	return val.value();
}

// NOTE: Optimizing this is a priority
template <typename _OPTIONAL, typename... _OPTIONAL_REST>
static FORCEINLINE _OPTIONAL::value_type &get_value_cascade( _OPTIONAL &val, _OPTIONAL_REST &...rest ) {
	return val.value_or( get_value_cascade( rest ) );
}

// priority to optimize
template <typename... _STYLES>
static inline void transfer_style( InterfaceStyle::Style &out, const _STYLES &... bases ) {
	out.background = get_value_cascade( (bases.background)... );
	out.foreground = get_value_cascade( (bases.foreground)... );

	out.boarder = get_value_cascade( (bases.boarder)... );
	out.shadow = get_value_cascade( (bases.shadow)... );
	out.text = get_value_cascade( (bases.text)... );

	out.hovered = get_value_cascade( (bases.hovered)... );
	out.hovered_boarder = get_value_cascade( (bases.hovered_boarder)... );
	out.hovered_shadow = get_value_cascade( (bases.hovered_shadow)... );
	out.hovered_text = get_value_cascade( (bases.hovered_text)... );

	out.activated = get_value_cascade( (bases.activated)... );
	out.activated_boarder = get_value_cascade( (bases.activated_boarder)... );
	out.activated_shadow = get_value_cascade( (bases.activated_shadow)... );
	out.activated_text = get_value_cascade( (bases.activated_text)... );

	out.disabled = get_value_cascade( (bases.disabled)... );
	out.disabled_boarder = get_value_cascade( (bases.disabled_boarder)... );
	out.disabled_shadow = get_value_cascade( (bases.disabled_shadow)... );
	out.disabled_text = get_value_cascade( (bases.disabled_text)... );
}


/// @returns true on success, false otherwise
// priority to optimize
static inline bool build_style_image( const InterfaceStyle &style, InterfaceStyle &out ) {
#define CHECK_STYLE_BASE(name) if (!style.base.## name) { std::cout << "InterfaceStyle base style has no value: " << #name << '\n'; return false; }
	CHECK_STYLE_BASE( background );
	CHECK_STYLE_BASE( foreground );
	CHECK_STYLE_BASE( boarder );
	CHECK_STYLE_BASE( shadow );

	CHECK_STYLE_BASE( hovered );
	CHECK_STYLE_BASE( hovered_boarder );
	CHECK_STYLE_BASE( hovered_shadow );
	CHECK_STYLE_BASE( hovered_text );

	CHECK_STYLE_BASE( activated );
	CHECK_STYLE_BASE( activated_boarder );
	CHECK_STYLE_BASE( activated_shadow );
	CHECK_STYLE_BASE( activated_text );

	CHECK_STYLE_BASE( disabled );
	CHECK_STYLE_BASE( disabled_boarder );
	CHECK_STYLE_BASE( disabled_shadow );
	CHECK_STYLE_BASE( disabled_text );
#undef CHECK_STYLE_BASE
	using Style = InterfaceStyle::Style;

	// bases don't need a transfer
	out.base = style.base;

	transfer_style( out.panel, style.panel, style.base );
	transfer_style( out.button, style.button, style.base );

	transfer_style( out.radial_button, style.radial_button, style.button, style.base );

	transfer_style( out.checkbox, style.checkbox, style.button, style.base );
	transfer_style( out.check_button, style.check_button, style.button, style.base );

	// TODO: add the rest of the styles or make something better

	out.custom_styles_count =
		style.custom_styles_count < style.custom_styles.size() ? style.custom_styles_count : style.custom_styles.size();

	for (size_t i = 0; i < out.custom_styles_count; i++)
	{
		transfer_style( out.custom_styles[ i ], style.custom_styles[ i ], style.base );
	}

	return true;
}

// HNF
enum HierarchyNodeFlags : uint16_t
{
	HNodeFlag_None = 0x0000,
	HNodeFlag_CapturedMouse = 0x0001,
};

// NHNF
enum NonHierarchyNodeFlags : uint16_t
{
	NHNodeFlag_None = 0x0000,

};


class Interface::NodeTree
{
public:
	struct ParentNodeConnection
	{
		index_t next_node;
		Vec2f position; // <- global position
		Color modulate;
		HierarchyNodeFlags flags; // <- inherited
		NonHierarchyNodeFlags own_flags; // <- NOT inherited
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

	inline ParentNodeConnection *get_connection() {
		if (m_connections.empty())
			return nullptr;

		return &m_connections.back();
	}

	inline const ParentNodeConnection *get_connection() const {
		if (m_connections.empty())
			return nullptr;

		return &m_connections.back();
	}

	// this function must not be called with no nodes
	inline index_t next_node() {

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
				m_connections.emplace_back(
					next_node_index,
					Vec2f( node.m_rect.x, node.m_rect.y ),
					Color( 1.f, 1.f, 1.f, 1.f ),
					m_connections.empty() ? HierarchyNodeFlags::HNodeFlag_None : m_connections.back().flags
				);
			}


			

			for (nodes_indices::const_reverse_iterator E = node.m_children.rbegin(); E != node.m_children.rend(); E++)
			{
				m_nodes.push_back( *E );
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
		bool handled = false;
	};

	inline InputRecord() : m_tick{ 0 } {
		m_values.resize( m_values.capacity );
	}

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

	inline bool is_pressed( const KeyCode key ) const {
		return m_values[ static_cast<size_t>(key) + key_offset ].state.active;
	}

	inline bool is_just_released( const KeyCode key ) const {
		return m_values[ static_cast<size_t>(key) + key_offset ].update_tick == m_tick && !m_values[ static_cast<size_t>(key) + key_offset ].state.active;
	}

	inline bool is_just_pressed( const KeyCode key ) const {
		return m_values[ static_cast<size_t>(key) + key_offset ].update_tick == m_tick && m_values[ static_cast<size_t>(key) + key_offset ].state.active;
	}


	inline bool is_pressed( const MouseButton button ) const {
		return m_values[ static_cast<size_t>(button) + mbutton_offset ].state.active;
	}

	inline bool is_just_released( const MouseButton button ) const {
		return m_values[ static_cast<size_t>(button) + mbutton_offset ].update_tick == m_tick && !m_values[ static_cast<size_t>(button) + mbutton_offset ].state.active;
	}

	inline bool is_just_pressed( const MouseButton button ) const {
		return m_values[ static_cast<size_t>(button) + mbutton_offset ].update_tick == m_tick && m_values[ static_cast<size_t>(button) + mbutton_offset ].state.active;
	}

	inline InputActionState get_action_state( const MouseButton button ) const {
		if (m_values[ static_cast<size_t>(button) + mbutton_offset ].state.active)
		{
			return InputActionState::Actived;
		}

		return m_values[ static_cast<size_t>(button) + mbutton_offset ].update_tick == m_tick ?
			InputActionState::Deactivated : InputActionState::None;
	}

	inline InputActionState get_action_state( const KeyCode key ) const {
		if (m_values[ static_cast<size_t>(key) + key_offset ].state.active)
		{
			return InputActionState::Actived;
		}

		return m_values[ static_cast<size_t>(key) + key_offset ].update_tick == m_tick ?
			InputActionState::Deactivated : InputActionState::None;
	}

	inline void mark_as_handled( const MouseButton button ) {
		if (m_values[ static_cast<size_t>(button) + mbutton_offset ].handled)
		{
			return;
		}

		m_values[ static_cast<size_t>(button) + mbutton_offset ].handled = true;
		m_handled_input.push_back( static_cast<size_t>(button) + mbutton_offset );
	}

	inline void mark_as_handled( const KeyCode key ) {
		if (m_values[ static_cast<size_t>(key) + key_offset ].handled)
		{
			return;
		}

		m_values[ static_cast<size_t>(key) + key_offset ].handled = true;
		m_handled_input.push_back( static_cast<size_t>(key) + key_offset );
	}


	inline bool is_handled( const MouseButton button ) {
		return m_values[ static_cast<size_t>(button) + mbutton_offset ].handled;
	}

	inline bool is_handled( const KeyCode key ) {
		return m_values[ static_cast<size_t>(key) + key_offset ].handled;
	}

	inline void clear_handled_inputs() {
		for (const index_t input_index : m_handled_input)
		{
			m_values[ input_index ].handled = false;
		}
		m_handled_input.clear();
	}

	Vec2f mouse_pos = {}; // transformed
	Vec2f raw_mouse_pos = {}; // raw
private:
	size_t m_tick = 0;
	stacklist<InputValue, RecordSize> m_values;
	stacklist<index_t, RecordSize> m_handled_input;
};

class Interface::StyleData
{
public:

	/// @returns true on success, otherwise false
	inline bool invalidate_image() const {
#ifdef _DEBUG
		m_image_invalidation_count++;
#endif // _DEBUG
		return build_style_image( m_style, m_image );
	}

	inline InterfaceStyle &get_style() noexcept {
		m_dirty_image = true;
		return m_style;
	}

	inline const InterfaceStyle &get_style() const noexcept {
		return m_style;
	}

	inline const InterfaceStyle &get_image() const noexcept {
		if (m_dirty_image)
		{
			invalidate_image();
			m_dirty_image = false;
		}
		return m_image;
	}


private:
	InterfaceStyle m_style;

	mutable size_t m_image_invalidation_count = 0;
	mutable bool m_dirty_image = true;
	mutable InterfaceStyle m_image;
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
		: m_drawing_sc{ nullptr }, m_ticks{ 0 },
		m_input{ new InputRecord() }, m_style_data{ new StyleData() } {

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

				// TODO: skip this if the node has a layout, and order acording to the layout


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

		m_input->update();
		m_ticks++;
	}

	void Interface::draw( Renderer *renderer ) const {
		using ParentNodeConnection = NodeTree::ParentNodeConnection;
		static const ParentNodeConnection DefaultCPNC = { 0 };
		static ParentNodeConnection DefaultPNC = { 0 };

		if (!m_drawing_sc)
		{
			m_drawing_sc = new(reinterpret_cast<void *>(m_sp_drawing_sc.memory.data())) DrawingStateCache();
		}

		m_drawing_sc->start( renderer );

		m_input->raw_mouse_pos = Vec2f( renderer->get_window().get_mouse_position() );

		// i fucked the how the transforms are applied that applying 
		// the transform on the gpu is the inverted (or the reverse)
		// so this is the 'inverted transform' for now...
		const Trnsf inverted_transform = renderer->get_canvas().transform2d();

		m_input->mouse_pos = inverted_transform * m_input->raw_mouse_pos;

		renderer->get_canvas().circle( 8.f, m_input->mouse_pos, { 0.25f, 0.5f, 0.75f }, 8 );
		renderer->get_canvas().circle( 4.f, m_input->raw_mouse_pos, { 0.75f, 0.5f, 0.25f }, 8 );

		const InterfaceStyle &style_image = m_style_data->get_image();

		MouseCapturedState mouse_captured = MouseCapturedState::Free;
		NodeTree tree{ m_roots, m_nodes };
		index_t last_node = npos;

		/*
		FIXME: draw order is fine but update order is not workable

		updating/capturing inputs should be reverse order of drawing (children -> parents)
		drawing should be for children last to render them on top of parent (parents -> children)
		or reverse the depth test or some shit, just fix this mess
		*/

		for (index_t i = 0; tree; i = tree.next_node())
		{
			const Node &node = m_nodes[ i ];
			const ParentNodeConnection *con = ([]( const ParentNodeConnection *c ) { return c ? c : &DefaultCPNC; })(tree.get_connection());

			const Vec2f node_global_pos = { con->position.x + node.m_rect.x, con->position.y + node.m_rect.y };
			const Rectf node_global_rect = { node_global_pos.x, node_global_pos.y, node.m_rect.w, node.m_rect.h };


			const bool mouse_available =
				node.m_mouse_filter != MouseFilter::Ignore &&
				mouse_captured == MouseCapturedState::Free ||
				(mouse_captured == MouseCapturedState::CapturedToFamily && con->flags & HierarchyNodeFlags::HNodeFlag_CapturedMouse);

			if (mouse_available)
			{
				if (node.m_mouse_filter == MouseFilter::Stop)
				{
					mouse_captured = MouseCapturedState::Captured;
				}
				else if (mouse_captured != MouseCapturedState::CapturedToFamily && node.m_mouse_filter == MouseFilter::Children)
				{
					mouse_captured = MouseCapturedState::CapturedToFamily;
				}
			}

			const bool hovered = mouse_available && node_global_rect.contains( m_input->mouse_pos );

			// crappy
			const InputActionState _ics = m_input->get_action_state( node.m_trigger_button );
			const InputActionState mb_state = hovered ?
				(_ics == InputActionState::None ? InputActionState::Hovered : _ics) : InputActionState::None;

			node.m_state &= ~(StateMask_Hovered | StateMask_Pressed);
			if (hovered)
			{
				node.m_state |= StateMask_Hovered;
			}

			// can be but in if (hovered) but nesting is yucky
			if (mb_state == InputActionState::Actived)
			{
				node.m_state |= StateMask_Pressed;
			}

			// drawing the node
			switch (node.m_type)
			{
			case NodeType::Panel:
				{
					m_drawing_sc->apply_style_element( style_image.panel.background.value() );

					m_drawing_sc->gen_box( node_global_pos.x, node_global_pos.y, node_global_rect.w, node_global_rect.h );
					m_drawing_sc->draw_box();

					const auto boarder = style_image.panel.boarder.value();

					m_drawing_sc->apply_style_element( boarder.style );

					m_drawing_sc->gen_frame(
						{
							node_global_pos.x - boarder.left,
							node_global_pos.y - boarder.top,
							node_global_rect.w + boarder.left + boarder.right,
							node_global_rect.h + boarder.top + boarder.bottom
						},
						node_global_rect );
					m_drawing_sc->draw_frame();
				}
				break;
			case NodeType::Button:
				{
					m_drawing_sc->apply_style_element( style_image.button.background.value() );

					m_drawing_sc->gen_box( node_global_pos.x, node_global_pos.y, node_global_rect.w, node_global_rect.h );
					m_drawing_sc->draw_box();

					const InterfaceStyle::Boarder &boarder = std::invoke(
						[ &style_image, mb_state ]() {
							if (mb_state == InputActionState::Hovered)
							{
								return style_image.button.hovered_boarder.value();
							}

							if (mb_state == InputActionState::Actived)
							{
								return style_image.button.activated_boarder.value();
							}

							return style_image.button.boarder.value();
						} );


					m_drawing_sc->apply_style_element( boarder.style );

					m_drawing_sc->gen_frame(
						{
							node_global_pos.x - boarder.left,
							node_global_pos.y - boarder.top,
							node_global_rect.w + boarder.left + boarder.right,
							node_global_rect.h + boarder.top + boarder.bottom
						},
						node_global_rect );
					m_drawing_sc->draw_frame();
				}

				break;
			default:
				break;
			}

			// loop tail:

			last_node = i;
		}


	}

	void Interface::input( InputEvent event ) {
		m_input->input( event );
	}

	index_t Interface::add_node( const Node &node, index_t parent ) {
		// use add_branch() for this case, not add_node()
		if (node.m_parent != InvalidIndex || !node.m_children.empty())
			return InvalidIndex;


		const size_t node_index = m_nodes.size();
		m_nodes.push_back( node );

		// the node's rect should be clean
		m_nodes[ node_index ].m_rect_dirty = false;
		m_nodes[ node_index ].m_old_rect = m_nodes[ node_index ].m_rect;

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
				const index_t dropped_value = drop_steps( m_nodes[ i ].m_parent );
				if (dropped_value > m_nodes[ i ].m_parent)
				{
					//! BUG BUG, this block shouldn't be reached
				}
				m_nodes[ i ].m_parent -= dropped_value;
			}

			for (index_t &child_index : m_nodes[ i ].m_children)
			{
				const index_t dropped_value = drop_steps( child_index );
				if (dropped_value > child_index)
				{
					//! if this block is executed, we might fucked up with the drop_steps lambda
				}
				child_index -= dropped_value;
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

	vector<index_t> Interface::get_family( const index_t node_index ) const {
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


		return indices;
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

	void Node::set_rect( const Rectf &rect ) {
		m_rect = rect;

		if (!m_rect_dirty)
			m_rect_dirty = true;
	}

	void Node::set_anchors( const Rectf &anchros ) {
		m_anchors = anchros;
	}

	void Node::set_anchors( float left, float right, float top, float bottom ) {
		m_anchors.x = left;
		m_anchors.y = top;
		m_anchors.w = right - left;
		m_anchors.h = bottom - top;
	}

	void Node::set_mouse_filter( MouseFilter filter ) {
		m_mouse_filter = filter;
	}

	void Node::set_pivot( Vec2f pivot ) {
		m_pivot = pivot;
	}

	void Node::set_angle( float angle ) {
		m_angle = angle;
	}



}

