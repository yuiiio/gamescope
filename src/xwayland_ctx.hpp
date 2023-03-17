#pragma once

#include "drm.hpp"

#include <mutex>
#include <memory>
#include <vector>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/XRes.h>
#include <X11/extensions/shape.h>
#include <X11/extensions/xf86vmode.h>

class gamescope_xwayland_server_t;
struct ignore;
struct win;
class MouseCursor;

struct focus_t
{
	win				*focusWindow;
	win				*inputFocusWindow;
	uint32_t		inputFocusMode;
	win				*overlayWindow;
	win				*externalOverlayWindow;
	win				*notificationWindow;
	win				*overrideWindow;
	bool			outdatedInteractiveFocus;
};

struct xwayland_ctx_t
{
	gamescope_xwayland_server_t *xwayland_server;
	Display			*dpy;

	win				*list;
	int				scr;
	Window			root;
	XserverRegion	allDamage;
	bool			clipChanged;
	int				root_height, root_width;
	ignore			*ignore_head, **ignore_tail;
	int				xfixes_event, xfixes_error;
	int				damage_event, damage_error;
	int				composite_event, composite_error;
	int				render_event, render_error;
	int				xshape_event, xshape_error;
	int				composite_opcode;
	Window			ourWindow;

	focus_t 		focus;
	Window 			currentKeyboardFocusWindow;
	Window			focusControlWindow;

	std::unique_ptr<MouseCursor> cursor;

	std::mutex listCommitsDoneLock;
	std::vector< uint64_t > listCommitsDone;

	double accum_x = 0.0;
	double accum_y = 0.0;

	bool force_windows_fullscreen = false;

	struct {
		Atom steamAtom;
		Atom gameAtom;
		Atom overlayAtom;
		Atom externalOverlayAtom;
		Atom gamesRunningAtom;
		Atom screenZoomAtom;
		Atom screenScaleAtom;
		Atom opacityAtom;
		Atom winTypeAtom;
		Atom winDesktopAtom;
		Atom winDockAtom;
		Atom winToolbarAtom;
		Atom winMenuAtom;
		Atom winUtilAtom;
		Atom winSplashAtom;
		Atom winDialogAtom;
		Atom winNormalAtom;
		Atom sizeHintsAtom;
		Atom netWMStateFullscreenAtom;
		Atom activeWindowAtom;
		Atom netWMStateAtom;
		Atom WMTransientForAtom;
		Atom netWMStateHiddenAtom;
		Atom netWMStateFocusedAtom;
		Atom netWMStateSkipTaskbarAtom;
		Atom netWMStateSkipPagerAtom;
		Atom WLSurfaceIDAtom;
		Atom WMStateAtom;
		Atom steamInputFocusAtom;
		Atom WMChangeStateAtom;
		Atom steamTouchClickModeAtom;
		Atom utf8StringAtom;
		Atom netWMNameAtom;
		Atom motifWMHints;
		Atom netSystemTrayOpcodeAtom;
		Atom steamStreamingClientAtom;
		Atom steamStreamingClientVideoAtom;
		Atom gamescopeFocusableAppsAtom;
		Atom gamescopeFocusableWindowsAtom;
		Atom gamescopeFocusedWindowAtom;
		Atom gamescopeFocusedAppAtom;
		Atom gamescopeFocusedAppGfxAtom;
		Atom gamescopeCtrlAppIDAtom;
		Atom gamescopeCtrlWindowAtom;
		Atom gamescopeInputCounterAtom;
		Atom gamescopeScreenShotAtom;

		Atom gamescopeFocusDisplay;
		Atom gamescopeMouseFocusDisplay;
		Atom gamescopeKeyboardFocusDisplay;

		Atom gamescopeTuneableVBlankRedZone;
		Atom gamescopeTuneableRateOfDecay;

		Atom gamescopeScalingFilter;
		Atom gamescopeFSRSharpness;
		Atom gamescopeSharpness;

		Atom gamescopeColorLinearGain;
		Atom gamescopeColorGain;
		Atom gamescopeColorMatrix[DRM_SCREEN_TYPE_COUNT];
		Atom gamescopeColorLinearGainBlend;

		Atom gamescopeColorGammaExponent[DRM_SCREEN_TYPE_COUNT];

		Atom gamescopeXWaylandModeControl;

		Atom gamescopeFPSLimit;
		Atom gamescopeDynamicRefresh[DRM_SCREEN_TYPE_COUNT];
		Atom gamescopeLowLatency;

		Atom gamescopeFSRFeedback;

		Atom gamescopeBlurMode;
		Atom gamescopeBlurRadius;
		Atom gamescopeBlurFadeDuration;

		Atom gamescopeCompositeForce;
		Atom gamescopeCompositeDebug;

		Atom gamescopeAllowTearing;
		Atom gamescopeDisplayForceInternal;
		Atom gamescopeDisplayModeNudge;

		Atom gamescopeDisplayIsExternal;
		Atom gamescopeDisplayModeListExternal;

		Atom gamescopeCursorVisibleFeedback;

		Atom gamescopeSteamMaxHeight;

		Atom gamescopeVRRCapable;
		Atom gamescopeVRREnabled;
		Atom gamescopeVRRInUse;

		Atom gamescopeNewScalingFilter;
		Atom gamescopeNewScalingScaler;

		Atom gamescopeDisplayEdidPath;

		Atom gamescopeForceWindowsFullscreen;

		Atom wineHwndStyle;
		Atom wineHwndStyleEx;
	} atoms;
};
