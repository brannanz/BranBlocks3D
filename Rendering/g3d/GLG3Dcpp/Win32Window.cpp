/**
  @file Win32Window.cpp

  @maintainer Corey Taylor, vejita@users.sf.net 
  @cite       Written by Corey Taylor & Morgan McGuire
  @cite       Special thanks to Max McGuire of Ironlore
  @created 	  2004-05-21
  @edited  	  2006-01-15
    
  Copyright 2000-2006, Morgan McGuire.
  All rights reserved.
*/

#include "G3D/platform.h"
#include "GLG3D/GLCaps.h"

// This file is ignored on other platforms
#ifdef G3D_WIN32

#include <windows.h>
#include "graphics3d.h"
#include "GLG3D/Win32Window.h"
#include "GLG3D/glcalls.h"
#include "GLG3D/UserInput.h"
#include "directinput8.h"

#include <time.h>
#include <sstream>
#if !defined(G3D_MINGW32)
#include <crtdbg.h>
#endif

using G3D::_internal::_DirectInput;

/*
    DirectInput8 support is added by loading dinupt8.dll if found.

    COM calls are not used to limit the style of code and includes needed.
    DirectInput8 has a special creation function that lets us do this properly.

    The joystick state structure used simulates the exports found in dinput8.lib

    The joystick axis order returned to users is: X, Y, Z, Slider1, Slider2, rX, rY, rZ.

    Important:  The cooperation level of Win32Window joysticks is Foreground:Non-Exclusive.
    This means that other programs can get access to the joystick(preferably non-exclusive) and the joystick
     is only aquired when Win32Window is in the foreground.
*/

namespace G3D {

static const UINT BLIT_BUFFER = 0xC001;

#define WGL_SAMPLE_BUFFERS_ARB	0x2041
#define WGL_SAMPLES_ARB		    0x2042

static bool hasWGLMultiSampleSupport = false;

static PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB = NULL;

static unsigned int _sdlKeys[SDLK_LAST];
static bool sdlKeysInitialized = false;

// Prototype static helper functions at end of file
static bool ChangeResolution(int, int, int, int);
static void makeKeyEvent(int, int, GEvent&);
static void mouseButton(bool, int, DWORD, GEvent&);
static void initWin32KeyMap();
//static LRESULT WINAPI _internal::window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);

#if 0
// For use when debugging format negotiation
static void printPixelFormatDescription(int, HDC, TextOutput&);
#endif

/** Return the G3D window class, which owns a private DC. 
    See http://www.starstonesoftware.com/OpenGL/whyyou.htm
    for a discussion of why this is necessary. */
static const char* G3DWndClass();

std::auto_ptr<Win32Window> Win32Window::_shareWindow(NULL);


Win32Window::Win32Window(const GWindow::Settings& s, bool creatingShareWindow)
:createdWindow(true)
,_diDevices(NULL)

{
    _receivedCloseEvent = false;

    initWGL();

	_hDC = NULL;
	_mouseVisible = true;
	_inputCapture = false;
    _windowActive = false;
	_thread = ::GetCurrentThread();

    if (!sdlKeysInitialized) {
        initWin32KeyMap();
    }

	settings = s;
    
	std::string name = "";
    
    // Add the non-client area
	RECT rect;
	rect.left = 0;
	rect.top = 0;
	rect.right = settings.width;
	rect.bottom = settings.height;

	DWORD style = 0;
	
	if (s.framed) {

        // http://msdn.microsoft.com/library/default.asp?url=/library/en-us/winui/WinUI/WindowsUserInterface/Windowing/Windows/WindowReference/WindowStyles.asp
		style |= WS_BORDER | WS_CAPTION | WS_MINIMIZEBOX | WS_SYSMENU;

		if (s.resizable) {
			style |= WS_SIZEBOX;
		}

    } else {

        // Show nothing but the client area (cannot move window with mouse)
        style |= WS_POPUP;
    }

    int oldTop = rect.top;
    int oldLeft = rect.left;
	AdjustWindowRect(&rect, style, false);

    clientRectOffset.x = oldLeft - rect.left;
    clientRectOffset.y = oldTop - rect.top;

	int total_width  = rect.right - rect.left;
	int total_height = rect.bottom - rect.top;

    int startX = 0;
    int startY = 0;

    if (! s.fullScreen) {
        if (s.center) {
            
            startX = (GetSystemMetrics(SM_CXSCREEN) - total_width) / 2;
            startY = (GetSystemMetrics(SM_CYSCREEN) - total_height) / 2;
        } else {

            startX = s.x;
            startY = s.y;
        }
    }

    clientX = settings.x = startX;
    clientY = settings.y = startY;
    
    HWND window = CreateWindow(G3DWndClass(), 
        name.c_str(),
        style,
        startX,
        startY,
        total_width,
        total_height,
        NULL,
        NULL,
        GetModuleHandle(NULL),
        NULL);

    alwaysAssertM(window != NULL, "");

    // Set early so windows messages have value
    this->window = window;

    SetWindowLong(window, GWL_USERDATA, (LONG)this);

    if (s.visible) {
        ShowWindow(window, SW_SHOW);
    }         

    if (settings.fullScreen) {
	    // Change the desktop resolution if we are running in fullscreen mode
        if (!ChangeResolution(settings.width, settings.height, (settings.rgbBits * 3) + settings.alphaBits, settings.refreshRate)) {
			alwaysAssertM(false, "Failed to change resolution");
        }
    }

	init(window, creatingShareWindow);

    // Set default icon if available
    if (settings.defaultIconFilename != "nodefault") {

        try {

            GImage defaultIcon;
            defaultIcon.load(settings.defaultIconFilename);

            setIcon(defaultIcon);
        } catch (const GImage::Error& e) {
            // Throw away default icon
		    debugPrintf("GWindow's default icon failed to load: %s (%s)", e.filename.c_str(), e.reason.c_str());
            Log::common()->printf("GWindow's default icon failed to load: %s (%s)", e.filename.c_str(), e.reason.c_str());            
        }
    }
}



Win32Window::Win32Window(const GWindow::Settings& s, HWND hwnd) : createdWindow(false), _diDevices(NULL) {
    initWGL();

	_thread = ::GetCurrentThread();
	settings = s;
	init(hwnd);

    _windowActive = hasFocus();
}


Win32Window::Win32Window(const GWindow::Settings& s, HDC hdc) : createdWindow(false), _diDevices(NULL)  {
    initWGL();

	_thread = ::GetCurrentThread();
	settings = s;

    HWND hwnd = ::WindowFromDC(hdc);

    debugAssert(hwnd != NULL);

	init(hwnd);

    _windowActive = hasFocus();
}


Win32Window* Win32Window::create(const GWindow::Settings& settings) {

    // Create Win32Window which uses DI8 joysticks but WM_ keyboard messages
    return new Win32Window(settings);    
    
}


Win32Window* Win32Window::create(const GWindow::Settings& settings, HWND hwnd) {

    // Create Win32Window which uses DI8 joysticks but WM_ keyboard messages
    return new Win32Window(settings, hwnd);    
    
}

Win32Window* Win32Window::create(const GWindow::Settings& settings, HDC hdc) {

    // Create Win32Window which uses DI8 joysticks but WM_ keyboard messages
    return new Win32Window(settings, hdc);    
    
}


void Win32Window::init(HWND hwnd, bool creatingShareWindow) {

	if (! creatingShareWindow) {
		createShareWindow(settings);
	}

	window = hwnd;

    // Initialize mouse buttons to up
    _mouseButtons[0] = _mouseButtons[1] = _mouseButtons[2] = false;

    // Clear all keyboard buttons to up (not down)
    memset(_keyboardButtons, 0, sizeof(_keyboardButtons));
 
    // Setup the pixel format properties for the output device
    _hDC = GetDC(window);

    bool foundARBFormat = false;
    int pixelFormat = 0;

    if (wglChoosePixelFormatARB != NULL) {
        // Use wglChoosePixelFormatARB to override the pixel format choice for antialiasing.
        // Based on http://nehe.gamedev.net/data/lessons/lesson.asp?lesson=46
        // and http://oss.sgi.com/projects/ogl-sample/registry/ARB/wgl_pixel_format.txt

        Array<float> fAttributes;
        fAttributes.append(0.0, 0.0);

        Array<int> iAttributes;

        iAttributes.append(WGL_DRAW_TO_WINDOW_ARB, GL_TRUE);
		iAttributes.append(WGL_SUPPORT_OPENGL_ARB, GL_TRUE);
        if (settings.hardware) {
		    iAttributes.append(WGL_ACCELERATION_ARB,   WGL_FULL_ACCELERATION_ARB);
        }
        iAttributes.append(WGL_DOUBLE_BUFFER_ARB,  GL_TRUE);
        iAttributes.append(WGL_COLOR_BITS_ARB,     settings.rgbBits * 3);
        iAttributes.append(WGL_RED_BITS_ARB,       settings.rgbBits);
        iAttributes.append(WGL_GREEN_BITS_ARB,     settings.rgbBits);
        iAttributes.append(WGL_BLUE_BITS_ARB,      settings.rgbBits);
        iAttributes.append(WGL_ALPHA_BITS_ARB,     settings.alphaBits);
        iAttributes.append(WGL_DEPTH_BITS_ARB,     settings.depthBits);
        iAttributes.append(WGL_STENCIL_BITS_ARB,   settings.stencilBits);
        iAttributes.append(WGL_STEREO_ARB,         settings.stereo);
        if (hasWGLMultiSampleSupport && (settings.fsaaSamples > 1)) {
            // On some ATI cards, even setting the samples to false will turn it on,
            // so we only take this branch when FSAA is explicitly requested.
            iAttributes.append(WGL_SAMPLE_BUFFERS_ARB, settings.fsaaSamples > 1);
		    iAttributes.append(WGL_SAMPLES_ARB,        settings.fsaaSamples);
        } else {
            // Report actual settings
            settings.fsaaSamples = 0;
        }
        iAttributes.append(0, 0); // end sentinel
        
        // http://www.nvidia.com/dev_content/nvopenglspecs/WGL_ARB_pixel_format.txt
        uint32 numFormats;
        int valid = wglChoosePixelFormatARB(
            _hDC,
            iAttributes.getCArray(), 
            fAttributes.getCArray(),
            1,
            &pixelFormat,
            &numFormats);

        // "If the function succeeds, the return value is TRUE. If the function
        // fails the return value is FALSE. To get extended error information,
        // call GetLastError. If no matching formats are found then nNumFormats
        // is set to zero and the function returns TRUE."  -- I think this means
        // that when numFormats == 0 some reasonable format is still selected.
        
        // Corey - I don't think it does, but now I check for valid pixelFormat + valid return only.

        if ( valid && (pixelFormat > 0)) {
            // Found a valid format
            foundARBFormat = true;
        }

    }

    PIXELFORMATDESCRIPTOR pixelFormatDesc;

    if ( !foundARBFormat ) {

        ZeroMemory(&pixelFormatDesc, sizeof(PIXELFORMATDESCRIPTOR));

        pixelFormatDesc.nSize        = sizeof(PIXELFORMATDESCRIPTOR);
        pixelFormatDesc.nVersion     = 1; 
        pixelFormatDesc.dwFlags      = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pixelFormatDesc.iPixelType   = PFD_TYPE_RGBA; 
        pixelFormatDesc.cColorBits   = settings.rgbBits * 3;
        pixelFormatDesc.cDepthBits   = settings.depthBits;
        pixelFormatDesc.cStencilBits = settings.stencilBits;
        pixelFormatDesc.iLayerType   = PFD_MAIN_PLANE; 
        pixelFormatDesc.cRedBits     = settings.rgbBits;
        pixelFormatDesc.cGreenBits   = settings.rgbBits;
        pixelFormatDesc.cBlueBits    = settings.rgbBits;
        pixelFormatDesc.cAlphaBits   = settings.alphaBits;

        // Reset for completeness
        pixelFormat = 0;

        // Get the initial pixel format.  We'll override this below in a moment.
        pixelFormat = ChoosePixelFormat(_hDC, &pixelFormatDesc);
    } else {
        DescribePixelFormat(_hDC, pixelFormat, sizeof(PIXELFORMATDESCRIPTOR), &pixelFormatDesc);
    }

    alwaysAssertM(pixelFormat != 0, "[0] Unsupported video mode");

    if (SetPixelFormat(_hDC, pixelFormat, &pixelFormatDesc) == FALSE) {
		alwaysAssertM(false, "[1] Unsupported video mode");
    }

    // Create the OpenGL context
    _glContext = wglCreateContext(_hDC);

	alwaysAssertM(_glContext != NULL, "Failed to create OpenGL context.");

	if (! creatingShareWindow) {
		// Now share resources with the global window
		wglShareLists(_shareWindow->_glContext, _glContext);
	}

	this->makeCurrent();

	if (! creatingShareWindow) {
        GLCaps::init();
    }
}

/* TODO
const Array<GWindow::Settings>& Win32Window::SupportedWindowSettings() {
    return _supportedSettings;
}

bool Win32Window::ClosestSupportedWindowSettings(const GWindow::Settings& desired, GWindow::Settings& closest) {
    return false;
}
*/

int Win32Window::width() const {
	return settings.width;
}


int Win32Window::height() const {
	return settings.height;
}


void Win32Window::setDimensions(const Rect2D& dims) {

    int W = ::GetSystemMetrics(SM_CXSCREEN);
    int H = ::GetSystemMetrics(SM_CYSCREEN);

    int x = iClamp((int)dims.x0(), 0, W);
    int y = iClamp((int)dims.y0(), 0, H);
    int w = iClamp((int)dims.width(), 1, W);
    int h = iClamp((int)dims.height(), 1, H);

    // Set dimensions and repaint.
    ::MoveWindow(window, x, y, w, h, true);
}


Rect2D Win32Window::dimensions() const {
	return Rect2D::xywh((float)clientX, (float)clientY, (float)width(), (float)height());
}


bool Win32Window::hasFocus() const {
    // Double check state with foreground and visibility just to be sure.
    return ( (window == ::GetForegroundWindow()) && (::IsWindowVisible(window)) );
}


std::string Win32Window::getAPIVersion() const {
	return "1.1";
}


std::string Win32Window::getAPIName() const {
	return "Win32";
}


bool Win32Window::requiresMainLoop() const {
    return false;
}


void Win32Window::setIcon(const GImage& image) {
    alwaysAssertM((image.channels == 3) ||
                  (image.channels == 4), 
                  "Icon image must have at least 3 channels.");

    alwaysAssertM((image.width == 32) && (image.height == 32),
        "Icons must be 32x32 on windows.");

    uint8 bwMaskData[128];
    uint8 colorMaskData[1024*4];


    GImage icon;
    if (image.channels == 3) {
        
        GImage alpha(image.width, image.height, 1);
        System::memset(alpha.byte(), 255, (image.width * image.height));
        icon = image.insertRedAsAlpha(alpha);
    } else {
        icon = image;
    }

    int colorMaskIdx = 0;
    System::memset(bwMaskData, 0x00, 128);
    for (int y = 0; y < 32; ++y) {
        for (int x = 0; x < 32; ++x) {
            bwMaskData[ (y * 4) + (x / 8) ] |= ((icon.pixel4(x, y).a > 127) ? 1 : 0) << (x % 8);

            // Windows icon images are BGRA like a lot of windows image data
            colorMaskData[colorMaskIdx] = icon.pixel4()[y * 32 + x].b;
            colorMaskData[colorMaskIdx + 1] = icon.pixel4()[y * 32 + x].g;
            colorMaskData[colorMaskIdx + 2] = icon.pixel4()[y * 32 + x].r;
            colorMaskData[colorMaskIdx + 3] = icon.pixel4()[y * 32 + x].a;
            colorMaskIdx += 4;
        }
    }

    HBITMAP bwMask = ::CreateBitmap(32, 32, 1, 1, bwMaskData);  
    HBITMAP colorMask = ::CreateBitmap(32, 32, 1, 32, colorMaskData);

    ICONINFO iconInfo;
    iconInfo.xHotspot = 0;
    iconInfo.yHotspot = 0;
    iconInfo.hbmColor = colorMask;
    iconInfo.hbmMask = bwMask;
    iconInfo.fIcon = true;

    HICON hicon = ::CreateIconIndirect(&iconInfo);
    _usedIcons.insert((int)hicon);

    // Purposely leak any icon created indirectly like hicon becase we don't know.
    HICON hsmall = (HICON)::SendMessage(this->window, WM_SETICON, (WPARAM)ICON_SMALL, (LPARAM)hicon);
    HICON hlarge = (HICON)::SendMessage(this->window, WM_SETICON, (WPARAM)ICON_BIG, (LPARAM)hicon);

    if (_usedIcons.contains((int)hsmall)) {
        ::DestroyIcon(hsmall);
        _usedIcons.remove((int)hsmall);
    }
    
    if (_usedIcons.contains((int)hlarge)) {
        ::DestroyIcon(hlarge);
        _usedIcons.remove((int)hlarge);
    }

    ::DeleteObject(bwMask);
    ::DeleteObject(colorMask);
}


void Win32Window::swapGLBuffers() {
	SwapBuffers(hdc());
}


void Win32Window::close() {
    PostMessage(window, WM_CLOSE, 0, 0);
}


Win32Window::~Win32Window() {
	if (GWindow::current() == this) {
		if (wglMakeCurrent(NULL, NULL) == FALSE)	{
			debugAssertM(false, "Failed to set context");
		}

        if (createdWindow) {
            // Release the mouse
            setMouseVisible(true);
            setInputCapture(false);
        }
	}

	if (createdWindow) {
        SetWindowLong(window, GWL_USERDATA, (LONG)NULL);
		close();
	}

    // Do not need to release private HDC's

    delete _diDevices;
}


void Win32Window::getSettings(GWindow::Settings& s) const {
	s = settings;
}


void Win32Window::setCaption(const std::string& caption) {
	if (_title != caption) {
		_title = caption;
		SetWindowText(window, _title.c_str());
	}
}


std::string Win32Window::caption() {
	return _title;
}


bool Win32Window::pollEvent(GEvent& e) {
    MSG message;

    while (PeekMessage(&message, window, 0, 0, PM_REMOVE)) {
        TranslateMessage(&message);
		DispatchMessage(&message);

        if (message.hwnd == window) {
            switch (message.message) {
            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:

				e.key.type = SDL_KEYDOWN;
				e.key.state = SDL_PRESSED;

                // Fix invalid repeat key flag
                if (justReceivedFocus) {
                    justReceivedFocus = false;
                    message.lParam &= ~(0x40000000);
                }

                // Need the repeat messages to find LSHIFT and RSHIFT
				if (!((message.lParam >> 30) & 0x01)) {
					// This is not an autorepeat message
					makeKeyEvent(message.wParam, message.lParam, e);
                    _keyboardButtons[message.wParam] = true;
					return true;
				}
				break;


			case WM_KEYUP:
            case WM_SYSKEYUP:
				e.key.type = SDL_KEYUP;
				e.key.state = SDL_RELEASED;

				makeKeyEvent(message.wParam, message.lParam, e);
                _keyboardButtons[message.wParam] = false;
				return true;
				break;


            case WM_LBUTTONDOWN:
				mouseButton(true, SDL_LEFT_MOUSE_KEY, message.wParam, e); 
                _mouseButtons[0] = true;
				return true;

            case WM_MBUTTONDOWN:
				mouseButton(true, SDL_MIDDLE_MOUSE_KEY, message.wParam, e); 
                _mouseButtons[1] = true;
				return true;

            case WM_RBUTTONDOWN:
				mouseButton(true, SDL_RIGHT_MOUSE_KEY, message.wParam, e); 
                _mouseButtons[2] = true;
				return true;

            case WM_LBUTTONUP:
				mouseButton(false, SDL_LEFT_MOUSE_KEY, message.wParam, e); 
                _mouseButtons[0] = false;
				return true;

            case WM_MBUTTONUP:
				mouseButton(false, SDL_MIDDLE_MOUSE_KEY, message.wParam, e); 
                _mouseButtons[1] = false;
				return true;

            case WM_RBUTTONUP:
				mouseButton(false, SDL_RIGHT_MOUSE_KEY, message.wParam, e); 
                _mouseButtons[2] = false;
				return true;
            } // switch
        } // if
    } // while

    // We never seem to "receive" the WM_QUIT message 
    // so test for WM_CLOSE in the window_proc
    if (_receivedCloseEvent) {

        // Reset so SDL_QUIT is only sent once
        _receivedCloseEvent = false;

        e.type = SDL_QUIT;
        return true;
    }

    RECT rect;
    GetWindowRect(window, &rect);
    settings.x = rect.left;
    settings.y = rect.top;

    GetClientRect(window, &rect);
    settings.width = rect.right - rect.left;
    settings.height = rect.bottom - rect.top;

    clientX = settings.x;
    clientY = settings.y;

    if (settings.framed) {
        // Add the border offset
        clientX	+= GetSystemMetrics(settings.resizable ? SM_CXSIZEFRAME : SM_CXFIXEDFRAME);
        clientY += GetSystemMetrics(settings.resizable ? SM_CYSIZEFRAME : SM_CYFIXEDFRAME) + GetSystemMetrics(SM_CYCAPTION);
    }

    // Check for a resize event and use only most recent
    // Just use an array instead of used flag and variable
    // This will be clearer in the 7.0 event system
    if (sizeEventInjects.size() > 0) {
        e = sizeEventInjects.last();
        sizeEventInjects.clear();
        return true;
    }

    return false;
}


void Win32Window::setMouseVisible(bool b) {
    m_mouseHideCount = b ? 0 : 1;

    if (_mouseVisible == b) {
        return;
    }

    if (b) {
        while (ShowCursor(true) < 0);
    } else {
        while (ShowCursor(false) >= 0); 
    }

    _mouseVisible = b;
}


bool Win32Window::mouseVisible() const {
    return _mouseVisible;
}


void Win32Window::notifyResize(int w, int h) {
    settings.width = w;
    settings.height = h;
}


bool Win32Window::inputCapture() const {
    return _inputCapture;
}


void Win32Window::setGammaRamp(const Array<uint16>& gammaRamp) {
    alwaysAssertM(gammaRamp.size() >= 256, "Gamma ramp must have at least 256 entries");

    Log* debugLog = Log::common();

    uint16* ptr = const_cast<uint16*>(gammaRamp.getCArray());
    uint16 wptr[3 * 256];
    for (int i = 0; i < 256; ++i) {
        wptr[i] = wptr[i + 256] = wptr[i + 512] = ptr[i]; 
    }
    BOOL success = SetDeviceGammaRamp(hdc(), wptr);

    if (! success) {
        if (debugLog) {debugLog->println("Error setting gamma ramp! (Possibly LCD monitor)");}
    }
}


void Win32Window::setRelativeMousePosition(double x, double y) {
	SetCursorPos(iRound(x + clientX), iRound(y + clientY));
}


void Win32Window::setRelativeMousePosition(const Vector2& p) {
    setRelativeMousePosition(p.x, p.y);
}


void Win32Window::getRelativeMouseState(Vector2& p, uint8& mouseButtons) const {
    int x, y;
    getRelativeMouseState(x, y, mouseButtons);
    p.x = (float)x;
    p.y = (float)y;
}


void Win32Window::getRelativeMouseState(int& x, int& y, uint8& mouseButtons) const {
	POINT point;
	GetCursorPos(&point);
	x = point.x - clientX;
	y = point.y - clientY;

    // Clear mouseButtons and set each button bit.
	mouseButtons ^= mouseButtons;
    mouseButtons |= (_mouseButtons[0] ? 1 : 0) << 0;
    mouseButtons |= (_mouseButtons[1] ? 1 : 0) << 1;
    mouseButtons |= (_mouseButtons[2] ? 1 : 0) << 2;
}


void Win32Window::getRelativeMouseState(double& x, double& y, uint8& mouseButtons) const {
    int ix, iy;
    getRelativeMouseState(ix, iy, mouseButtons);
    x = ix;
    y = iy;
}

inline void Win32Window::enableDirectInput() const {
    if (_diDevices==NULL)
		_diDevices = new _DirectInput(window);
}

int Win32Window::numJoysticks() const {
	enableDirectInput();
    return _diDevices->getNumJoysticks();
}


std::string Win32Window::joystickName(unsigned int sticknum)
{
	enableDirectInput();
    return _diDevices->getJoystickName(sticknum);
}


void Win32Window::getJoystickState(unsigned int stickNum, Array<float>& axis, Array<bool>& button) {

	enableDirectInput();
    if (!_diDevices->joystickExists(stickNum)) {
        return;
    }

    _diDevices->getJoystickState(stickNum, axis, button);
}


void Win32Window::setInputCapture(bool c) {
    m_inputCaptureCount = c ? 1 : 0;

	if (c != _inputCapture) {
		_inputCapture = c;

		if (_inputCapture) {
            RECT wrect;
            GetWindowRect(window, &wrect);
            clientX = wrect.left;
            clientY = wrect.top;

            RECT rect = 
            {clientX + clientRectOffset.x, 
             clientY + clientRectOffset.y, 
             clientX + settings.width + clientRectOffset.x, 
             clientY + settings.height + clientRectOffset.y};
			ClipCursor(&rect);
		} else {
			ClipCursor(NULL);
		}
	}
}


void Win32Window::initWGL() {
    
    // This function need only be called once
    static bool wglInitialized = false;
    if (wglInitialized) {
        return;
    }
    wglInitialized = true;

    std::string name = "G3D";
    WNDCLASS window_class;
    
    window_class.style         = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc   = _internal::window_proc;
    window_class.cbClsExtra    = 0; 
    window_class.cbWndExtra    = 0;
    window_class.hInstance     = GetModuleHandle(NULL);
    window_class.hIcon         = LoadIcon(NULL, IDI_APPLICATION); 
    window_class.hCursor       = LoadCursor(NULL, IDC_ARROW);
    window_class.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    window_class.lpszMenuName  = name.c_str();
    window_class.lpszClassName = "window"; 
    
    int ret = RegisterClass(&window_class);
	alwaysAssertM(ret, "Registration Failed");

    // Create some dummy pixel format.
	PIXELFORMATDESCRIPTOR pfd =	
    {
		sizeof (PIXELFORMATDESCRIPTOR),									// Size Of This Pixel Format Descriptor
		1,																// Version Number
		PFD_DRAW_TO_WINDOW |											// Format Must Support Window
		PFD_SUPPORT_OPENGL |											// Format Must Support OpenGL
		PFD_DOUBLEBUFFER,												// Must Support Double Buffering
		PFD_TYPE_RGBA,													// Request An RGBA Format
		24,		                        								// Select Our Color Depth
		0, 0, 0, 0, 0, 0,												// Color Bits Ignored
		1,																// Alpha Buffer
		0,																// Shift Bit Ignored
		0,																// No Accumulation Buffer
		0, 0, 0, 0,														// Accumulation Bits Ignored
		16,																// 16Bit Z-Buffer (Depth Buffer)  
		0,																// No Stencil Buffer
		0,																// No Auxiliary Buffer
		PFD_MAIN_PLANE,													// Main Drawing Layer
		0,																// Reserved
		0, 0, 0															// Layer Masks Ignored
	};

    HWND hWnd = CreateWindow("window", "", 0, 0, 0, 100, 100, NULL, NULL, GetModuleHandle(NULL), NULL);
    debugAssert(hWnd);

    HDC  hDC  = GetDC(hWnd);
    debugAssert(hDC);
    
	int pixelFormat = ChoosePixelFormat(hDC, &pfd);
    debugAssert(pixelFormat);

    if (SetPixelFormat(hDC, pixelFormat, &pfd) == FALSE) {
        debugAssertM(false, "Failed to set pixel format");
	}

	HGLRC hRC = wglCreateContext(hDC);
    debugAssert(hRC);

	if (wglMakeCurrent(hDC, hRC) == FALSE)	{
        debugAssertM(false, "Failed to set context");
	}

    // We've now brought OpenGL online.  Grab the pointers we need and 
    // destroy everything.

    wglChoosePixelFormatARB =
        (PFNWGLCHOOSEPIXELFORMATARBPROC)wglGetProcAddress("wglChoosePixelFormatARB");

    PFNWGLGETEXTENSIONSSTRINGARBPROC wglGetExtensionsStringARB =
        (PFNWGLGETEXTENSIONSSTRINGARBPROC)wglGetProcAddress("wglGetExtensionsStringARB");

    if (wglGetExtensionsStringARB != NULL) {
        
        std::string wglExtensions = wglGetExtensionsStringARB(hDC);

        std::istringstream extensionsStream;
        extensionsStream.str(wglExtensions.c_str());
        
        std::string extension;
        while ( extensionsStream >> extension ) {
            if (extension == "WGL_ARB_multisample") {
                hasWGLMultiSampleSupport = true;
                break;
            }
        }

    } else {
        hasWGLMultiSampleSupport = false;
    }

    // Now destroy the dummy windows
    wglDeleteContext(hRC);					
    hRC = 0;	
	ReleaseDC(hWnd, hDC);	
	hWnd = 0;				
	DestroyWindow(hWnd);			
	hWnd = 0;
}


void Win32Window::createShareWindow(GWindow::Settings settings) {
	static bool init = false;
	if (init) {
		return;
	}
	
	init = true;	

	// We want a small (low memory), invisible window
	settings.visible = false;
	settings.width = 16;
	settings.height = 16;
	settings.framed = false;

	// This call will force us to re-enter createShareWindow, however
	// the second time through init will be true, so we'll skip the 
	// recursion.
#	if defined(_MSC_VER) && (_MSC_VER <= 1200)
		// VC6 doesn't have a "reset" method on auto_ptr.  This statement 
		// accomplishes the same purpose for that implementation, however.
		// Morgan verified that the source correctly passes ownership from the
		// newly created auto_ptr to the old one.
		_shareWindow = std::auto_ptr<Win32Window>(new Win32Window(settings, true));
#	else
		_shareWindow.reset(new Win32Window(settings, true));
#	endif
}


void Win32Window::reallyMakeCurrent() const {
	debugAssertM(_thread == ::GetCurrentThread(), 
		"Cannot call GWindow::makeCurrent on different threads.");

	if (wglMakeCurrent(_hDC, _glContext) == FALSE)	{
        debugAssertM(false, "Failed to set context");
	}
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
    Static helper functions for Win32Window
*/


/** Changes the screen resolution */
static bool ChangeResolution(int width, int height, int bpp, int refreshRate) {

	if (refreshRate == 0) {
		refreshRate = 85;
	}

    DEVMODE deviceMode;

    ZeroMemory(&deviceMode, sizeof(DEVMODE));

    int bppTries[3];
    bppTries[0] = bpp;
    bppTries[1] = 32;
    bppTries[2] = 16;

    deviceMode.dmSize       = sizeof(DEVMODE);
	deviceMode.dmPelsWidth  = width;
	deviceMode.dmPelsHeight = height;
	deviceMode.dmFields     = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY;
    deviceMode.dmDisplayFrequency = refreshRate;

    LONG result = -1;

    for (int i = 0; (i < 3) && (result != DISP_CHANGE_SUCCESSFUL); ++i) {
    	deviceMode.dmBitsPerPel = bppTries[i];
        result = ChangeDisplaySettings(&deviceMode, CDS_FULLSCREEN);
    }

    if (result != DISP_CHANGE_SUCCESSFUL) {
        // If it didn't work, try just changing the resolution and not the
        // refresh rate.
        deviceMode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
        for (int i = 0; (i < 3) && (result != DISP_CHANGE_SUCCESSFUL); ++i) {
    	    deviceMode.dmBitsPerPel = bppTries[i];
            result = ChangeDisplaySettings(&deviceMode, CDS_FULLSCREEN);
        }
    }

    return result == DISP_CHANGE_SUCCESSFUL;
}


static void makeKeyEvent(int vkCode, int lParam, GEvent& e) {

    // If true, we're looking at the right hand version of
    // Fix VK_SHIFT, VK_CONTROL, VK_MENU
    bool extended = (lParam >> 24) & 0x01;

    // Check for normal letter event
    if ((vkCode >= 'A') && (vkCode <= 'Z')) {

        // Make key codes lower case canonically
        e.key.keysym.sym = (SDLKey)(vkCode - 'A' + 'a');

    } else if (vkCode == VK_SHIFT) {

        e.key.keysym.sym = (SDLKey)(extended ? SDLK_RSHIFT : SDLK_LSHIFT);

    } else if (vkCode == VK_CONTROL) {

        e.key.keysym.sym = (SDLKey)(extended ? SDLK_RCTRL : SDLK_LCTRL);

    } else if (vkCode == VK_MENU) {

        e.key.keysym.sym = (SDLKey)(extended ? SDLK_RALT : SDLK_LALT);

    } else {

        e.key.keysym.sym = (SDLKey)_sdlKeys[iClamp(vkCode, 0, SDLK_LAST)];

    }

    e.key.keysym.scancode = MapVirtualKey(vkCode, 0); 
        //(lParam >> 16) & 0x7F;

    static BYTE lpKeyState[256];
    GetKeyboardState(lpKeyState);

    int mod = 0;
    if (lpKeyState[VK_LSHIFT] & 0x80) {
        mod = mod | KMOD_LSHIFT;
    }

    if (lpKeyState[VK_RSHIFT] & 0x80) {
        mod = mod | KMOD_RSHIFT;
    }

    if (lpKeyState[VK_LCONTROL] & 0x80) {
        mod = mod | KMOD_LCTRL;
    }

    if (lpKeyState[VK_RCONTROL] & 0x80) {
        mod = mod | KMOD_RCTRL;
    }

    if (lpKeyState[VK_LMENU] & 0x80) {
        mod = mod | KMOD_LALT;
    }

    if (lpKeyState[VK_RMENU] & 0x80) {
        mod = mod | KMOD_RALT;
    }
    e.key.keysym.mod = (SDLMod)mod;

    ToUnicode(vkCode, e.key.keysym.scancode, lpKeyState, (LPWSTR)&e.key.keysym.unicode, 1, 0);
}


/** 
 Configures a mouse up/down event
 */
static void mouseButton(bool down, int keyEvent, DWORD flags, GEvent& e) {
    (void)flags;
    // TODO: process flags
	if (down) {
		e.key.type  = SDL_KEYDOWN;
		e.key.state = SDL_PRESSED;
	} else {
		e.key.type  = SDL_KEYUP;
		e.key.state = SDL_RELEASED;
	}

    e.key.keysym.unicode = ' ';
	e.key.keysym.sym = (SDLKey)keyEvent;


    e.key.keysym.scancode = 0;

    static BYTE lpKeyState[256];
    GetKeyboardState(lpKeyState);

    int mod = 0;
    if (lpKeyState[VK_LSHIFT] & 0x80) {
        mod = mod | KMOD_LSHIFT;
    }

    if (lpKeyState[VK_RSHIFT] & 0x80) {
        mod = mod | KMOD_RSHIFT;
    }

    if (lpKeyState[VK_LCONTROL] & 0x80) {
        mod = mod | KMOD_LCTRL;
    }

    if (lpKeyState[VK_RCONTROL] & 0x80) {
        mod = mod | KMOD_RCTRL;
    }

    if (lpKeyState[VK_LMENU] & 0x80) {
        mod = mod | KMOD_LALT;
    }

    if (lpKeyState[VK_RMENU] & 0x80) {
        mod = mod | KMOD_RALT;
    }
    e.key.keysym.mod = (SDLMod)mod;
}


/**
 Initializes SDL to Win32 key map
 */
static void initWin32KeyMap() {
    memset(_sdlKeys, 0, sizeof(_sdlKeys));

	_sdlKeys[VK_BACK] = SDLK_BACKSPACE;
	_sdlKeys[VK_TAB] = SDLK_TAB;
	_sdlKeys[VK_CLEAR] = SDLK_CLEAR;
	_sdlKeys[VK_RETURN] = SDLK_RETURN;
	_sdlKeys[VK_PAUSE] = SDLK_PAUSE;
	_sdlKeys[VK_ESCAPE] = SDLK_ESCAPE;
	_sdlKeys[VK_SPACE] = SDLK_SPACE;
	_sdlKeys[VK_APOSTROPHE] = SDLK_QUOTE;
	_sdlKeys[VK_COMMA] = SDLK_COMMA;
	_sdlKeys[VK_MINUS] = SDLK_MINUS;
	_sdlKeys[VK_PERIOD] = SDLK_PERIOD;
	_sdlKeys[VK_SLASH] = SDLK_SLASH;
	_sdlKeys['0'] = SDLK_0;
	_sdlKeys['1'] = SDLK_1;
	_sdlKeys['2'] = SDLK_2;
	_sdlKeys['3'] = SDLK_3;
	_sdlKeys['4'] = SDLK_4;
	_sdlKeys['5'] = SDLK_5;
	_sdlKeys['6'] = SDLK_6;
	_sdlKeys['7'] = SDLK_7;
	_sdlKeys['8'] = SDLK_8;
	_sdlKeys['9'] = SDLK_9;
	_sdlKeys[VK_SEMICOLON] = SDLK_SEMICOLON;
	_sdlKeys[VK_EQUALS] = SDLK_EQUALS;
	_sdlKeys[VK_LBRACKET] = SDLK_LEFTBRACKET;
	_sdlKeys[VK_BACKSLASH] = SDLK_BACKSLASH;
	_sdlKeys[VK_RBRACKET] = SDLK_RIGHTBRACKET;
	_sdlKeys[VK_GRAVE] = SDLK_BACKQUOTE;
	_sdlKeys[VK_BACKTICK] = SDLK_BACKQUOTE;
	_sdlKeys[VK_DELETE] = SDLK_DELETE;

	_sdlKeys[VK_NUMPAD0] = SDLK_KP0;
	_sdlKeys[VK_NUMPAD1] = SDLK_KP1;
	_sdlKeys[VK_NUMPAD2] = SDLK_KP2;
	_sdlKeys[VK_NUMPAD3] = SDLK_KP3;
	_sdlKeys[VK_NUMPAD4] = SDLK_KP4;
	_sdlKeys[VK_NUMPAD5] = SDLK_KP5;
	_sdlKeys[VK_NUMPAD6] = SDLK_KP6;
	_sdlKeys[VK_NUMPAD7] = SDLK_KP7;
	_sdlKeys[VK_NUMPAD8] = SDLK_KP8;
	_sdlKeys[VK_NUMPAD9] = SDLK_KP9;
	_sdlKeys[VK_DECIMAL] = SDLK_KP_PERIOD;
	_sdlKeys[VK_DIVIDE] = SDLK_KP_DIVIDE;
	_sdlKeys[VK_MULTIPLY] = SDLK_KP_MULTIPLY;
	_sdlKeys[VK_SUBTRACT] = SDLK_KP_MINUS;
	_sdlKeys[VK_ADD] = SDLK_KP_PLUS;

	_sdlKeys[VK_UP] = SDLK_UP;
	_sdlKeys[VK_DOWN] = SDLK_DOWN;
	_sdlKeys[VK_RIGHT] = SDLK_RIGHT;
	_sdlKeys[VK_LEFT] = SDLK_LEFT;
	_sdlKeys[VK_INSERT] = SDLK_INSERT;
	_sdlKeys[VK_HOME] = SDLK_HOME;
	_sdlKeys[VK_END] = SDLK_END;
	_sdlKeys[VK_PRIOR] = SDLK_PAGEUP;
	_sdlKeys[VK_NEXT] = SDLK_PAGEDOWN;

	_sdlKeys[VK_F1] = SDLK_F1;
	_sdlKeys[VK_F2] = SDLK_F2;
	_sdlKeys[VK_F3] = SDLK_F3;
	_sdlKeys[VK_F4] = SDLK_F4;
	_sdlKeys[VK_F5] = SDLK_F5;
	_sdlKeys[VK_F6] = SDLK_F6;
	_sdlKeys[VK_F7] = SDLK_F7;
	_sdlKeys[VK_F8] = SDLK_F8;
	_sdlKeys[VK_F9] = SDLK_F9;
	_sdlKeys[VK_F10] = SDLK_F10;
	_sdlKeys[VK_F11] = SDLK_F11;
	_sdlKeys[VK_F12] = SDLK_F12;
	_sdlKeys[VK_F13] = SDLK_F13;
	_sdlKeys[VK_F14] = SDLK_F14;
	_sdlKeys[VK_F15] = SDLK_F15;

	_sdlKeys[VK_NUMLOCK] = SDLK_NUMLOCK;
	_sdlKeys[VK_CAPITAL] = SDLK_CAPSLOCK;
	_sdlKeys[VK_SCROLL] = SDLK_SCROLLOCK;
	_sdlKeys[VK_RSHIFT] = SDLK_RSHIFT;
	_sdlKeys[VK_LSHIFT] = SDLK_LSHIFT;
	_sdlKeys[VK_RCONTROL] = SDLK_RCTRL;
	_sdlKeys[VK_LCONTROL] = SDLK_LCTRL;
	_sdlKeys[VK_RMENU] = SDLK_RALT;
	_sdlKeys[VK_LMENU] = SDLK_LALT;
	_sdlKeys[VK_RWIN] = SDLK_RSUPER;
	_sdlKeys[VK_LWIN] = SDLK_LSUPER;

	_sdlKeys[VK_HELP] = SDLK_HELP;
	_sdlKeys[VK_PRINT] = SDLK_PRINT;
	_sdlKeys[VK_SNAPSHOT] = SDLK_PRINT;
	_sdlKeys[VK_CANCEL] = SDLK_BREAK;
	_sdlKeys[VK_APPS] = SDLK_MENU;

    sdlKeysInitialized = true;
}


#if 0
static void printPixelFormatDescription(int format, HDC hdc, TextOutput& out) {

    PIXELFORMATDESCRIPTOR pixelFormat;
    DescribePixelFormat(hdc, format, sizeof(PIXELFORMATDESCRIPTOR), &pixelFormat);

    /*
    typedef struct tagPIXELFORMATDESCRIPTOR { // pfd   
      WORD  nSize; 
      WORD  nVersion; 
      DWORD dwFlags; 
      BYTE  iPixelType; 
      BYTE  cColorBits; 
      BYTE  cRedBits; 
      BYTE  cRedShift; 
      BYTE  cGreenBits; 
      BYTE  cGreenShift; 
      BYTE  cBlueBits; 
      BYTE  cBlueShift; 
      BYTE  cAlphaBits; 
      BYTE  cAlphaShift; 
      BYTE  cAccumBits; 
      BYTE  cAccumRedBits; 
      BYTE  cAccumGreenBits; 
      BYTE  cAccumBlueBits; 
      BYTE  cAccumAlphaBits; 
      BYTE  cDepthBits; 
      BYTE  cStencilBits; 
      BYTE  cAuxBuffers; 
      BYTE  iLayerType; 
      BYTE  bReserved; 
      DWORD dwLayerMask; 
      DWORD dwVisibleMask; 
      DWORD dwDamageMask; 
    } PIXELFORMATDESCRIPTOR; 
    */

    out.printf("#%d Format Description\n", format);
    out.printf("nSize:\t\t\t\t%d\n", pixelFormat.nSize);
    out.printf("nVersion:\t\t\t%d\n", pixelFormat.nVersion);
    std::string s = (std::string((pixelFormat.dwFlags&PFD_DRAW_TO_WINDOW) ? "PFD_DRAW_TO_WINDOW|" : "") + 
                                     std::string((pixelFormat.dwFlags&PFD_DRAW_TO_BITMAP) ? "PFD_DRAW_TO_BITMAP|" : "") + 
                                     std::string((pixelFormat.dwFlags&PFD_SUPPORT_GDI) ? "PFD_SUPPORT_GDI|" : "") + 
                                     std::string((pixelFormat.dwFlags&PFD_SUPPORT_OPENGL) ? "PFD_SUPPORT_OPENGL|" : "") + 
                                     std::string((pixelFormat.dwFlags&PFD_GENERIC_ACCELERATED) ? "PFD_GENERIC_ACCELERATED|" : "") + 
                                     std::string((pixelFormat.dwFlags&PFD_GENERIC_FORMAT) ? "PFD_GENERIC_FORMAT|" : "") + 
                                     std::string((pixelFormat.dwFlags&PFD_NEED_PALETTE) ? "PFD_NEED_PALETTE|" : "") + 
                                     std::string((pixelFormat.dwFlags&PFD_NEED_SYSTEM_PALETTE) ? "PFD_NEED_SYSTEM_PALETTE|" : "") + 
                                     std::string((pixelFormat.dwFlags&PFD_DOUBLEBUFFER) ? "PFD_DOUBLEBUFFER|" : "") + 
                                     std::string((pixelFormat.dwFlags&PFD_STEREO) ? "PFD_STEREO|" : "") +
                                     std::string((pixelFormat.dwFlags&PFD_SWAP_LAYER_BUFFERS) ? "PFD_SWAP_LAYER_BUFFERS" : ""));

    out.printf("dwFlags:\t\t\t%s\n", s.c_str());
    out.printf("iPixelType:\t\t\t%d\n", pixelFormat.iPixelType);
    out.printf("cColorBits:\t\t\t%d\n", pixelFormat.cColorBits);
    out.printf("cRedBits:\t\t\t%d\n", pixelFormat.cRedBits);
    out.printf("cRedShift:\t\t\t%d\n", pixelFormat.cRedShift);
    out.printf("cGreenBits:\t\t\t%d\n", pixelFormat.cGreenBits);
    out.printf("cGreenShift:\t\t\t%d\n", pixelFormat.cGreenShift);
    out.printf("cBlueBits:\t\t\t%d\n", pixelFormat.cBlueBits);
    out.printf("cBlueShift:\t\t\t%d\n", pixelFormat.cBlueShift);
    out.printf("cAlphaBits:\t\t\t%d\n", pixelFormat.cAlphaBits);
    out.printf("cAlphaShift:\t\t\t%d\n", pixelFormat.cAlphaShift);
    out.printf("cAccumBits:\t\t\t%d\n", pixelFormat.cAccumBits);
    out.printf("cAccumRedBits:\t\t\t%d\n", pixelFormat.cAccumRedBits);
    out.printf("cAccumGreenBits:\t\t%d\n", pixelFormat.cAccumGreenBits);
    out.printf("cAccumBlueBits:\t\t\t%d\n", pixelFormat.cAccumBlueBits);
    out.printf("cAccumAlphaBits:\t\t%d\n", pixelFormat.cAccumAlphaBits);
    out.printf("cDepthBits:\t\t\t%d\n", pixelFormat.cDepthBits);
    out.printf("cStencilBits:\t\t\t%d\n", pixelFormat.cStencilBits);
    out.printf("cAuxBuffers:\t\t\t%d\n", pixelFormat.cAuxBuffers);
    out.printf("iLayerType:\t\t\t%d\n", pixelFormat.iLayerType);
    out.printf("bReserved:\t\t\t%d\n", pixelFormat.bReserved);
    out.printf("dwLayerMask:\t\t\t%d\n", pixelFormat.dwLayerMask);
    out.printf("dwDamageMask:\t\t\t%d\n", pixelFormat.dwDamageMask);

    out.printf("\n");
}
#endif

namespace _internal{
static LRESULT WINAPI window_proc(
    HWND                window,
    UINT                message,
    WPARAM              wparam,
    LPARAM              lparam) {
    
    Win32Window* this_window = (Win32Window*)GetWindowLong(window, GWL_USERDATA);
    
    if (this_window != NULL) {
        switch (message) {
        case WM_ACTIVATE:
            if ((LOWORD(wparam) != WA_INACTIVE) &&
                (HIWORD(wparam) == 0) &&
                ((HWND)lparam != this_window->hwnd())) { // non-zero is minimized 
                this_window->_windowActive = true;
            } else if ((HWND)lparam != this_window->hwnd()) {
                this_window->_windowActive = false;
            }
            break;

        case WM_CLOSE:
            this_window->_receivedCloseEvent = true;
			break;

        case WM_SIZE:
            if ((wparam == SIZE_MAXIMIZED) ||
                (wparam == SIZE_RESTORED)) {
                this_window->injectSizeEvent(LOWORD(lparam), HIWORD(lparam));
            }
            break;

        case WM_SETFOCUS:
            this_window->justReceivedFocus = true;
            break;

        case WM_KILLFOCUS:
            for (int i = 0; i < sizeof(this_window->_keyboardButtons); ++i) {
                if (this_window->_keyboardButtons[i]) {
                    ::PostMessage(window, WM_KEYUP, i, 0);
                }
            }
            memset(this_window->_keyboardButtons, 0, sizeof(this_window->_keyboardButtons));
            break;

        case WM_SYSCOMMAND:
            // Only upper 12-bits are public
            if ((wparam & 0xFFF0) == SC_KEYMENU) {
                // Ignore Alt button that opens system menu, freezes render
                return 0;
            }
            break;
        }
    }
    
    return DefWindowProc(window, message, wparam, lparam);
}
}

static const char* G3DWndClass() {

    static char const* g3dWindowClassName = NULL;

    if (g3dWindowClassName == NULL) {
        
        WNDCLASS wndcls;
        
        wndcls.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS | CS_OWNDC;
        wndcls.lpfnWndProc = _internal::window_proc;
        wndcls.cbClsExtra = wndcls.cbWndExtra = 0;
        wndcls.hInstance = ::GetModuleHandle(NULL);
        wndcls.hIcon = NULL;
        wndcls.hCursor = ::LoadCursor(NULL, IDC_ARROW);
        wndcls.hbrBackground = NULL;
        wndcls.lpszMenuName = NULL;
        wndcls.lpszClassName = "G3DWindow";
        
        if (!RegisterClass(&wndcls)) {
            Log::common()->printf("\n**** WARNING: could not create G3DWindow class ****\n");
            // error!  Return the default window class.
            return "window";
        }
        
        g3dWindowClassName = wndcls.lpszClassName;        
    }
    
    return g3dWindowClassName;
}

} // G3D namespace

#endif // G3D_WIN32
