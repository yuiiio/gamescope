// For the nested case, reads input from the SDL window and send to wayland

#include <thread>
#include <mutex>

#include <linux/input-event-codes.h>

#include "main.hpp"
#include "wlserver.hpp"
#include "sdlwindow.hpp"
#include "rendervulkan.hpp"
#include "steamcompmgr.hpp"

#include "sdlscancodetable.hpp"

#define DEFAULT_TITLE "gamescope"

static bool g_bSDLInitOK = false;
static std::mutex g_SDLInitLock;

static bool g_bWindowShown = false;

static int g_nOldNestedRefresh = 0;
static bool g_bWindowFocused = true;


extern bool steamMode;
extern bool g_bFirstFrame;

SDL_Window *g_SDLWindow;

enum UserEvents
{
	USER_EVENT_TITLE,
	USER_EVENT_VISIBLE,

	USER_EVENT_COUNT
};

static uint32_t g_unSDLUserEventID;

static std::mutex g_SDLWindowTitleLock;
static std::string g_SDLWindowTitle;
static bool g_bUpdateSDLWindowTitle = false;

//-----------------------------------------------------------------------------
// Purpose: Convert from the remote scancode to a Linux event keycode
//-----------------------------------------------------------------------------
static inline uint32_t SDLScancodeToLinuxKey( uint32_t nScancode )
{
	if ( nScancode < sizeof( s_ScancodeTable ) / sizeof( s_ScancodeTable[0] ) )
	{
		return s_ScancodeTable[ nScancode ];
	}
	return KEY_RESERVED;
}

static inline int SDLButtonToLinuxButton( int SDLButton )
{
	switch ( SDLButton )
	{
		case SDL_BUTTON_LEFT: return BTN_LEFT;
		case SDL_BUTTON_MIDDLE: return BTN_MIDDLE;
		case SDL_BUTTON_RIGHT: return BTN_RIGHT;
		case SDL_BUTTON_X1: return BTN_SIDE;
		case SDL_BUTTON_X2: return BTN_EXTRA;
		default: return 0;
	}
}

void updateOutputRefresh( void )
{
	int display_index = 0;
	SDL_DisplayMode mode = { SDL_PIXELFORMAT_UNKNOWN, 0, 0, 0, 0 };

	display_index = SDL_GetWindowDisplayIndex( g_SDLWindow );
	if ( SDL_GetDesktopDisplayMode( display_index, &mode ) == 0 )
	{
		g_nOutputRefresh = mode.refresh_rate;
	}
}

void inputSDLThreadRun( void )
{
	pthread_setname_np( pthread_self(), "gamescope-sdl" );

	SDL_Event event;
	uint32_t key;

	g_unSDLUserEventID = SDL_RegisterEvents( USER_EVENT_COUNT );

	uint32_t nSDLWindowFlags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN;

	if ( g_bBorderlessOutputWindow == true )
	{
		nSDLWindowFlags |= SDL_WINDOW_BORDERLESS;
	}

	if ( g_bFullscreen == true )
	{
		nSDLWindowFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
	}

	g_SDLWindow = SDL_CreateWindow( DEFAULT_TITLE,
							   SDL_WINDOWPOS_UNDEFINED,
							SDL_WINDOWPOS_UNDEFINED,
							g_nOutputWidth,
							g_nOutputHeight,
							nSDLWindowFlags );

	if ( g_SDLWindow == nullptr )
	{
		fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
		g_SDLInitLock.unlock();
		return;
	}

	SDL_SetRelativeMouseMode(SDL_TRUE);
	SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");

	g_nOldNestedRefresh = g_nNestedRefresh;

	g_bSDLInitOK = true;
	g_SDLInitLock.unlock();

	while( SDL_WaitEvent( &event ) )
	{
		switch( event.type )
		{
			case SDL_MOUSEMOTION:
				if ( g_bWindowFocused )
				{
					wlserver_lock();
					wlserver_mousemotion( event.motion.xrel, event.motion.yrel, event.motion.timestamp );
					wlserver_unlock();
				}
				break;
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
				wlserver_lock();
				wlserver_mousebutton( SDLButtonToLinuxButton( event.button.button ),
									  event.button.state == SDL_PRESSED,
									  event.button.timestamp );
				wlserver_unlock();
				break;
			case SDL_MOUSEWHEEL:
				wlserver_lock();
				wlserver_mousewheel( -event.wheel.x, -event.wheel.y, event.wheel.timestamp );
				wlserver_unlock();
				break;
			case SDL_FINGERMOTION:
				wlserver_lock();
				wlserver_touchmotion( event.tfinger.x, event.tfinger.y, event.tfinger.fingerId, event.tfinger.timestamp );
				wlserver_unlock();
				break;
			case SDL_FINGERDOWN:
				wlserver_lock();
				wlserver_touchdown( event.tfinger.x, event.tfinger.y, event.tfinger.fingerId, event.tfinger.timestamp );
				wlserver_unlock();
				break;
			case SDL_FINGERUP:
				wlserver_lock();
				wlserver_touchup( event.tfinger.fingerId, event.tfinger.timestamp );
				wlserver_unlock();
				break;
			case SDL_KEYDOWN:
			case SDL_KEYUP:
				key = SDLScancodeToLinuxKey( event.key.keysym.scancode );

				if ( event.type == SDL_KEYUP && ( event.key.keysym.mod & KMOD_LGUI ) )
				{
					bool handled = true;
					switch ( key )
					{
						case KEY_F:
							g_bFullscreen = !g_bFullscreen;
							SDL_SetWindowFullscreen( g_SDLWindow, g_bFullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0 );
							break;
						case KEY_N:
							g_wantedUpscaleFilter = GamescopeUpscaleFilter::NEAREST;
							break;
						case KEY_B:
							g_wantedUpscaleFilter = GamescopeUpscaleFilter::LINEAR;
							break;
						case KEY_U:
							g_wantedUpscaleFilter = (g_wantedUpscaleFilter == GamescopeUpscaleFilter::FSR) ?
								GamescopeUpscaleFilter::LINEAR : GamescopeUpscaleFilter::FSR;
							break;
						case KEY_Y:
							g_wantedUpscaleFilter = (g_wantedUpscaleFilter == GamescopeUpscaleFilter::NIS) ? 
								GamescopeUpscaleFilter::LINEAR : GamescopeUpscaleFilter::NIS;
							break;
						case KEY_I:
							g_upscaleFilterSharpness = std::min(20, g_upscaleFilterSharpness + 1);
							break;
						case KEY_O:
							g_upscaleFilterSharpness = std::max(0, g_upscaleFilterSharpness - 1);
							break;
						case KEY_S:
							take_screenshot();
							break;
						default:
							handled = false;
					}
					if ( handled )
					{
						break;
					}
				}

				// On Wayland, clients handle key repetition
				if ( event.key.repeat )
					break;

				wlserver_lock();
				wlserver_key( key, event.type == SDL_KEYDOWN, event.key.timestamp );
				wlserver_unlock();
				break;
			case SDL_WINDOWEVENT:
				switch( event.window.event )
				{
					case SDL_WINDOWEVENT_CLOSE:
						g_bRun = false;
						nudge_steamcompmgr();
						break;
					default:
						break;
					case SDL_WINDOWEVENT_MOVED:
					case SDL_WINDOWEVENT_SHOWN:
						updateOutputRefresh();
						break;
					case SDL_WINDOWEVENT_SIZE_CHANGED:
						g_nOutputWidth = event.window.data1;
						g_nOutputHeight = event.window.data2;

						updateOutputRefresh();

						break;
					case SDL_WINDOWEVENT_FOCUS_LOST:
						g_nNestedRefresh = g_nNestedUnfocusedRefresh;
						g_bWindowFocused = false;
						break;
					case SDL_WINDOWEVENT_FOCUS_GAINED:
						g_nNestedRefresh = g_nOldNestedRefresh;
						g_bWindowFocused = true;
						break;
					case SDL_WINDOWEVENT_EXPOSED:
						force_repaint();
						break;
				}
				break;
			default:
				if ( event.type == g_unSDLUserEventID + USER_EVENT_TITLE )
				{
					g_SDLWindowTitleLock.lock();
					if ( g_bUpdateSDLWindowTitle )
					{
						g_bUpdateSDLWindowTitle = false;
						SDL_SetWindowTitle( g_SDLWindow, g_SDLWindowTitle.c_str() );
					}
					g_SDLWindowTitleLock.unlock();
				}
				if ( event.type == g_unSDLUserEventID + USER_EVENT_VISIBLE )
				{
					bool should_show = !!event.user.code;

					// If we are Steam Mode in nested, show the window
					// whenever we have had a first frame to match
					// what we do in embedded with Steam for testing
					// held commits, etc.
					if ( steamMode )
						should_show |= !g_bFirstFrame;

					if ( g_bWindowShown != should_show )
					{
						g_bWindowShown = should_show;

						if ( g_bWindowShown )
						{
							SDL_ShowWindow( g_SDLWindow );
						}
						else
						{
							SDL_HideWindow( g_SDLWindow );
						}
					}
				}
				break;
		}
	}
}

bool sdlwindow_init( void )
{
	g_SDLInitLock.lock();

	std::thread inputSDLThread( inputSDLThreadRun );
	inputSDLThread.detach();

	// When this returns SDL_Init should be over
	g_SDLInitLock.lock();

	return g_bSDLInitOK;
}

void sdlwindow_title( const char* title )
{
	if ( !BIsNested() )
		return;

	title = title ? title : DEFAULT_TITLE;
	g_SDLWindowTitleLock.lock();
	if ( g_SDLWindowTitle != title )
	{
		g_SDLWindowTitle = title ? title : DEFAULT_TITLE;
		g_bUpdateSDLWindowTitle = true;

		SDL_Event event;
		event.type = g_unSDLUserEventID + USER_EVENT_TITLE;
		SDL_PushEvent( &event );
	}
	g_SDLWindowTitleLock.unlock();
}

void sdlwindow_visible( bool bVisible )
{
	if ( !BIsNested() )
		return;

	SDL_Event event;
	event.type = g_unSDLUserEventID + USER_EVENT_VISIBLE;
	event.user.code = bVisible ? 1 : 0;
	SDL_PushEvent( &event );
}
