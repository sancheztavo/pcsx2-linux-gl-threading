/*
 *	Copyright (C) 2007-2012 Gabest
 *	http://www.gabest.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "PrecompiledHeader.h"
#include "GSWndOGL.h"

#if defined(__unix__)
GSWndOGL::GSWndOGL()
	: m_NativeWindow(0), m_NativeDisplay(nullptr), m_context(0), m_has_late_vsync(false), m_swapinterval_ext(nullptr), m_swapinterval_mesa(nullptr)
{
}

static bool ctxError = false;
static int  ctxErrorHandler(Display *dpy, XErrorEvent *ev)
{
	ctxError = true;
	return 0;
}

void GSWndOGL::CreateContext(int major, int minor)
{
	if ( !m_NativeDisplay || !m_NativeWindow )
	{
		fprintf( stderr, "Wrong X11 display/window\n" );
		throw GSRecoverableError();
	}

	// Get visual information
	static int attrListDbl[] =
	{
		// GLX_X_RENDERABLE: If True is specified, then only frame buffer configurations that have associated X
		// visuals (and can be used to render to Windows and/or GLX pixmaps) will be considered. The default value is GLX_DONT_CARE.
		GLX_X_RENDERABLE    , True,
		GLX_RED_SIZE        , 8,
		GLX_GREEN_SIZE      , 8,
		GLX_BLUE_SIZE       , 8,
		GLX_DEPTH_SIZE      , 0,
		GLX_DOUBLEBUFFER    , True,
		None
	};

	PFNGLXCHOOSEFBCONFIGPROC glX_ChooseFBConfig = (PFNGLXCHOOSEFBCONFIGPROC) glXGetProcAddress((GLubyte *) "glXChooseFBConfig");
	int fbcount = 0;
	GLXFBConfig *fbc = glX_ChooseFBConfig(m_NativeDisplay, DefaultScreen(m_NativeDisplay), attrListDbl, &fbcount);
	if (!fbc || fbcount < 1) {
		throw GSRecoverableError();
	}

	PFNGLXCREATECONTEXTATTRIBSARBPROC glX_CreateContextAttribsARB = (PFNGLXCREATECONTEXTATTRIBSARBPROC)glXGetProcAddress((const GLubyte*) "glXCreateContextAttribsARB");
	if (!glX_CreateContextAttribsARB) {
		throw GSRecoverableError();
	}

	// Install a dummy handler to handle gracefully (aka not segfault) the support of GL version
	int (*oldHandler)(Display*, XErrorEvent*) = XSetErrorHandler(&ctxErrorHandler);
	// Be sure the handler is installed
	XSync( m_NativeDisplay, false);

	// Create a context
	int context_attribs[] =
	{
		GLX_CONTEXT_MAJOR_VERSION_ARB, major,
		GLX_CONTEXT_MINOR_VERSION_ARB, minor,
#ifdef ENABLE_OGL_DEBUG
		GLX_CONTEXT_FLAGS_ARB, GLX_CONTEXT_DEBUG_BIT_ARB,
#else
		// Open Source isn't happy with an unsupported flags...
		//GLX_CONTEXT_FLAGS_ARB, GL_CONTEXT_FLAG_NO_ERROR_BIT_KHR,
#endif
		GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
		None
	};

	m_context = glX_CreateContextAttribsARB(m_NativeDisplay, fbc[0], 0, true, context_attribs);
	XFree(fbc);

	// Don't forget to reinstall the older Handler
	XSetErrorHandler(oldHandler);

	// Get latest error
	XSync( m_NativeDisplay, false);

	if (!m_context || ctxError) {
		fprintf(stderr, "Failed to create the opengl context. Check your drivers support openGL %d.%d. Hint: opensource drivers don't\n", major, minor );
		throw GSRecoverableError();
	}
}

void GSWndOGL::AttachContext()
{
	if (!IsContextAttached()) {
		//fprintf(stderr, "Attach the context\n");
		glXMakeCurrent(m_NativeDisplay, m_NativeWindow, m_context);
		m_ctx_attached = true;
	}
}

void GSWndOGL::DetachContext()
{
	if (IsContextAttached()) {
		//fprintf(stderr, "Detach the context\n");
		glXMakeCurrent(m_NativeDisplay, None, NULL);
		m_ctx_attached = false;
	}
}

void GSWndOGL::PopulateWndGlFunction()
{
	m_swapinterval_ext  = (PFNGLXSWAPINTERVALEXTPROC) glXGetProcAddress((const GLubyte*) "glXSwapIntervalEXT");
	m_swapinterval_mesa = (PFNGLXSWAPINTERVALMESAPROC)glXGetProcAddress((const GLubyte*) "glXSwapIntervalMESA");

	const char* ext = glXQueryExtensionsString(m_NativeDisplay, DefaultScreen(m_NativeDisplay));
	m_has_late_vsync = m_swapinterval_ext && ext && strstr(ext, "GLX_EXT_swap_control");
}

bool GSWndOGL::Attach(void* handle, bool managed)
{
	m_NativeWindow = *(Window*)handle;
	m_managed = managed;

	m_NativeDisplay = XOpenDisplay(NULL);

	FullContextInit();

	return true;
}

void GSWndOGL::Detach()
{
	// Actually the destructor is not called when there is only a GSclose/GSshutdown
	// The window still need to be closed
	DetachContext();
	if (m_context) glXDestroyContext(m_NativeDisplay, m_context);

	if (m_NativeDisplay) {
		XCloseDisplay(m_NativeDisplay);
		m_NativeDisplay = NULL;
	}
}

bool GSWndOGL::Create(const std::string& title, int w, int h)
{
	if(m_NativeWindow)
		throw GSRecoverableError();

	if(w <= 0 || h <= 0) {
		w = theApp.GetConfigI("ModeWidth");
		h = theApp.GetConfigI("ModeHeight");
	}

	m_managed = true;

	// note this part must be only executed when replaying .gs debug file
	m_NativeDisplay = XOpenDisplay(NULL);

	m_NativeWindow = XCreateSimpleWindow(m_NativeDisplay, DefaultRootWindow(m_NativeDisplay), 0, 0, w, h, 0, 0, 0);
	XMapWindow (m_NativeDisplay, m_NativeWindow);

	if (m_NativeWindow == 0)
		throw GSRecoverableError();

	FullContextInit();

	return true;
}

void* GSWndOGL::GetProcAddress(const char* name, bool opt)
{
	void* ptr = (void*)glXGetProcAddress((const GLubyte*)name);
	if (ptr == NULL) {
		if (theApp.GetConfigB("debug_opengl"))
			fprintf(stderr, "Failed to find %s\n", name);

		if (!opt)
			throw GSRecoverableError();
	}
	return ptr;
}

void* GSWndOGL::GetDisplay()
{
	// note this part must be only executed when replaying .gs debug file
	return (void*)m_NativeDisplay;
}

GSVector4i GSWndOGL::GetClientRect()
{
	unsigned int h = 480;
	unsigned int w = 640;

	unsigned int borderDummy;
	unsigned int depthDummy;
	Window winDummy;
    int xDummy;
    int yDummy;

	if (!m_NativeDisplay) m_NativeDisplay = XOpenDisplay(NULL);
	XGetGeometry(m_NativeDisplay, m_NativeWindow, &winDummy, &xDummy, &yDummy, &w, &h, &borderDummy, &depthDummy);

	return GSVector4i(0, 0, (int)w, (int)h);
}

// Returns FALSE if the window has no title, or if th window title is under the strict
// management of the emulator.

bool GSWndOGL::SetWindowText(const char* title)
{
	if (!m_managed) return true;

	XTextProperty prop;

	memset(&prop, 0, sizeof(prop));

	char* ptitle = (char*)title;
	if (XStringListToTextProperty(&ptitle, 1, &prop)) {
		XSetWMName(m_NativeDisplay, m_NativeWindow, &prop);
	}

	XFree(prop.value);
	XFlush(m_NativeDisplay);

	return true;
}

void GSWndOGL::SetSwapInterval()
{
	// m_swapinterval uses an integer as parameter
	// 0 -> disable vsync
	// n -> wait n frame
	if      (m_swapinterval_ext)  m_swapinterval_ext(m_NativeDisplay, m_NativeWindow, m_vsync);
	else if (m_swapinterval_mesa) m_swapinterval_mesa(m_vsync);
	else						 fprintf(stderr, "Failed to set VSync\n");
}

void GSWndOGL::Flip()
{
	if (m_vsync_change_requested.exchange(false))
		SetSwapInterval();

	glXSwapBuffers(m_NativeDisplay, m_NativeWindow);
}

void GSWndOGL::Show()
{
	XMapRaised(m_NativeDisplay, m_NativeWindow);
	XFlush(m_NativeDisplay);
}

void GSWndOGL::Hide()
{
	XUnmapWindow(m_NativeDisplay, m_NativeWindow);
	XFlush(m_NativeDisplay);
}

void GSWndOGL::HideFrame()
{
	// TODO
}

#endif
