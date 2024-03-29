#include "pch.h"
#include "igui.h"
using namespace igui;

#include "internal.h"

#include <exception>
#include <array>

#if IGUI_IMPL == IGUI_IGLIB
template <size_t _SZ>
using Vertex2Array = ig::BaseVertexArray<ig::Vertex2, stacklist<ig::Vertex2, _SZ>>;
using Vertex2 = ig::Vertex2;
using LongIndexBuffer = ig::Index16Buffer;
using ShortIndexBuffer = ig::Index8Buffer;
using ig::BufferUsage;
using ig::PrimitiveType;
using std::array;
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
enum NoneHierarchyNodeFlags : uint16_t
{
	NHNodeFlag_None = 0x0000,

};

typedef uint16_t zindex_t;

struct ZIndexedVertex2
{
	Vec2f pos;
	Color clr;
	Vec2f uv;
	zindex_t zindex;
};

template <typename _T, size_t _SZ, size_t _REPC>
static constexpr std::array<_T, _REPC *_SZ> repeat_pattern( const std::array<_T, _SZ> &arr ) {
	static_assert(_REPC > 0, "can't repeat zero times!");

	std::array<_T, _REPC *_SZ> result;

	// copy pattern _REPC times
	for (size_t i = 0; i < _REPC; i++)
	{
		// copy pattern
		for (size_t j = 0; j < _SZ; j++)
		{
			result[ i * _SZ + j ] = arr[ j ];
		}
	}

	return result;
}

template <size_t _SZ, size_t _REPC>
static constexpr std::array<LongIndexBuffer::element_type, _REPC *_SZ>
indexbuffer_pattern( const std::array<LongIndexBuffer::element_type, _SZ> &arr, const size_t shape_vert_count ) {
	static_assert(_REPC > 0, "can't repeat zero times!");
	std::array<LongIndexBuffer::element_type, _REPC *_SZ> result{};

	if (shape_vert_count * (_REPC - 1) > (size_t)std::numeric_limits<LongIndexBuffer::element_type>::max())
	{
		//! OVERFLOW RISK
		return result;
	}

	// copy pattern _REPC times
	for (size_t i = 0; i < _REPC; i++)
	{
		// copy pattern
		for (size_t j = 0; j < _SZ; j++)
		{
			result[ i * _SZ + j ] = arr[ j ] + static_cast<LongIndexBuffer::element_type>(i * shape_vert_count);
		}
	}

	return result;
}

struct ZIndexedTextMeshBuilder
{
	static constexpr bool Indexed = true;
	using vertex_type = ZIndexedVertex2;

	inline ZIndexedTextMeshBuilder( vertex_type *vertices, const size_t p_count, const zindex_t p_zindex, const Vec2f &p_pos, const Color &p_clr )
		: m_vertices{ vertices }, count{ p_count }, zindex{ p_zindex }, pos{ p_pos }, clr{ p_clr } {

	}

	inline void operator()( size_t index, float pos_x, float pos_y, const Vec2f glyph_size, const Font::UVBox &uv_box ) {
		pos_x += pos.x;
		pos_y += pos.y;

		m_vertices[ index + 0 ].pos = { pos_x + glyph_size.x, pos_y + glyph_size.y };
		m_vertices[ index + 0 ].uv = uv_box.origin + uv_box.left + uv_box.bottom;
		m_vertices[ index + 0 ].clr = clr;
		m_vertices[ index + 0 ].zindex = zindex;

		m_vertices[ index + 1 ].pos = { pos_x, pos_y + glyph_size.y };
		m_vertices[ index + 1 ].uv = (uv_box.origin + uv_box.left);
		m_vertices[ index + 1 ].clr = clr;
		m_vertices[ index + 1 ].zindex = zindex;

		m_vertices[ index + 2 ].pos = { pos_x, pos_y };
		m_vertices[ index + 2 ].uv = uv_box.origin;
		m_vertices[ index + 2 ].clr = clr;
		m_vertices[ index + 2 ].zindex = zindex;

		m_vertices[ index + 3 ].pos = { pos_x + glyph_size.x, pos_y };
		m_vertices[ index + 3 ].uv = (uv_box.origin + uv_box.bottom);
		m_vertices[ index + 3 ].clr = clr;
		m_vertices[ index + 3 ].zindex = zindex;
	}

	const size_t count;
	const Vec2f pos;
	const Color clr;
	const zindex_t zindex;
private:
	vertex_type *m_vertices;
};

class Interface::RenderingFactory
{
public:
	static constexpr zindex_t start_zindex = 0xfffe;

	static constexpr size_t PerBoxVertices = 4;
	static constexpr size_t PerFrameVertices = 10;
	static constexpr size_t PerGlyphVertices = 4;

	static constexpr size_t IndicesPerBox = 6;
	static constexpr size_t IndicesPerFrame = IndicesPerBox * 4;
	static constexpr size_t IndicesPerGlyph = IndicesPerBox;

	static constexpr size_t IndexMaxValue = (1ull << (sizeof( LongIndexBuffer::element_type ) * 8)) - 1;

	static constexpr size_t RectPatchesMaxCount = 512;
	static constexpr size_t FramePatchesMaxCount = 2048;

	static constexpr size_t BoxPatchesCount =
		std::min<size_t>( (IndexMaxValue + 1) / PerBoxVertices, RectPatchesMaxCount );

	static constexpr size_t FramePatchesCount =
		std::min<size_t>( (IndexMaxValue + 1) / PerFrameVertices, FramePatchesMaxCount );

	static constexpr size_t TotalBoxVertices = PerBoxVertices * BoxPatchesCount;
	static constexpr size_t TotalFrameVertices = PerFrameVertices * FramePatchesCount;


	static constexpr size_t TextBufferVerticesCount =
		std::min<size_t>( 4096ULL, std::numeric_limits<LongIndexBuffer::element_type>::max() / IndicesPerGlyph );
	static constexpr size_t TextBufferSzInGlyphs = TextBufferVerticesCount / PerGlyphVertices;

	using TextVertexArray = Vertex2Array<TextBufferSzInGlyphs *Text::VerticesPerGlyph>;

	static inline const char *get_zindexed_vertex_shader_src() {
		static std::string test = "";
		if (test.empty())
		{
			std::ostringstream ss{};
			ss << "#version 330\n";
			ss << "layout ( location = 0 ) in vec2 a_pos;";
			ss << "layout ( location = 1 ) in vec4 a_clr;";
			ss << "layout ( location = 2 ) in vec2 a_texcoord;";
			ss << "layout ( location = 3 ) in float a_zindex;";
			ss << ig::Shader::get_default_source_part( ig::ShaderUsage::Usage2D, ig::ShaderSourcePart::VertexOutputs );
			ss << ig::Shader::get_default_source_part( ig::ShaderUsage::Usage2D, ig::ShaderSourcePart::VertexUniforms );
			ss << ig::Shader::get_default_source_part( ig::ShaderUsage::Usage2D, ig::ShaderSourcePart::VertexUtilityMethods );
			ss << "void main() {vec2 tpos = (_trans * a_pos) + _offset;gl_Position = vec4(to_native_space(tpos), a_zindex, 1.0);FragColor = a_clr;UV = a_texcoord;}";
			test = ss.str();
		}
		return test.c_str();
	}

	const struct RFIndexBuffers {

		using BulkBoxIndexBufferArray = std::array<LongIndexBuffer::element_type, IndicesPerBox *BoxPatchesCount>;
		using BoxIndexBufferArray = std::array<LongIndexBuffer::element_type, IndicesPerBox>;

		using BulkFrameIndexBufferArray = std::array<LongIndexBuffer::element_type, IndicesPerFrame *FramePatchesCount>;
		using FrameIndexBufferArray = std::array<LongIndexBuffer::element_type, IndicesPerFrame>;

		using BulkTextIndexBufferArray = std::array<LongIndexBuffer::element_type, IndicesPerGlyph *TextBufferSzInGlyphs>;
		using TextIndexBufferArray = std::array<LongIndexBuffer::element_type, IndicesPerGlyph>;

		static constexpr BulkBoxIndexBufferArray BoxI =
			indexbuffer_pattern<IndicesPerBox, BoxPatchesCount>( BoxIndexBufferArray{ 0, 1, 2, 2, 3, 0 }, 4 );


		static constexpr BulkFrameIndexBufferArray FrameI =
			indexbuffer_pattern<IndicesPerFrame, FramePatchesCount>(
				FrameIndexBufferArray{
					0, 1, 2, 2, 3, 0, // quad 0
					3, 2, 4, 4, 5, 3, // quad 1
					5, 4, 6, 6, 7, 5, // quad 2
					7, 6, 8, 8, 9, 7 // quad 3
				}, IndicesPerBox + 2 * 2 );


		static constexpr BulkTextIndexBufferArray TextI =
			indexbuffer_pattern<IndicesPerGlyph, TextBufferSzInGlyphs>( TextIndexBufferArray{ 0, 1, 2, 2, 3, 0 }, 4 );

		const LongIndexBuffer box = { std::size( BoxI ), ig::VBufferUsage::StaticDraw, BoxI.data() };
		const LongIndexBuffer frame = { std::size( FrameI ), ig::VBufferUsage::StaticDraw, FrameI.data() };
		const LongIndexBuffer text = { std::size( TextI ), ig::VBufferUsage::StaticDraw, TextI.data() };
	};

	RenderingFactory()
		: text_vbuffer{ TextBufferVerticesCount * sizeof( ZIndexedVertex2 ), ig::VBufferUsage::StreamDraw },
		text_vpd{} {
		m_shader.reset( ig::Shader::compile_raw( get_zindexed_vertex_shader_src(), nullptr, ig::ShaderUsage::Usage2D ).release() );

		text_vpd.set_stride( sizeof( ZIndexedVertex2 ) );
		{
			auto interface = text_vpd.create_interface();
			interface.attributes.push_back( { ig::VPDAttributeType::Float, ig::VPDAttributeSize::Vec2 } );
			interface.attributes.push_back( { ig::VPDAttributeType::Float, ig::VPDAttributeSize::Color } );
			interface.attributes.push_back( { ig::VPDAttributeType::Float, ig::VPDAttributeSize::Vec2 } );
			interface.attributes.push_back( { ig::VPDAttributeType::NormalizedUnsignedShort, ig::VPDAttributeSize::Single } );
		}
		text_vbuffer.create( TextBufferVerticesCount * sizeof( ZIndexedVertex2 ), ig::VBufferUsage::StreamDraw );
	}

	inline void dispatch_boxes();
	inline void dispatch_frames();
	inline void dispatch_text();

	inline void start( Renderer *const r ) {
		m_renderer = r;
		m_zindices.box = start_zindex;
		m_zindices.frame = start_zindex;
		m_zindices.text = start_zindex;

		// finish should flush m_box_build_index
		// flushing it sets it to zero
		if (m_box_build_index)
		{
			dispatch_boxes();
		}

		// same story as m_box_build_index
		if (m_frame_build_index)
		{
			dispatch_frames();
		}



		if (m_text_build_index)
		{
			dispatch_text();
		}
	}


	inline void finish() {
		if (m_box_build_index)
			dispatch_boxes();

		if (m_frame_build_index)
			dispatch_frames();

		if (m_text_build_index)
			dispatch_text();

	}

	inline void build_rect( const Rectf &rect );
	inline void build_frame( const Rectf &outer, const Rectf &inner );
	inline void build_text( const string &text, const Vec2f &pos, const Color &clr );

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

	inline void set_font( const Font *font ) {
		if (font == nullptr)
			font = &DefaultFont;

		if (font == m_font)
			return;
		dispatch_text();
		font = m_font;
	}

	const Font DefaultFont = Font::get_default();

	Vertex2Array<TotalBoxVertices> box_vertices{ ig::PrimitiveType::Triangle, TotalBoxVertices, ig::BufferUsage::Stream }; // quad
	Vertex2Array<TotalFrameVertices> frame_vertices{ ig::PrimitiveType::Triangle, TotalFrameVertices, ig::BufferUsage::Stream }; // quad strip

	ig::RawVertexBuffer text_vbuffer;
	array<ZIndexedVertex2, TextBufferVerticesCount> text_vertices{};
	ig::VertexPipelineDescriptor text_vpd;

	Vec2f text_scale = { 1.f, 1.f };
	RFIndexBuffers index_buffers;

private:
	inline void rect_draw_queued() {
		m_box_build_index++;
		if (m_box_build_index >= BoxPatchesCount)
		{
			dispatch_boxes();
		}
		m_zindices.box--;
	}
	inline void frame_draw_queued() {
		m_frame_build_index++;
		if (m_frame_build_index >= FramePatchesCount)
		{
			dispatch_frames();
		}
		m_zindices.frame--;
	}
	inline void text_draw_queued( const size_t glyphs_count ) {
		m_text_build_index += glyphs_count;
		if (m_text_build_index >= TextBufferVerticesCount)
		{
			dispatch_text();
		}
		m_zindices.text--;
	}

private:
	StyleElement m_active_style_element = {};
	bool m_dirty_color = true;
	Renderer *m_renderer = nullptr;

	size_t m_box_build_index = 0;
	size_t m_frame_build_index = 0;
	size_t m_text_build_index = 0;

	struct zindices
	{
		zindex_t box, frame, text;
	} m_zindices;

	const Font *m_font = &DefaultFont;
	std::unique_ptr<ig::Shader> m_shader;
};

#pragma region(rendering factory defs)

inline void
Interface::RenderingFactory::build_rect( const Rectf &rect ) {
	Vertex2 *const box_verts = box_vertices.get_vertices().data() + (m_box_build_index * PerBoxVertices);
	box_verts[ 0 ].pos.x = rect.x;
	box_verts[ 0 ].pos.y = rect.y;

	box_verts[ 1 ].pos.x = rect.x + rect.w;
	box_verts[ 1 ].pos.y = rect.y;

	box_verts[ 2 ].pos.x = rect.x + rect.w;
	box_verts[ 2 ].pos.y = rect.y + rect.h;

	box_verts[ 3 ].pos.x = rect.x;
	box_verts[ 3 ].pos.y = rect.y + rect.h;

	if (m_dirty_color)
	{
		for (size_t i = 0; i < PerBoxVertices; i++)
		{
			box_verts[ i ].clr = m_active_style_element.color;
		}
	}

	m_dirty_color = false;

	rect_draw_queued();
}

inline void
Interface::RenderingFactory::build_frame( const Rectf &outer, const Rectf &inner ) {
	const Rectf::vector_type outer_end = outer.end();
	const Rectf::vector_type inner_end = inner.end();
	Vertex2 *const frame_verts = frame_vertices.get_vertices().data() + (m_frame_build_index * PerFrameVertices);

	// quad 0
	frame_verts[ 0 ].pos = { outer.x, outer.y };
	frame_verts[ 1 ].pos = { inner.x, inner.y };
	frame_verts[ 2 ].pos = { inner_end.x, inner.y };
	frame_verts[ 3 ].pos = { outer_end.x, outer.y };

	// quad 1
	frame_verts[ 4 ].pos = inner_end;
	frame_verts[ 5 ].pos = outer_end;

	// quad 2
	frame_verts[ 6 ].pos = { inner.x, inner_end.y };
	frame_verts[ 7 ].pos = { outer.x, outer_end.y };

	// quad 3
	frame_verts[ 8 ].pos = frame_verts[ 1 ].pos;
	frame_verts[ 9 ].pos = frame_verts[ 0 ].pos;

	if (m_dirty_color)
	{
		for (size_t i = 0; i < PerFrameVertices; i++)
		{
			frame_verts[ i ].clr = m_active_style_element.color;
		}
	}

	m_dirty_color = false;

	frame_draw_queued();
}

inline void Interface::RenderingFactory::build_text( const string &text, const Vec2f &pos, const Color &clr ) {
	// the cached text buffer may not suffice...
	if (text_vertices.size() - m_text_build_index < text.length())
	{
		dispatch_text();
		if (text.length() > 1)
		{
			std::cerr << "\nIGUI: DISPLAING STRING LARGER THEN " << TextBufferSzInGlyphs << " IS CURRENTLY NOT SUPPORTED\n";
			return;
		}

	}

	ZIndexedTextMeshBuilder imb{ text_vertices.data() + m_text_build_index, text_vertices.size() - m_text_build_index, m_zindices.text, pos, clr };
	const size_t len = Text::build(
		imb, text, *m_font, text_scale, 4.f
	);

	text_draw_queued( len );
}

inline void
Interface::RenderingFactory::dispatch_boxes() {
	box_vertices.update( 0, m_box_build_index * PerBoxVertices );
	m_renderer->get_canvas().draw( box_vertices.get_buffer(), index_buffers.box );
	m_box_build_index = 0;
}

inline void
Interface::RenderingFactory::dispatch_frames() {
	frame_vertices.update( 0, m_frame_build_index * PerFrameVertices );
	m_renderer->get_canvas().draw( frame_vertices.get_buffer(), index_buffers.frame );
	m_frame_build_index = 0;

}

inline void
Interface::RenderingFactory::dispatch_text() {
	text_vbuffer.update_bytes( text_vertices.data(), 0, m_text_build_index * sizeof( ZIndexedVertex2 ) );

	m_renderer->bind_texture( m_font->get_atlas() );
	m_renderer->try_update_shader_state();

	const auto &p = RenderingFactory::RFIndexBuffers::BoxI;
	const auto &p2 = RenderingFactory::RFIndexBuffers::TextI;

	{
		//m_renderer->bind_shader( m_shader.get() );
		m_renderer->bind_shader( Font::get_shader().get() );
		text_vpd.bind();
		text_vbuffer.bind();
		index_buffers.text.bind();

		text_vpd.setup();

		// i NEED to know why mul the m_text_build_index by 2 fixes the text rendering problem??
		m_renderer->render( PrimitiveType::Triangle, m_text_build_index * 2, ig::VertexIndexType::Short, 0 );

		text_vpd.clear_setup();

		index_buffers.text.clear_bound();
		text_vbuffer.clear_bound();
		text_vpd.clear_bound();
		m_renderer->bind_default_shader( ig::ShaderUsage::Usage2D );
	}

	m_renderer->bind_texture( 0 );

	m_text_build_index = 0;
}

#pragma endregion

class Interface::NodeTree
{
public:
#pragma region(classes, structs and aliases)
	// the so called 'pnc', used for relations between nodes and their parents
	struct ParentNodeConnection
	{
		index_t owned_node; // <- what node is the 'parent'
		index_t parent_pnc; // <- connection to grandparent, can be npos
		Vec2f position = { 0.f, 0.f }; // <- global position
		Color modulate = { 1.f, 1.f, 1.f, 1.f };
		HierarchyNodeFlags flags = HierarchyNodeFlags::HNodeFlag_None; // <- inherited
		NoneHierarchyNodeFlags own_flags = NoneHierarchyNodeFlags::NHNodeFlag_None; // <- NOT inherited
	};
	using PNC = ParentNodeConnection;

	struct NodeEntry
	{
		index_t node_index;
		index_t pnc_index = npos; // <- npos for no pnc (e.g. root)
	};

	using node_entries = vector<NodeEntry>;
	using node_entries_iter = node_entries::reverse_iterator;
	using node_entries_citer = node_entries::const_reverse_iterator;
	using node_entries_riter = node_entries::iterator;
	using node_entries_criter = node_entries::const_iterator;

#pragma endregion


	inline NodeTree() {
		m_entries.reserve( 1024 );
		m_pncs.reserve( 1024 );
	}

	inline void regenerate( const Interface::nodes_indices &roots, const Interface::nodes_collection &nodes ) {
		m_entries.clear();
		m_pncs.clear();

		m_dirty = false;

		vector<NodeEntry> nodes_to_calculate{};
		for (int i = 0; i < roots.size(); i++)
			nodes_to_calculate.emplace_back( roots[ roots.size() - 1 - i ], npos );

		while (!nodes_to_calculate.empty())
		{
			const index_t node_index = nodes_to_calculate.back().node_index;
			const index_t parent_pnc_index = nodes_to_calculate.back().pnc_index;
			const Node &node = nodes[ node_index ];
			m_entries.push_back( nodes_to_calculate.back() );
			nodes_to_calculate.pop_back();


			if (!node.m_children.empty())
			{
				// add a pnc for this parent
				// setting only the owned node, other field will be set when required
				// setting them now might make them out of date when drawing/updating
				const index_t current_pnc_index = m_pncs.size();
				m_pncs.emplace_back( node_index, parent_pnc_index );


				for (auto E = node.m_children.rbegin(); E != node.m_children.rend(); E++)
				{
					nodes_to_calculate.emplace_back( *E, current_pnc_index );
				}
			}
		}

		update( nodes );
	}

	// updates the given pnc
	inline void update( const Interface::nodes_collection &nodes, PNC &pnc ) {
		// given how the pncs are generated, no pnc can be before one of it's parents
		// if a pnc comes before on of it's parents, the algorithm will fail

		if (pnc.parent_pnc != npos)
		{
			pnc.position = nodes[ pnc.owned_node ].m_rect.position() + m_pncs[ pnc.parent_pnc ].position;
			//pnc.modulate = ??
			pnc.flags = m_pncs[ pnc.parent_pnc ].flags;
		}
		else
		{
			pnc.position = nodes[ pnc.owned_node ].m_rect.position();
			//pnc.modulate = ??
		}
	}

	// updates all the pncs
	inline void update( const Interface::nodes_collection &nodes ) {
		// given how the pncs are generated, no pnc can be before one of it's parents
		// if a pnc comes before on of it's parents, the algorithm will fail
		for (PNC &pnc : m_pncs)
		{
			update( nodes, pnc );
		}
	}

	inline bool is_dirty() const noexcept {
		return m_dirty;
	}

	inline void mark_dirty() noexcept {
		m_dirty = true;
	}

	inline PNC &get_pnc( const index_t index ) {
		return m_pncs[ index ];
	}

	inline const PNC &get_pnc( const index_t index ) const {
		return m_pncs[ index ];
	}

	inline const NodeEntry &get_entry( const index_t index ) {
		return m_entries[ index ];
	}

	inline node_entries_iter begin() noexcept {
		// REVERSED
		return m_entries.rbegin();
	}

	inline node_entries_iter end() noexcept {
		// REVERSED
		return m_entries.rend();
	}

	inline node_entries_citer begin() const noexcept {
		// REVERSED
		return m_entries.rbegin();
	}

	inline node_entries_citer end() const noexcept {
		// REVERSED
		return m_entries.rend();
	}

	inline node_entries_riter rbegin() noexcept {
		// REVERSED
		return m_entries.begin();
	}

	inline node_entries_riter rend() noexcept {
		// REVERSED
		return m_entries.end();
	}

	inline node_entries_criter rbegin() const noexcept {
		// REVERSED
		return m_entries.begin();
	}

	inline node_entries_criter rend() const noexcept {
		// REVERSED
		return m_entries.end();
	}

private:
	bool m_dirty = true;
	vector<ParentNodeConnection> m_pncs = {};
	// currently iterated in reverse but should it be reversed instead?
	vector<NodeEntry> m_entries = {};
};

class Interface::InputManger
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
			Vec2i point;
		} state;
		index_t update_tick;
		bool handled = false;
	};

	inline InputManger() : m_tick{ 0 } {
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
				m_values[ mscroll_offset ].state.point.x = input.mouse_scroll.x;
				m_values[ mscroll_offset ].state.point.y = input.mouse_scroll.y;
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

	Node::Node()
		: m_type{ NodeType::None }, m_index{ npos }, m_layout{} {
	}

	Node::Node( NodeType type )
		: m_type{ type }, m_index{ npos }, m_layout{} {
	}

	Interface::Interface()
		: m_ticks{ 0 },
		m_drawing_sc{ new RenderingFactory() },
		m_input{ new InputManger() },
		m_style_data{ new StyleData() },
		m_tree{ new NodeTree() } {

	}

	Interface::~Interface() {
	}

	void Interface::update() {
		using NodeTreeEntry = Interface::NodeTree::NodeEntry;

		if (m_tree->is_dirty())
		{
			// also updates the pncs after generation
			rebuild();
		}

		for (NodeTreeEntry entry : *m_tree)
		{
			Node &node = m_nodes[ entry.node_index ];

			if (node.m_rect_dirty)
			{
				const float width_dt = node.m_rect.w - node.m_old_rect.w;
				const float height_dt = node.m_rect.h - node.m_old_rect.h;

				// TODO: skip this if the node has a layout, and order according to the layout


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
		const Font DefaultFont = Font::get_default();

		m_drawing_sc->start( renderer );

		m_input->raw_mouse_pos = Vec2f( renderer->get_window().get_mouse_position() );


		const Trnsf inverted_transform = renderer->get_canvas().transform2d().inverse();
		// FIXME: rotating transforms will make this fucked
		m_input->mouse_pos = inverted_transform * (m_input->raw_mouse_pos);


		renderer->get_canvas().circle( 8.f, m_input->mouse_pos, { 0.25f, 0.5f, 0.75f }, 8 );
		renderer->get_canvas().circle( 4.f, m_input->raw_mouse_pos, { 0.75f, 0.5f, 0.25f }, 8 );

		const InterfaceStyle &style_image = m_style_data->get_image();

		MouseCapturedState mouse_captured = MouseCapturedState::Free;
		index_t last_node = npos;

		m_tree->update( m_nodes );

		const ig::DepthTestComparison old_dtc = renderer->get_depth_test_comparison();
		const bool was_depthtest_enabled = renderer->is_feature_enabled( ig::Feature::DepthTest );
		renderer->set_depth_test_comparison( ig::DepthTestComparison::LessThen );
		if (!was_depthtest_enabled)
			renderer->enable_feature( ig::Feature::DepthTest );

		for (NodeTree::NodeEntry entry : *m_tree)
		{
			const Node &node = m_nodes[ entry.node_index ];
			const ParentNodeConnection &con = entry.pnc_index == npos ? DefaultPNC : m_tree->get_pnc( entry.pnc_index );

			const Vec2f node_global_pos = { con.position.x + node.m_rect.x, con.position.y + node.m_rect.y };
			const Rectf node_global_rect = { node_global_pos.x, node_global_pos.y, node.m_rect.w, node.m_rect.h };

			const bool mouse_available =
				node.m_mouse_filter != MouseFilter::Ignore &&
				mouse_captured == MouseCapturedState::Free ||
				(mouse_captured == MouseCapturedState::CapturedToFamily && con.flags & HierarchyNodeFlags::HNodeFlag_CapturedMouse);
			const bool hovered = mouse_available && node_global_rect.contains( m_input->mouse_pos );

			if (hovered)
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



			// crappy
			const InputActionState _ics = m_input->get_action_state( node.m_trigger_button );
			const InputActionState mb_state = hovered ?
				(_ics == InputActionState::None ? InputActionState::Hovered : _ics) : InputActionState::None;

			const auto old_state = node.m_state;
			node.m_state &= ~(StateMask_Hovered | StateMask_Pressed);
			if (hovered)
			{
				node.m_state |= StateMask_Hovered;
			}

			// can be in if (hovered) but nesting is yucky
			if (mb_state == InputActionState::Actived)
			{
				node.m_state |= StateMask_Pressed;
			}

			if (node.m_state & StateMask_Hovered && !(old_state & StateMask_Hovered))
			{
				node.trigger( SignalType::MouseEntered );
			}
			else if (!(node.m_state & StateMask_Hovered) && old_state & StateMask_Hovered)
			{
				node.trigger( SignalType::MouseExited );
			}

			// drawing the node
			switch (node.m_type)
			{
			case NodeType::Panel:
				{
					m_drawing_sc->apply_style_element( style_image.panel.background.value() );

					m_drawing_sc->build_rect( node_global_rect );

					const auto &boarder = style_image.panel.boarder.value();

					m_drawing_sc->apply_style_element( boarder.style );

					m_drawing_sc->build_frame(
						{
							node_global_pos.x - boarder.left,
							node_global_pos.y - boarder.top,
							node_global_rect.w + boarder.left + boarder.right,
							node_global_rect.h + boarder.top + boarder.bottom
						},
						node_global_rect );
				}
				break;
			case NodeType::Button:
				{

					if (mb_state == InputActionState::Actived)
					{
						m_drawing_sc->apply_style_element( style_image.button.activated.value() );
						if (!(old_state & StateMask_Activated))
						{
							node.trigger( SignalType::Pressed );
						}
					}
					else if (mb_state == InputActionState::Hovered)
					{
						m_drawing_sc->apply_style_element( style_image.button.hovered.value() );
						if (old_state & StateMask_Activated)
						{
							node.trigger( SignalType::Released );
						}
					}
					else
					{
						m_drawing_sc->apply_style_element( style_image.button.background.value() );
					}

					m_drawing_sc->build_rect( node_global_rect );

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

					m_drawing_sc->build_frame(
						{
							node_global_pos.x - boarder.left,
							node_global_pos.y - boarder.top,
							node_global_rect.w + boarder.left + boarder.right,
							node_global_rect.h + boarder.top + boarder.bottom
						},
						node_global_rect );
				}

				break;
			default:
				break;
			}

			if (!node.m_text.empty())
			{
				m_drawing_sc->build_text( node.m_text, node_global_pos, ig::ColorfTable::White );
			}

			last_node = entry.node_index;
		}

		m_drawing_sc->finish();

		renderer->set_depth_test_comparison( old_dtc );
		if (!was_depthtest_enabled)
			renderer->disable_feature( ig::Feature::DepthTest );

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

		m_tree->mark_dirty();
		return node_index;
	}

	void Interface::move_child( index_t child, index_t new_index ) {
		UNREFERENCED_PARAMETER( child );
		UNREFERENCED_PARAMETER( new_index );
		throw std::exception( "not implemented" );
	}

	void Interface::reparent_child( index_t child, index_t parent ) {
		UNREFERENCED_PARAMETER( child );
		UNREFERENCED_PARAMETER( parent );
		throw std::exception( "not implemented" );
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


		m_tree->mark_dirty();

		// should we validate?
		//validate();
	}

	index_t Interface::transfer_branch( const this_type &old, index_t node_index, index_t parent ) {
		UNREFERENCED_PARAMETER( old );
		UNREFERENCED_PARAMETER( node_index );
		UNREFERENCED_PARAMETER( parent );
		m_tree->mark_dirty();
		return npos;
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

	void Interface::rebuild() {
		m_tree->regenerate( m_roots, m_nodes );
		validate();
	}

	vector<index_t> Interface::get_family( const index_t node_index ) const {
		vector<index_t> indices{};
		vector<index_t> tobe_processed{};
		tobe_processed.push_back( node_index );

		const size_t nodes_count = m_nodes.size();
#ifdef _DEBUG
		size_t limit = 0;
#endif // _DEBUG
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

	void Node::disable() {
		m_state &= ~StateMask_Enabled;
	}

	void Node::enable() {
		m_state |= StateMask_Enabled;
	}

	void Node::hide() {
		m_state &= ~StateMask_Visible;
	}

	void Node::show() {
		m_state |= StateMask_Visible;
	}

	void Node::set_visibility( bool visible ) {
		if (visible)
		{
			show();
		}
		else
		{
			hide();
		}
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

	void Node::set_anchors( const Boxf &anchors ) {
		m_anchors = anchors;
	}

	void Node::set_anchors( float left, float right, float top, float bottom ) {
		m_anchors.left = left;
		m_anchors.right = right;
		m_anchors.top = top;
		m_anchors.bottom = bottom;
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

	void Node::set_text( const string &text ) {
		m_text = text;
	}

	void Node::set_tooltip( const string &text ) {
		m_tooltip = text;
	}

}


