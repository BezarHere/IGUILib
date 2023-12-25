// Zaher Abdullatif babker (C) 2023-2024
#pragma once

// GUI base on IGLIB (which is based on OpenGL)
// Requires iglib to link
// 
// NOTES:
//   No port of this library will target MacOS
//   C++ Exclusive

// ROADMAP:
//   Add more extensibility to drawing

#include <iglib.h>

namespace igui
{
	// for extensibility, use own type names

	typedef uint8_t u8;
	typedef int8_t i8;
	typedef uint16_t u16;
	typedef int16_t i16;
	typedef uint32_t u32;
	typedef int32_t i32;
	typedef uint64_t u64;
	typedef int64_t i64;

	typedef size_t index_t;
	constexpr index_t InvalidIndex = (index_t)-1;

	
	using Trnsf = ig::Transform2D;
	using Vec2f = ig::Vector2f;
	using Vec2i = ig::Vector2i;
	using Rectf = ig::Rect2f;
	using Recti = ig::Rect2i;
	using Color = ig::Colorf;

	using Window = ig::Window;
	using Renderer = ig::Renderer;

	// FIXME: literals will break if 'string' requires a wide string
	using string = std::string; 
	template <typename _T>
	using vector = std::vector<_T>;


	class Node;
	class NodeFactory;
	class Context;

	enum class NodeType : u8
	{
		None = 0,
		Button,
		RadialButton,
		Label,

	};

	enum StateMask : i64
	{
		StateMask_Enabled = 0x00'00'00'01,

		StateMask_Value   = 0xff'ff'00'00,
		StateMask_FPValue = StateMask_Value,
	};

	enum class MouseFilter : i16
	{
		Stop,
		Through,
		Ignore
	};

	enum class CursorShape : i16
	{
		Arrow,
		Move,
		ResizeHorizontal,
		ResizeVertical,
		Resize,
		PointerHand,
	};

	class NodeFactory
	{
	public:



	private:
		NodeFactory() = delete;
		~NodeFactory() = delete;
	};

	enum SizeFlags : i16
	{
		SizeFlags_None = 0
	};

	struct Style
	{

	};

	class Interface
	{
	public:
		using this_type = Interface;

		void update();
		void draw(const Window *window, Renderer *renderer) const;

		// to add a node it must be singlet (no parent or children) OR this will fail
		// if 'parent' is set to InvalidIndex the node will be a root node
		// [On Error] InvalidIndex will be returned
		index_t add_node( const Node &node, index_t parent = InvalidIndex );

		// to transfer the branch node (the node at 'node_index') an it children
		// the branch node will be parented to 'parent'
		// if 'parent' is set to InvalidIndex the node will be a root node
		// [On Error] InvalidIndex will be returned
		index_t transfer_branch( const this_type &old, index_t node_index, index_t parent = InvalidIndex );

		template <typename _PRED>
		inline index_t find_node( _PRED predicate ) const;

		inline Node &get_node( index_t i ) {
			return m_nodes[ i ];
		}

		inline const Node &get_node( index_t i ) const {
			return m_nodes[ i ];
		}

		inline const vector<Node> &get_nodes() const {
			return m_nodes;
		}

	private:
		vector<Node> m_nodes;
		vector<index_t> m_roots;
		Style m_style;
	};

	class Node
	{
		friend Interface;
	public:
		Node();
		Node( NodeType type );

		inline i32 value() const noexcept;
		inline float value_fp() const noexcept;

		inline bool enabled() const noexcept {
			return m_state & StateMask_Enabled;
		}

		inline i64 state() const noexcept {
			return m_state;
		}

	private:
		NodeType m_type;
		i64 m_state        = StateMask_Enabled;
		i32 m_flags        = 0;
		Rectf m_rect       = { 0.f, 0.f, 32.f, 32.f };
		Rectf m_anchors    = { 0.f, 0.f, 1.f, 1.f };
		Vec2f m_pivot      = { 0.f, 0.f };
		float m_angle      = 0.0f;

		MouseFilter m_mouse_filter = MouseFilter::Stop;
		CursorShape m_cursor_shape = CursorShape::Arrow;

		string m_text      = "Sample text";
		string m_tooltip   = "Sample tooltip"; // <- empty tooltip will not be displayed

		struct {
			SizeFlags horizontal = SizeFlags_None;
			SizeFlags vertical   = SizeFlags_None;
		} m_size_flags;

		vector<index_t> m_children = {};
		index_t         m_parent   = InvalidIndex;
	};

	template<typename _PRED>
	inline index_t Interface::find_node( _PRED predicate ) const {
		const size_t nodes_count = m_nodes.size();
		for (index_t i = 0; i < nodes_count; i++)
		{
			if (predicate( m_nodes[ i ] ))
				return i;
		}
		return InvalidIndex;
	}

	inline i32 Node::value() const noexcept {
		return m_state & StateMask_Value;
	}

	inline float Node::value_fp() const noexcept {
		const i32 val = value();
		return *reinterpret_cast<const float *>(&val); // <- it works
	}

}
