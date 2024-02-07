// Zaher Abdullatif babker (C) 2023-2024
#pragma once

// GUI base on IGLIB (which is based on OpenGL)
// Requires iglib to link
// 
// NOTES:
//   No port of this library will target MacOS
//   C++ Exclusive

#include <yvals.h>
#if !_HAS_CXX17
#error IGUI currently supports C++17 or later versions only
#endif

#include <iostream>
#include <iglib.h>
#include <optional>

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
	typedef size_t uintptr_t;
	typedef intptr_t offset_t;

	constexpr index_t npos = (index_t)-1;
	constexpr index_t InvalidIndex = npos;


	using Trnsf = ig::Transform2D;
	using Vec2f = ig::Vector2f;
	using Vec2i = ig::Vector2i;
	using Rectf = ig::Rect2f;
	using Recti = ig::Rect2i;
	using Color = ig::Colorf;

	using Texture = ig::Texture;
	using Font = ig::Font;

	using Window = ig::Window;
	using Renderer = ig::Renderer;

	// FIXME: literals will break if 'string' requires a wide string
	using string = std::string;
	template <typename _T>
	using vector = std::vector<_T>;

	struct InputEvent
	{
		ig::InputEvent input;
		ig::InputEventType type;
	};

	using KeyCode = ig::KeyCode;
	using MouseButton = ig::MouseButton;

	class Node;
	class NodeFactory;
	class Context;

	enum class NodeType : u16
	{
		None = 0,
		ReferenceBox, // <- unfilled box
		Panel,

		// buttons
		Button,
		RadialButton,
		Checkbox, // [X] or [_]
		CheckButton, // [1--] or [--O]
		DropdownMenu, // <- dropdown button, labels and sliders
		DropdownOption, // <- dropdown selection

		// ranges
		Slider,
		ProgressBar,

		Sprite,
		Line,
		SolidColor,
		TextInput
	};

	enum StateMask : u64
	{
		StateMask_Enabled = 0x0000'0001,
		StateMask_Visible = StateMask_Enabled,

		StateMask_Hovered = 0x0000'0002,

		StateMask_Pressed = 0x0000'0004,
		StateMask_Activated = StateMask_Pressed,

		//StateMask_Activated = 0x0000'0004,

		StateMask_Value = 0xffff'ffff'0000'0000,
		StateMask_FPValue = StateMask_Value,
	};

	enum class MouseFilter : i16
	{
		// stops the mouse input after receiving it
		Stop,
		// detects the mouse, but passes the mouse event only to children (non-recursive)
		Children,
		// detects the mouse, but doesn't catch any input, letting it propagate
		Through,
		// ignores the mouse all together
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

	enum SizeFlags : i16
	{
		SizeFlags_None = 0x0000,
		SizeFlags_Expand = 0x0001,
		SizeFlags_Fill = 0x0002,
		SizeFlags_End = 0x0004,
	};

	struct StyleElement
	{
		inline constexpr StyleElement() : color{ 1.f, 1.f, 1.f }, texture{ nullptr } {
		}

		inline constexpr StyleElement( const Color &clr ) : color{ clr }, texture{ nullptr } {
		}

		inline constexpr StyleElement( const Texture *tex ) : color{ 1.f, 1.f, 1.f }, texture{ tex } {
		}

		inline constexpr StyleElement( const Color &clr, const Texture *tex ) : color{ clr }, texture{ tex } {
		}

		inline StyleElement &operator=( const Color &clr ) {
			color = clr;
			return *this;
		}

		inline StyleElement &operator=( const Texture *tex ) {
			texture = tex;
			return *this;
		}

		Color color;
		const Texture *texture;
	};

	struct InterfaceStyle
	{
		template <typename _T>
		using optional = std::optional<_T>;
		static constexpr std::nullopt_t novalue{ std::nullopt_t::_Tag() };
		// why is this fixed? idk, a bad decision for later
		static constexpr size_t max_custom_styles = 256;

		struct Text
		{
			inline Text() : color{ 1.f, 1.f, 1.f }, font{} {
			}

			inline Text( Color pcolor ) : color{ pcolor }, font{} {
			}

			inline Text( const Font *pfont ) : color{ 1.f, 1.f, 1.f }, font{ pfont } {
			}

			inline Text( Color pcolor, const Font *pfont ) : color{ pcolor }, font{ pfont } {
			}

			Color color;
			const Font *font;
		};

		struct Shadow
		{
			inline Shadow() : offset{}, color{} {
			}

			inline Shadow( Vec2f poffset ) : offset{ poffset }, color{} {
			}

			inline Shadow( Color pcolor ) : offset{}, color{ pcolor } {
			}

			inline Shadow( Vec2f poffset, Color pcolor )
				: offset{ poffset }, color{ pcolor } {
			}

			Vec2f offset;
			Color color;
		};

		struct Boarder
		{
			float left = 0, top = 0, right = 0, bottom = 0;
			StyleElement style = StyleElement();

			inline Boarder() {
			}

			inline Boarder( float size, Color clr )
				: left{ size }, top{ size }, right{ size }, bottom{ size }, style{ clr } {
			}

			inline Boarder( float size )
				: Boarder( size, { 1.f, 1.f, 1.f } ) {
			}

			inline void expand( float offset ) {
				left += offset;
				top += offset;
				right += offset;
				bottom += offset;
			}

			inline void set( float size ) {
				left = (top = (right = (bottom = size)));
			}

		};

		// TODO: organize more...
		struct Style
		{
			optional<StyleElement> background = novalue;
			optional<StyleElement> foreground = novalue;
			optional<Boarder> boarder = novalue;
			optional<Shadow> shadow = novalue;
			optional<Text> text = novalue;

			optional<StyleElement> hovered = novalue; // fallback to background
			optional<Boarder> hovered_boarder = novalue;  // fallback to boarder
			optional<Shadow> hovered_shadow = novalue; // fallback to shadow
			optional<Text> hovered_text = novalue; // fallback to text

			optional<StyleElement> activated = novalue; // fallback to background
			optional<Boarder> activated_boarder = novalue;  // fallback to boarder
			optional<Shadow> activated_shadow = novalue; // fallback to shadow
			optional<Text> activated_text = novalue; // fallback to text

			optional<StyleElement> disabled = novalue; // fallback to background
			optional<Boarder> disabled_boarder = novalue;  // fallback to boarder
			optional<Shadow> disabled_shadow = novalue; // fallback to shadow
			optional<Text> disabled_text = novalue; // fallback to text
		};

		using StyleArray = std::array<Style, max_custom_styles>;

		// base field's values should always exist
		// FIXME: how it's constructed is unreliable and buggy
		Style base = {
			Color( 0.14f, 0.14f, 0.14f ),   // bg
			Color( 0.94f, 0.94f, 0.94f ),   // fg
			Boarder( 2, Color( 0.2f, 0.2f, 0.2f ) ), // base boarder
			Shadow(), // base shadow
			Text( Color( 0.9f, 0.9f, 0.9f ) ), // text

			Color( 0.14f, 0.24f, 0.44f ),   // hovered
			Boarder( 2, Color( 0.4f, 0.4f, 0.4f ) ), // hovered boarder
			Shadow(), // hovered shadow
			Text(), // hovered text

			Color( 0.24f, 0.44f, 0.84f ),   // activated
			Boarder( 4, Color( 0.3f, 0.3f, 0.3f ) ), // activated boarder
			Shadow(), // activated shadow
			Text(), // activated text

			Color( 0.12f, 0.07f, 0.04f ),   // disabled
			Boarder( 0, Color() ), // disabled boarder
			Shadow(), // disabled shadow
			Text(), // disabled text
		};

		Style panel;
		Style button;

		// currently placeholders
		Style radial_button;

		Style checkbox;
		Style check_button;

		Style vertical_slider;
		Style horizontal_slider;

		Style vertical_progress_bar;
		Style horizontal_progress_bar;

		StyleArray custom_styles;
		size_t custom_styles_count = 0;
	};

	class Interface
	{
	public:
		struct DrawingStateCache;

		using this_type = Interface;
		using nodes_collection = vector<Node>;
		using nodes_indices = vector<index_t>;

		Interface();
		~Interface();

		void update();

		/// @brief draws the interface
		/// @param renderer the iglib renderer
		/// @note should be called from the rendering callback
		void draw( Renderer *renderer ) const;

		void input( InputEvent event );

		/// @brief to add a node it must be singlet (no parent or children) OR this will fail
		/// @brief if 'parent' is set to [npos] the node will be a root node
		/// @error [npos] will be returned
		index_t add_node( const Node &node, index_t parent = npos );

		/// @brief moves the child up or down through it's siblings, useful for reordering children
		/// @param new_index the new between the siblings, clamped down if it's out of bounds
		void move_child( index_t child, index_t new_index );

		/// @brief moves the child to a diffren parent, or makes the child a root node
		/// @brief if the parent index is `npos`
		/// @param parent the new parent's index, pass `npos` to make the a child a root node
		void reparent_child( index_t child, index_t parent );

		/// @brief remove the node at [node_index], removing it's children too
		/// @brief will change the index of some nodes so be careful
		/// @param node_index the node index to remove, invalid indices will be ignored
		void remove_node( const index_t node_index );

		/// @brief to transfer the branch node (the node at 'node_index') an it children
		/// @brief the branch node will be parented to 'parent'
		/// @brief if 'parent' is set to [npos] the node will be a root node
		/// @error [npos] will be returned
		index_t transfer_branch( const this_type &old, index_t node_index, index_t parent = npos );

		template <typename _PRED>
		inline index_t find_node( _PRED predicate ) const;

		inline Node &get_node( index_t i ) {
			return m_nodes[ i ];
		}

		inline const Node &get_node( index_t i ) const {
			return m_nodes[ i ];
		}

		inline const nodes_collection &get_nodes() const {
			return m_nodes;
		}

		DrawingStateCache *get_drawing_sc();
		const DrawingStateCache *get_drawing_sc() const;

		/// @brief checking if anything is not 'normal'
		void validate();

	private:
		vector<index_t> get_family( const index_t node_index ) const;

	private:
		class NodeTree;
		class InputRecord;
		class StyleData;
		// internal structure
		struct SPDrawingStateCache
		{
			// FOR USERS: Changing this value from 2048 will CORRUPT the STACK
			static constexpr size_t AllocationSize = 2048;
			std::array<i32, AllocationSize / 4> memory = {};
		};

		size_t m_ticks;
		nodes_collection m_nodes;
		nodes_indices m_roots;
		std::unique_ptr<NodeTree> m_tree;
		std::unique_ptr<StyleData> m_style_data;
		std::unique_ptr<InputRecord> m_input;
		mutable SPDrawingStateCache m_sp_drawing_sc;
		mutable DrawingStateCache *m_drawing_sc;
	};

	typedef void(*NodeSignalProc)(Node *node, uintptr_t action);
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

		inline const vector<index_t> &get_children() const noexcept {
			return m_children;
		}
		
		void disable();
		void enable();
		void hide();
		void show();
		void set_visibilty( bool visible );
		void set_position( Vec2f pos );
		void set_size( Vec2f size );
		void set_rect( Vec2f pos, Vec2f size );
		void set_rect( const Rectf &rect );
		void set_anchors( const Rectf &anchors );
		void set_anchors( float left, float right, float top, float bottom );
		void set_mouse_filter( MouseFilter filter );
		void set_pivot( Vec2f pivot );
		void set_angle( float angle );

		inline Vec2f get_position() const noexcept {
			return { m_rect.x, m_rect.y };
		}

		inline Vec2f get_size() const noexcept {
			return { m_rect.h, m_rect.w };
		}

		inline const Rectf &get_rect() const noexcept {
			return m_rect;
		}

		inline const Rectf &get_anchors() const noexcept {
			return m_anchors;
		}

		inline Vec2f get_pivot() const noexcept {
			return m_pivot;
		}

		inline float get_angle() const noexcept {
			return m_angle;
		}

		inline index_t index() const noexcept {
			return m_index;
		}


		inline void set_signal_proc( const NodeSignalProc proc ) noexcept {
			m_signal_proc = proc;
		}

		inline NodeSignalProc get_signal_proc() const noexcept {
			return m_signal_proc;
		}

		inline MouseFilter get_mouse_filter() const noexcept {
			return m_mouse_filter;
		}

		inline bool is_enabled() const noexcept {
			return m_state & StateMask_Enabled;
		}

		inline bool is_disabled() const noexcept {
			return !(m_state & StateMask_Enabled);
		}

		inline bool is_visible() const noexcept {
			return m_state & StateMask_Visible;
		}

	private:
		NodeType m_type;
		index_t m_index;

		mutable u64 m_state = StateMask_Enabled;

		u32 m_flags = 0;
		Rectf m_old_rect = { 0.f, 0.f, 32.f, 32.f };
		Rectf m_rect = { 0.f, 0.f, 32.f, 32.f }; // <- relative rect to it's parent
		bool m_rect_dirty = false;
		Rectf m_anchors = { 0.f, 0.f, 0.f, 0.f };
		Vec2f m_pivot = { 0.f, 0.f };
		float m_angle = 0.0f;

		MouseFilter m_mouse_filter = MouseFilter::Stop;
		CursorShape m_cursor_shape = CursorShape::Arrow;
		MouseButton m_trigger_button = MouseButton::Left;

		string m_text = "Sample text";
		string m_tooltip = "Sample tooltip"; // <- empty tooltip will not be displayed
		string m_desc;

		i16 m_text_align = 0;

		struct {
			SizeFlags horizontal = SizeFlags_None;
			SizeFlags vertical = SizeFlags_None;
		} m_size_flags;

		NodeSignalProc m_signal_proc = nullptr;

		vector<index_t> m_children = {};
		index_t         m_parent = npos;
	};

	template<typename _PRED>
	inline index_t Interface::find_node( _PRED predicate ) const {
		const size_t nodes_count = m_nodes.size();
		for (index_t i = 0; i < nodes_count; i++)
		{
			if (predicate( m_nodes[ i ] ))
				return i;
		}
		return npos;
	}

	inline i32 Node::value() const noexcept {
		return (m_state & StateMask_Value) >> 32;
	}

	inline float Node::value_fp() const noexcept {
		const i32 val = value();
		// kind hacky but it should work...
		return *reinterpret_cast<const float *>(&val);
	}

}
