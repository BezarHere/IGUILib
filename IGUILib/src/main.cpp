#include <igui.h>
#include <ranges>

#ifdef _DEBUG
#pragma comment(lib, "iglib_x64-d")
#pragma comment(lib, "igui_x64-d")
#else
#pragma comment(lib, "iglib_x64")
#pragma comment(lib, "igui_x64")
#endif // _DEBUG


static void drawer( ig::Renderer &r );

igui::Interface *g_interface;
int main() {
	ig::Window wind{ {512, 512}, "hello" };
	ig::Renderer rend{ wind, drawer };
	igui::Interface interface{};
	g_interface = &interface;

	igui::Node n{ igui::NodeType::Button };
	n.set_rect( { 32.f, 32.f, 64.f, 64.f } );

	igui::index_t t = interface.add_node(n);

	for (int i = 0; i < 1000; i++)
	{
		t = interface.add_node( igui::Node( n ), t );
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
