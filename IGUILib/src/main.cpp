#include <igui.h>
#include <ranges>

#ifdef _DEBUG
#pragma comment(lib, "iglib_x64-d")
#pragma comment(lib, "igui_x64-d")
#else
#pragma comment(lib, "iglib_x64")
#pragma comment(lib, "igui_x64")
#endif // _DEBUG

using igui::index_t;
using igui::Node;
using igui::Interface;

static void drawer( ig::Renderer &r );

Interface *g_interface;
int main() {
	ig::Window wind{ {512, 512}, "hello" };
	ig::Renderer rend{ wind, drawer };
	Interface interface{};
	g_interface = &interface;

	{
		Node n{ igui::NodeType::Button };
		n.set_rect( { 0.f, 0.f, 512.f, 512.f } );

		Node boxes{ igui::NodeType::Button };
		boxes.set_rect( { 0.f, 0.f, 32.f, 32.f } );

		index_t t = interface.add_node( n );

		for (int i = 0; i < 9; i++)
		{
			interface.add_node( Node(boxes), t );
		}

	}

	while (!wind.should_close())
	{

		wind.poll();
		rend.clear();
		rend.draw();
	}

}

void drawer( ig::Renderer &r ) {
	g_interface->draw( &r );
}
