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
		n.set_rect( { 32.f, 32.f, 64.f, 64.f } );

		index_t t = interface.add_node( n );
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
