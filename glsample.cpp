//
// GLSAMPLE.CPP
//  by Blaine Hodge
//

// Includes

#include <windows.h>
#include <gl/gl.h>
//#include <gl/glew.h> //is using glew a good option for loading the necessary extension to allow rendering to texture ?
#include "dependencies\Wyphon.h"
#include "dependencies\WyphonUtils.h"


using namespace Wyphon;
using namespace WyphonUtils;


// GLOBAL VARIABLES
//ID3D11Texture2D*		g_pSharedTexture = NULL; //A texture we want to share with the rest of the world !!!
HANDLE					g_hWyphonPartner;
HWND                    g_hWnd_D3D9Ex = NULL;
HANDLE					g_hWyphonUtilsDevice;

bool					is64bit = ( sizeof(int*) == 8 );


struct TextureInfo {
	HANDLE hSharedTexture;
	unsigned __int32 width;
	unsigned __int32 height;
	DWORD format;	//D3DFMT_xxx constants like D3DFMT_A8R8G8B8 (http://msdn.microsoft.com/en-us/library/windows/desktop/bb172558(v=vs.85).aspx)
	DWORD usage;	//D3DUSAGE_xxx constants like D3DUSAGE_RENDERTARGET (http://msdn.microsoft.com/en-us/library/windows/desktop/bb172625(v=vs.85).aspx)
	wchar_t description[WYPHON_MAX_DESCRIPTION_LENGTH + 1];
		
	unsigned __int32 partnerId; //id of partner that shared it with us
};


TextureInfo				g_sharedTextureInfo;
bool					g_sharedTextureInfoIsInvalid = false;
//HANDLE					g_hTextureInfoMutex; //to protect the textureInfo


// Function Declarations

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
void EnableOpenGL(HWND hWnd, HDC * hDC, HGLRC * hRC);
void DisableOpenGL(HWND hWnd, HDC hDC, HGLRC hRC);





void TextureSharingStartedCALLBACK( HANDLE wyphonPartnerHandle, unsigned __int32 sendingPartnerId, HANDLE sharedTextureHandle, unsigned __int32 width, unsigned __int32 height, DWORD format, DWORD usage, LPTSTR description, void * customData ) {
	//DWORD result = WaitForSingleObject(g_hTextureInfoMutex, 999999 );
	
	//if ( result == WAIT_OBJECT_0 ) {
	//	try {
			//if the invalidTextureInfo has not been cleaned up, don't accept new textures
			if ( g_sharedTextureInfo.hSharedTexture == NULL ) {
				g_sharedTextureInfo.partnerId = sendingPartnerId;
				g_sharedTextureInfo.width = width;
				g_sharedTextureInfo.height = height;
				g_sharedTextureInfo.format = format;
				g_sharedTextureInfo.usage = usage;
				g_sharedTextureInfo.hSharedTexture = sharedTextureHandle;
			}
	//	}
	//	catch (...) {}

	//	ReleaseMutex( g_hTextureInfoMutex );
	//}
}


void TextureSharingStoppedCALLBACK( HANDLE wyphonPartnerHandle, unsigned __int32 sendingPartnerId, HANDLE sharedTextureHandle, unsigned __int32 width, unsigned __int32 height, DWORD format, DWORD usage, LPTSTR description, void * customData ) {
	//if ( WAIT_OBJECT_0 == WaitForSingleObject(g_hTextureInfoMutex, INFINITE ) ) {
	//	try {
			//if the invalidTextureInfo has not been cleaned up, don't accept new textures
			if ( g_sharedTextureInfo.hSharedTexture == sharedTextureHandle ) {
				g_sharedTextureInfoIsInvalid = true;
			}
	//	}
	//	catch (...) {}

	//	ReleaseMutex( g_hTextureInfoMutex );
	//}
}





// WinMain

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, 
				   LPSTR lpCmdLine, int iCmdShow)
{
	WNDCLASS wc;
	HWND hWnd;
	HDC hDC;
	HGLRC hRC;
	MSG msg;
	BOOL quit = FALSE;
	float theta = 0.0f;
	
	// register window class
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon( NULL, IDI_APPLICATION );
	wc.hCursor = LoadCursor( NULL, IDC_ARROW );
	wc.hbrBackground = (HBRUSH)GetStockObject( BLACK_BRUSH );
	wc.lpszMenuName = NULL;
	wc.lpszClassName = "GLSample";
	RegisterClass( &wc );
	

	unsigned __int32 windowWidth = 512;
	unsigned __int32 windowHeight = 512;


	// create main window
	hWnd = CreateWindow( 
		"GLSample", "OpenGL Sample", 
		WS_CAPTION | WS_POPUPWINDOW | WS_VISIBLE,
		0, 0, windowWidth, windowHeight,
		NULL, NULL, hInstance, NULL );
	
	// enable OpenGL for the window
	EnableOpenGL( hWnd, &hDC, &hRC );
	





	///////////////////
	//// SETUP Wyphon
	///////////////////

	g_sharedTextureInfo.hSharedTexture = NULL;
	g_sharedTextureInfoIsInvalid = false;
	//g_hTextureInfoMutex = CreateMutex( NULL, FALSE, NULL );

	//no callbacks since we only want to share textures, we're not interested in info of other partners
	g_hWyphonPartner = CreateWyphonPartner( is64bit ? TEXT( "WyphonGL sender test 64 BIT" ) : TEXT( "WyphonGL sender test 32 BIT" )
								, NULL
								, NULL, NULL, TextureSharingStartedCALLBACK, TextureSharingStoppedCALLBACK
	 						);

	////////////////////////////////////////////////////////////////
	// THIS APPLICAION WILL SHOW THE FIRST TEXTURE IT GETS FROM ANY 
	// OTHER WYPHON PARTNER, ROTATING TOGETHER WITH A COLORED TRIANGLE
	//
	// IT WILL SHARE ITS OWN SCREEN OUTPUT WITH ALL OTHER WYPHON PARTNERS
	////////////////////////////////////////////////////////////////

	//Initialize WyphonUtils
	g_hWyphonUtilsDevice = InitDevice();


	// this texture is for sharing our screen output with the world
	HANDLE toWyphon_dxShareHandle = NULL;
	GLuint toWyphon_glTextureName = NULL;
	HANDLE toWyphon_glTextureHandle = NULL;
	PDIRECT3DTEXTURE9 toWyphon_d3d9texture;


	HRESULT hr;

	hr = CreateDX9ExTexture( windowWidth, windowHeight, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, & toWyphon_d3d9texture, & toWyphon_dxShareHandle );

	if ( hr == S_OK ) { 
		//create an OpenGL texture that is linked to a DX9ex texture, so it can be shared by Wyphon
		hr = CreateLinkedGLTexture( windowWidth, windowHeight, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, toWyphon_dxShareHandle, toWyphon_glTextureName, toWyphon_glTextureHandle );
		if ( hr == S_OK ) {
			//then use Wyphon to share this texture (don't forget to fill hat texture after each render pass, by copying the screen buffer to it!)
			ShareD3DTexture( g_hWyphonPartner, toWyphon_dxShareHandle, windowWidth, windowHeight, D3DFMT_A8R8G8B8, D3DUSAGE_RENDERTARGET, TEXT( "screen output" ) );
		}
		else {
			bool done = true; //just a line to put a breakpoint on :)
		}
	}
	//Check out http://www.glprogramming.com/red/chapter09.html for OpenGL 1.1 examples and tutorials.

	//this texture is the texture shared by someone else
	HANDLE fromWyphon_dxShareHandle = NULL;
	GLuint fromWyphon_glTextureName = NULL;
	HANDLE fromWyphon_glTextureHandle = NULL;
	//PDIRECT3DTEXTURE9 fromWyphon_d3d9texture = NULL;


	// program main loop
	while ( !quit )
	{
		
		// check for messages
		if ( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE )  )
		{
			
			// handle or dispatch messages
			if ( msg.message == WM_QUIT ) 
			{
				quit = TRUE;
			} 
			else 
			{
				TranslateMessage( &msg );
				DispatchMessage( &msg );
			}
			
		} 
		else 
		{
			
			// OpenGL animation code goes here
			Sleep( 20 );


			//if ( WAIT_OBJECT_0 == WaitForSingleObject( g_hTextureInfoMutex, INFINITE ) ) {
			//	try {

					if ( g_sharedTextureInfo.hSharedTexture != NULL ) {
						if ( fromWyphon_glTextureHandle == NULL ) {
							HRESULT hr;
						
							HANDLE h = g_sharedTextureInfo.hSharedTexture;
							hr = CreateLinkedGLTexture( g_sharedTextureInfo.width, g_sharedTextureInfo.height, g_sharedTextureInfo.usage, (D3DFORMAT) g_sharedTextureInfo.format, h, fromWyphon_glTextureName, fromWyphon_glTextureHandle );
							if ( hr == S_OK ) {
								int ok = 1;
							}
						}
						else if ( g_sharedTextureInfoIsInvalid ) {
							fromWyphon_dxShareHandle = NULL;
							fromWyphon_glTextureHandle = NULL;
							fromWyphon_glTextureName = 0;
							g_sharedTextureInfo.hSharedTexture = NULL;
							g_sharedTextureInfoIsInvalid = false;
						}
					}


					glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
					glClear( GL_COLOR_BUFFER_BIT );
			
					glPushMatrix();
					glRotatef( theta, 0.0f, 0.0f, 1.0f );
					glBegin( GL_TRIANGLES );
					glColor3f( 1.0f, 0.0f, 0.0f ); glVertex2f( 0.0f, 1.0f );
					glColor3f( 0.0f, 1.0f, 0.0f ); glVertex2f( 0.87f, -0.5f );
					glColor3f( 0.0f, 0.0f, 1.0f ); glVertex2f( -0.87f, -0.5f );
					glEnd();

					//enable texturemapping, ONLY IF WE HAVE A SHARED texture by another Wyphon partner
					if ( fromWyphon_glTextureHandle != NULL ) {
						glEnable(GL_TEXTURE_2D);
						glBindTexture( GL_TEXTURE_2D, fromWyphon_glTextureName );
						LockGLTexture( fromWyphon_glTextureHandle );
					}

					glBegin(GL_QUADS);

					glTexCoord2f( 0.0, 0.0 ); glVertex3f( 0.0,  0.0, 0.0 );
					glTexCoord2f( 0.0, 1.0 ); glVertex3f( 0.0,  1.0, 0.0 );
					glTexCoord2f( 1.0, 1.0 ); glVertex3f( 1.0,  1.0, 0.0 );
					glTexCoord2f( 1.0, 0.0 ); glVertex3f( 1.0,  0.0, 0.0 );

					glEnd();

					if ( fromWyphon_glTextureHandle != NULL ) {
						UnlockGLTexture( fromWyphon_glTextureHandle );
						glDisable(GL_TEXTURE_2D);
					}


					glPopMatrix();
			//	}
			//	catch(...) {
			//		int error = 1;
			//	}
			
			//	ReleaseMutex( g_hTextureInfoMutex );
			//}


			//copy the backbuffer to the texture that this applicaion shares itself
			///////////////////////////////////////////////////////////////////////
			glReadBuffer( GL_BACK );

			// lock and bind destination texture
			glBindTexture( GL_TEXTURE_2D, toWyphon_glTextureName );
			WyphonUtils::LockGLTexture( toWyphon_glTextureHandle );

			//glMatrixMode( GL_TEXTURE_MATRIX );
			//glPushMatrix();
			//glMatrixMode( GL_TEXTURE );
			//glLoadIdentity( );
			//glScalef( 1, -2, 1 );

			/// copy from framebuffer (here, the FBO!) to the bound texture
			//glCopyTexImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, windowWidth, windowHeight, 0 );
			glCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, windowWidth, windowHeight );

			//glPopMatrix();
			//glMatrixMode( GL_MODELVIEW );
			glBindTexture( GL_TEXTURE_2D, 0 );
			// unlock destination texture (but do not unbind it, we need it for drawing)
			WyphonUtils::UnlockGLTexture( toWyphon_glTextureHandle );



			SwapBuffers( hDC );



			theta += 1.0f;
			
		}
		
	}
	


	/////////////////////
	//// TEARDOWN Wyphon
	/////////////////////

	//WyphonUtils
	if ( g_hWyphonUtilsDevice != NULL ) {
		ReleaseDevice( g_hWyphonUtilsDevice );
	}
	if (g_hWyphonPartner != NULL) {
		DestroyWyphonPartner( g_hWyphonPartner );
	}

	//ReleaseMutex( g_hTextureInfoMutex );


	// shutdown OpenGL
	DisableOpenGL( hWnd, hDC, hRC );
	
	// destroy the window explicitly
	DestroyWindow( hWnd );
	
	return msg.wParam;
	
}

// Window Procedure

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	
	switch (message)
	{
		
	case WM_CREATE:
		return 0;
		
	case WM_CLOSE:
		PostQuitMessage( 0 );
		return 0;
		
	case WM_DESTROY:
		return 0;
		
	case WM_KEYDOWN:
		switch ( wParam )
		{
			
		case VK_ESCAPE:
			PostQuitMessage(0);
			return 0;
			
		}
		return 0;
	
	default:
		return DefWindowProc( hWnd, message, wParam, lParam );
			
	}
	
}

// Enable OpenGL

void EnableOpenGL(HWND hWnd, HDC * hDC, HGLRC * hRC)
{
	PIXELFORMATDESCRIPTOR pfd;
	int format;
	
	// get the device context (DC)
	*hDC = GetDC( hWnd );
	
	// set the pixel format for the DC
	ZeroMemory( &pfd, sizeof( pfd ) );
	pfd.nSize = sizeof( pfd );
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 24;
	pfd.cDepthBits = 16;
	pfd.iLayerType = PFD_MAIN_PLANE;
	format = ChoosePixelFormat( *hDC, &pfd );
	SetPixelFormat( *hDC, format, &pfd );
	
	// create and enable the render context (RC)
	*hRC = wglCreateContext( *hDC );
	wglMakeCurrent( *hDC, *hRC );
	
}

// Disable OpenGL

void DisableOpenGL(HWND hWnd, HDC hDC, HGLRC hRC)
{
	wglMakeCurrent( NULL, NULL );
	wglDeleteContext( hRC );
	ReleaseDC( hWnd, hDC );
}
