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


	using Transform = ig::Transform2D;
	using Vec2f = ig::Vector2f;
	using Vec2i = ig::Vector2i;


	class Node;
	class NodeFactory;
	class Context;

	enum class NodeType : u8
	{
		None = 0,
		Button,
		RadialButton,
	};

	class NodeFactory
	{

	};

	class Style
	{

	};

	class Context
	{
	public:

	private:
		std::vector<Node> m_nodes;
	};

	class Node
	{
		NodeType m_type;
		std::vector<index_t> m_children;
		index_t m_parent = InvalidIndex;
	};

}
