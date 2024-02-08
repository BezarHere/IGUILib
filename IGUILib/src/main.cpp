#include <igui.h>
#include <chrono>
#include <thread>

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

// for debug
static uint64_t g_counter = 0;

static void drawer( ig::Renderer &r );
static void input( ig::Window &wind, ig::InputEvent event, ig::InputEventType type );

Interface *g_interface;
int main() {
	ig::Window wind{ {512, 512}, "hello" };

	wind.set_input_callback( input );

	ig::Renderer rend{ wind, drawer };
	Interface interface {};
	g_interface = &interface;

	{
		Node n{ igui::NodeType::Button };
		n.set_rect( { 0.f, 0.f, 256.f, 512.f } );

		index_t t = interface.add_node( n );

		Node subbox{ igui::NodeType::Button };
		subbox.set_rect( { 16.f, 16.f, 32.f, 32.f } );
		subbox.set_anchors( 0.f, 0.f, 0.f, 0.f );
		subbox.set_mouse_filter( igui::MouseFilter::Stop );

		//subbox.set_position( subbox.get_position() + igui::Vec2f( 64.f, 0.f ) );
		for (int i = 0; i < 32; i++)
		{
			subbox.set_position( { 16.f, 16.f + (i * 40.f) } );
			for (int j = 0; j < 32; j++)
			{
				interface.add_node( subbox, t );
				subbox.set_position( subbox.get_position() + igui::Vec2f( 40.f, 0.f ) );
			}
		}

		//for (int i = 0; i < 9; i++)
		//{
		//	interface.add_node( Node( boxes ), t );
		//}

	}

	while (wind)
	{
		std::this_thread::sleep_for( std::chrono::microseconds( 32 ) );

		wind.poll();
		interface.update();
		rend.clear();
		rend.draw();


		g_counter++;
	}

}

void drawer( ig::Renderer &r ) {
	r.get_canvas().transform2d().set_rotation( std::sin( (float)g_counter / 30.f ) * 0.1f );
	r.get_canvas().update_transform_state();
	g_interface->draw( &r );
}


static void input( ig::Window &wind, ig::InputEvent event, ig::InputEventType type ) {
	g_interface->input( { event, type } );
}
