#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#else
#endif

#include <SDL2/SDL.h>
#include <SDL2/SDL_keyboard.h>
#include <SDL2/SDL_stdinc.h>
#include <SDL2/SDL_scancode.h>
#include <stdlib.h>
#include <math.h>

#define SW			320
#define SH			240
#define kMultiplySW( y ) ( ( (y) << 6 ) + ( (y) << 8 ) )

#define MW			16
#define MH			16

#define PI			3.1415926535897932384626433832795f
#define TWOPI		6.283185307179586476925286766559f

#define FOV			( PI / 3.0f )

#define TW			256

#define CW			256
#define CH			640

#define kSpeedFriction	0.99f
#define kAngleFriction	0.9f

#define rgb(r,g,b)	( ( (r) << 16 ) | ( (g) << 8 ) | (b) )
#define r( color )	( ( (color) >> 16 ) & 255 )
#define g( color )	( ( (color) >> 8 ) & 255 )
#define b( color )	( (color) & 255 )
float	fPlayerAngle	= 0.0f;
float	fPlayerDeltaAngle = 0.0f;
float	fPlayerX		= 0.5f;
float	fPlayerY		= 0.5f;
float fPlayerSpeed	= 0.0f;
float fFishFactor[ SW ];

float fNewX, fNewY;
int ray,i,row;

int iMipMapTextureOffset[ 9 ];
int iMipMapLevelRow[ SH/2 ];

bool bHorzWalls[ MH+1 ][ MW ];
bool bVertWalls[ MH ][ MW+1 ];
bool bVisited[ MH ][ MW ];

SDL_Window *window;
SDL_Renderer *renderer;

SDL_Surface *screen;
SDL_Surface *texture;
SDL_Surface *clouds;

typedef Sint32 int32;

int32 *pixel   = NULL;
int32 *textel  = NULL;
int32 *cloudel = NULL;

int iPosDirs[24][4] = {
    { 0, 1, 2, 3 }, { 0, 1, 3, 2 }, { 0, 2, 1, 3 }, { 0, 2, 3, 1 }, { 0, 3, 1, 2 }, { 0, 3, 2, 1 },
    { 1, 0, 2, 3 }, { 1, 0, 3, 2 }, { 1, 2, 0, 3 }, { 1, 2, 3, 0 }, { 1, 3, 0, 2 }, { 1, 3, 2, 0 },
    { 2, 1, 0, 3 }, { 2, 1, 3, 0 }, { 2, 0, 1, 3 }, { 2, 0, 3, 1 }, { 2, 3, 1, 0 }, { 2, 3, 0, 1 },
    { 3, 1, 2, 0 }, { 3, 1, 0, 2 }, { 3, 2, 1, 0 }, { 3, 2, 0, 1 }, { 3, 0, 1, 2 }, { 3, 0, 2, 1 }
};

void vGenerateMaze( int y, int x )
{
	bVisited[y][x] = true;

	int i, sequence;

	// top of maze check
	if( y == 0 )
	{
		if( x == 0 )
		{
			i=2;
			sequence = rand() % 2;
		}
		else if( x == MW-1 )
		{
			i=2;
			sequence = rand() % 2 + 2;
		}
		else
		{
			i=1;
			sequence = rand() % 6;
		}
	}

	// bottom of maze check
	else if( y == MH-1 )
	{
		if( x == 0 )
		{
			i=2;
			sequence = rand() % 2 + 18;
		}
		else if( x == MW-1 )
		{
			i=2;
			sequence = rand() % 2 + 16;
		}
		else
		{
			i=1;
			sequence = rand() % 6 + 18;
		}
	}

	// left and right of maze check
	else if( x == 0 )
	{
		i=1;
		sequence = rand() % 6 + 6;
	}
	else if( x == MW-1 )
	{
		i=1;
		sequence = rand() % 6 + 12;
	}

	// if not an edge
	else
	{
		i=0;
		sequence = rand() % 24;
	}

	for( ; i<4; i++ )
	{	
		switch( iPosDirs[ sequence ][ i ] )
		{
		case 0:
			if( !bVisited[y-1][x] )
			{
				bHorzWalls[y][x] = false;
				vGenerateMaze( y-1, x );
			}
			break;
		case 1:
			if( !bVisited[y][x-1] )
			{
				bVertWalls[y][x] = false;
				vGenerateMaze( y, x-1 );
			}
			break;
		case 2:
			if( !bVisited[y][x+1] )
			{
				bVertWalls[y][x+1] = false;
				vGenerateMaze( y, x+1 );
			}
			break;
		case 3:
			if( !bVisited[y+1][x] )
			{
				bHorzWalls[y+1][x] = false;
				vGenerateMaze( y+1, x );
			}
		}
	}
}

void vDrawMaze()
{
	int x,y,i;
	for( y=0; y<MH+1; y++ )
	{
		for( x=0; x<MW; x++ )
		{
			if( bHorzWalls[y][x] )
			{
				for( i=0; i<=2; i++ )
					pixel[ x*2 + y*2*SW + i ] = 16777215;
			}
		}
	}
	for( y=0; y<MH; y++ )
	{
		for( x=0; x<MW+1; x++ )
		{
			if( bVertWalls[y][x] )
			{
				for( i=0; i<=2; i++ )
					pixel[ x*2 + y*2*SW + i*SW ] = 16777215;
			}
		}
	}
}


float x, y, dx, dy, fDistVert, fDistHorz, fDist, fHorzV, fVertV, v, fTanAngle, fLineHeight, du, u, fLineStart, fLineFinish, fLineIntensity;	
int32 top, bottom;
float bottomweight, topweight, angle;

void vRaycast()
{
	// ray trace
	angle = fPlayerAngle - FOV / 2.0f;
	if( angle < 0.0f )
		angle += TWOPI;

	for( ray=0; ray<SW; ray++ )
	{
		fTanAngle = (float) tan( angle );

		// first traverse vertical walls
		if( angle < .5f*PI || 1.5f*PI < angle )		// if ray going right
		{
			dx = 1.0f;
			dy = fTanAngle;
			x = float( int( fPlayerX ) + 1 );
			y = fPlayerY + fTanAngle * ( x - fPlayerX );
		}
		else		// if ray going left
		{
			dx = -1.0f;
			dy = -fTanAngle;
			x = float( int( fPlayerX ) );
			y = fPlayerY + fTanAngle * ( x - fPlayerX );
		}

		while( true )
		{
			// test if in range
			if( y < 0.0f || float(MH) <= y )
				break;

			// test if collide with wall
			if( bVertWalls[ int(y) ][ int(x) ] )
				break;

			// move ray
			x += dx;
			y += dy;
		}

		fVertV = y - float( int(y) );

		fDistVert = (float) sqrt( ( x - fPlayerX ) * ( x - fPlayerX ) + ( y - fPlayerY ) * ( y - fPlayerY ) );

		// then traverse horizontal walls
		if( 0.0f < angle && angle < PI ) // if ray going up
		{
			dx = 1.0f / fTanAngle;
			dy = 1.0f;
			y = float( int( fPlayerY + 1 ) );
			x = fPlayerX + 1.0f / fTanAngle * ( y - fPlayerY );
		}
		else		// if ray going down
		{
			dx = -1.0f / fTanAngle;
			dy = -1.0f;
			y = float( int( fPlayerY) );
			x = fPlayerX + 1.0f / fTanAngle * ( y - fPlayerY );
		}


		while( true )
		{
			// test if in range
			if( x < 0.0f || float(MW) <= x )
				break;

			// test if collide with wall
			if( bHorzWalls[ int(y) ][ int(x) ] )
				break;

			// move ray
			x += dx;
			y += dy;
		}

		fHorzV = x - float( int(x) );

		fDistHorz = (float) sqrt( ( x - fPlayerX ) * ( x - fPlayerX ) + ( y - fPlayerY ) * ( y - fPlayerY ) );

		// determine which is closer
		if( fDistHorz < fDistVert )
		{
			fDist = fDistHorz;
			v = fHorzV;
		}
		else
		{
			fDist = fDistVert;
			v = fVertV;
		}

		int iMipMapLevel, iMipMapOffset, iMipMapWidth;
		int iLineFinish, iLineStart, iLineHeight, iHalfLineHeight;

		iHalfLineHeight = int( float(SH/2) / fDist / fFishFactor[ ray ] );
		iLineHeight = iHalfLineHeight * 2;

		if( iHalfLineHeight < SH/2 )
			iMipMapLevel = iMipMapLevelRow[ iHalfLineHeight ];
		else
			iMipMapLevel = 8;

		iMipMapOffset= iMipMapTextureOffset[ iMipMapLevel ];
		iMipMapWidth = 1 << iMipMapLevel;
		du = float(iMipMapWidth) / float(iLineHeight);

		iLineStart = SH/2 - iHalfLineHeight;
		if( iLineStart < 0 )
		{
			u = -du * float(iLineStart);
			iLineStart = 0;
		}
		else
			u = 0.0f;

		iLineFinish = SH/2 + iHalfLineHeight;
		if( iLineFinish > SH )
			iLineFinish = SH;

		// draw clouds
		int32 *pixeloffset = &pixel[ ray ];
		int32 *cloudeloffset = &cloudel[ int( angle / TWOPI * float(CH) ) * CW ];
		int32 *pixelend = pixeloffset + kMultiplySW( iLineStart );
		float fCloudU = 0.0f;
		float fCloudDU = float(CW) / float(SH/2);
		for( ; pixeloffset < pixelend; pixeloffset += SW )
		{
			*pixeloffset = cloudeloffset[ int(fCloudU) ];
			fCloudU += fCloudDU;
		}

		// actually draw the line
		int32 *textureoffset = &textel[ int( v * float(iMipMapWidth) ) * TW + iMipMapOffset ];
		pixelend = &pixel[ ray + kMultiplySW( iLineFinish ) ];

		for( ; pixeloffset < pixelend; pixeloffset += SW )
		{
			*pixeloffset = *(textureoffset + int(u) ) ;
			u += du;
		}

		float fCosAngle = (float) cos( angle );
		float fSinAngle = (float) sin( angle );

		float fDeltaScale = -float(SH/2) / fFishFactor[ray];
		fDist = float(SH/2) / ( fFishFactor[ ray ] * float( iHalfLineHeight ) );

		for( row = iHalfLineHeight; row < SH/2; row++ )
		{
			fDist += fDeltaScale / float( row * (row+1) );
			v = fPlayerX + fCosAngle * fDist;
			u = fPlayerY + fSinAngle * fDist;
			iMipMapLevel = iMipMapLevelRow[ row ];
			iMipMapOffset= iMipMapTextureOffset[ iMipMapLevel ];
			iMipMapWidth = 1 << (iMipMapLevel);
			*pixeloffset = textel[ int( ( v - float( int( v ) ) ) * float(iMipMapWidth) ) * TW + int( ( u - float( int( u ) ) ) * float(iMipMapWidth) ) + iMipMapOffset ];
			pixeloffset += SW;
		}

		// increment the angle
		angle += FOV / float(SW);
		if( angle >= TWOPI )
			angle -= TWOPI;
	}
}
/*
bool bTestIfCanMove()
{
	if( fNewX < 0.0f || fNewY < 0.0f )
		return false;

	if( (int)fNewX > (int)fPlayerX )
	{
		if( (int)bVertWalls[ int(fPlayerY) ][ int(fPlayerX) + 1 ] )
			return false;
	}
	if( (int)fNewX < (int)fPlayerX )
	{
		if( bVertWalls[ int(fPlayerY) ][ int(fPlayerX) ] )
			return false;
	}
	if( (int)fNewY > (int)fPlayerY )
	{
		if(  bHorzWalls[ int(fPlayerY) + 1 ][ int(fPlayerX) ] )
			return false;
	}
	if( (int)fNewY < (int)fPlayerY )
	{
		if( bHorzWalls[ int(fPlayerY) ][ int(fPlayerX) ] )
			return false;
	}
	return true;
}
*/
int count = 0;
double lasttime = 0;
SDL_Rect screenRect {0, 0, SW, SH};

void copyScreenPixelsToWindow() {
  SDL_Texture *screenTexture = SDL_CreateTextureFromSurface(renderer, screen);
  SDL_RenderClear(renderer);  
  SDL_RenderCopy(renderer, screenTexture, NULL, NULL);
  SDL_RenderPresent(renderer);
  SDL_DestroyTexture(screenTexture);
}


#ifdef __EMSCRIPTEN__
EM_JS(int, canvas_get_width, (), {
  return canvas.width;
});

EM_JS(int, canvas_get_height, (), {
  return canvas.height;
});

EM_JS(int, body_get_width, (), {
  return document.getElementsByTagName("body")[0].clientWidth;
});

EM_JS(int, body_get_height, (), {
  return document.getElementsByTagName("body")[0].clientHeight;
});

EM_JS(int, window_get_width, (), {
  return window.innerWidth;
});

EM_JS(int, window_get_height, (), {
  return window.innerHeight;
});

EM_JS(int, document_is_fullscreen, (), {
	return document.fullscreen;
});

EM_BOOL emscripten_window_resized_callback(int eventType, const void *reserved, void *userData){

    double width, height;
    emscripten_get_element_css_size("canvas", &width, &height);
    printf("emscripten_window_resized_callback: %.0f x %.0f\n", width, height);

    // resize SDL window
    SDL_SetWindowSize(window, width, height);

    return true;
}

const char *emscripten_result_to_string(EMSCRIPTEN_RESULT result) {
  if (result == EMSCRIPTEN_RESULT_SUCCESS) return "EMSCRIPTEN_RESULT_SUCCESS";
  if (result == EMSCRIPTEN_RESULT_DEFERRED) return "EMSCRIPTEN_RESULT_DEFERRED";
  if (result == EMSCRIPTEN_RESULT_NOT_SUPPORTED) return "EMSCRIPTEN_RESULT_NOT_SUPPORTED";
  if (result == EMSCRIPTEN_RESULT_FAILED_NOT_DEFERRED) return "EMSCRIPTEN_RESULT_FAILED_NOT_DEFERRED";
  if (result == EMSCRIPTEN_RESULT_INVALID_TARGET) return "EMSCRIPTEN_RESULT_INVALID_TARGET";
  if (result == EMSCRIPTEN_RESULT_UNKNOWN_TARGET) return "EMSCRIPTEN_RESULT_UNKNOWN_TARGET";
  if (result == EMSCRIPTEN_RESULT_INVALID_PARAM) return "EMSCRIPTEN_RESULT_INVALID_PARAM";
  if (result == EMSCRIPTEN_RESULT_FAILED) return "EMSCRIPTEN_RESULT_FAILED";
  if (result == EMSCRIPTEN_RESULT_NO_DATA) return "EMSCRIPTEN_RESULT_NO_DATA";
  return "Unknown EMSCRIPTEN_RESULT!";
}

#define CHECK_EMSCRIPTEN_RESULT(func) {EMSCRIPTEN_RESULT ret = func; if (ret != EMSCRIPTEN_RESULT_SUCCESS) printf("%s returned %s.\n", #func, emscripten_result_to_string(ret));}

void setNewCanvasSize(int newWidth, int newHeight)
{
  printf( "setNewCanvasSize %d x %d\n", newWidth, newHeight);

  CHECK_EMSCRIPTEN_RESULT(emscripten_set_canvas_element_size("canvas", newWidth, newHeight));

  SDL_SetWindowSize(window, newWidth, newHeight);
}

EM_BOOL captureResizeEvent(int eventType, const EmscriptenUiEvent *e, void *rawState)
{

  if(document_is_fullscreen()) {
          printf("document_is_fullscreen()\n");
          EmscriptenFullscreenStrategy emFullscreenStrategy;
          emFullscreenStrategy.scaleMode = EMSCRIPTEN_FULLSCREEN_CANVAS_SCALE_STDDEF;
          emFullscreenStrategy.filteringMode = EMSCRIPTEN_FULLSCREEN_FILTERING_NEAREST;
          emFullscreenStrategy.canvasResizedCallback = emscripten_window_resized_callback;
          emFullscreenStrategy.canvasResizedCallbackUserData = 0;
          emscripten_enter_soft_fullscreen("canvas", &emFullscreenStrategy);
          setNewCanvasSize(SW, SH);
          return 0;
      }

  int newWidth = body_get_width();//(int) e->documentBodyClientWidth;
  int newHeight = (int) e->windowInnerHeight;
  printf( "captureResizeEvent %d x %d\n", newWidth, newHeight);
  setNewCanvasSize(newWidth, newHeight - 300);

  return 0;
}
#endif

void mainloop(void *arg)
{
	bool *receivedquit = (bool*) arg;
	SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT:
			printf("\nSDL_QUIT signal received.\n\n");
#ifdef __EMSCRIPTEN__
            emscripten_cancel_main_loop();
#endif
			*receivedquit = true;
            return;

            // get input{
        case SDL_KEYDOWN:
                //Select surfaces based on key press
                switch( event.key.keysym.sym )
                {


                    case SDLK_LEFT:
                    fPlayerDeltaAngle -= 0.025f;
                    break;

                    case SDLK_RIGHT:
                    fPlayerDeltaAngle += 0.025f;
                    break;

                    case SDLK_UP:
                    fPlayerSpeed += 0.01f;
                    break;

                    case SDLK_DOWN:
                    fPlayerSpeed -= 0.01f;
                    break;

                    default:
                    break;
                }
            break;
        }
    }

    fPlayerDeltaAngle *= kAngleFriction;

    fPlayerAngle += fPlayerDeltaAngle;

    if( fPlayerAngle < 0.0f )
        fPlayerAngle += TWOPI;

    else if( fPlayerAngle >= TWOPI )
        fPlayerAngle -= TWOPI;

    fPlayerSpeed *= kSpeedFriction;
    fNewY = fPlayerY + (float) sin( fPlayerAngle ) * fPlayerSpeed;
    fNewX = fPlayerX + (float) cos( fPlayerAngle ) * fPlayerSpeed;

    /*if( bTestIfCanMove() )
    {
        fPlayerY = fNewY;
        fPlayerX = fNewX;
    }
    else*/
        fPlayerSpeed = 0.0f;


    SDL_LockSurface(screen);
    pixel = (int32*) screen->pixels;
    vRaycast();
    SDL_UnlockSurface(screen);

    copyScreenPixelsToWindow();

	double time = SDL_GetTicks();
 // printf("time %.0f, it %d, diff %.2f, fps=%.2f\n", time, count, time-lasttime, 1000.0/(time-lasttime));

  lasttime=time;
	count++;
}


int main(int argc, char* argv[])
{
    SDL_Init(SDL_INIT_VIDEO);
    Uint32 starttime = lasttime = SDL_GetTicks();

	window = SDL_CreateWindow("Eric Fontaine's Raycasting Maze", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SW, SH, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI);
	if (window == NULL) {
		fprintf(stderr, "Error: %s\n", SDL_GetError());
		return -1;
	}
SDL_SetWindowSize(window, SW*2, SH*2);

	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (renderer == NULL) {
		fprintf(stderr, "Error: %s\n", SDL_GetError());
		return -1;
	}
 
    // screen is the SW*SH surface where pixel editing is done, which gets resized to window or canvas
    screen = SDL_CreateRGBSurfaceWithFormat(0, SW, SH, 24, SDL_PIXELFORMAT_RGBA8888);

    /////////////////////
    // load images
    /////////////////////

    SDL_PixelFormat *format = SDL_AllocFormat(SDL_PIXELFORMAT_RGBA8888);
    SDL_Surface *textureBMP, *cloudsBMP; // first read BMP, then convert to SDL_PIXELFORMAT_RGBA8888
    if(!(textureBMP = SDL_LoadBMP("assets/images/wallmipmap.bmp"))) SDL_Log("SDL_LoadBMP texture failed: %s\n", SDL_GetError());
    if(!(cloudsBMP = SDL_LoadBMP("assets/images/clouds.bmp")))      SDL_Log("SDL_LoadBMP clouds failed: %s\n", SDL_GetError());
    if(!(texture = SDL_ConvertSurface(textureBMP, format, 0)))      SDL_Log("SDL_ConvertSurface texture failed: %s\n", SDL_GetError());
    if(!(clouds = SDL_ConvertSurface(cloudsBMP, format, 0)))        SDL_Log("SDL_ConvertSurface clouds failed: %s\n", SDL_GetError());
    SDL_FreeSurface(textureBMP);
    SDL_FreeSurface(cloudsBMP);
    SDL_FreeFormat(format);

    SDL_LockSurface(texture);
    SDL_LockSurface(clouds);
    textel = (int32 *) texture->pixels;
    cloudel = (int32 *) clouds->pixels;

    /////////////////////
    // generate maze data
    /////////////////////

    memset( bHorzWalls, true, sizeof( bHorzWalls ) );
    memset( bVertWalls, true, sizeof( bVertWalls ) );
    memset( bVisited, false, sizeof( bVisited ) );

    srand( starttime );

    vGenerateMaze( 0, 0 );

    for( ray = 0; ray < SW; ray++ )
        fFishFactor[ ray ] = (float) cos( ( float(ray) / float(SW) * FOV ) - ( FOV / 2.0f ) );

    for( row = 1; row < SH/2; row ++ )
    {
        int iMipTextureRow = 0;
        int iMipTextureWidth = TW;
        float dv = float(TW) / float(row*2);

        i = 0;
        while( dv > 1.0f )
        {
            dv /= 2.0f;
            i++;
        }

        iMipMapLevelRow[ row ] = 8-i;
    }
    iMipMapLevelRow[0] = 0;

    int iMipMapRow = 0;
    int iMipMapTextureWidth = TW;
    for( i=0; i<9; i++ )
    {
        iMipMapTextureOffset[ 8-i ] = iMipMapRow * TW;
        iMipMapRow += iMipMapTextureWidth;
        iMipMapTextureWidth /= 2;
    }

    /////////////////////
    // main loop
    /////////////////////

#ifdef __EMSCRIPTEN__
    int newWidth = body_get_width();
    int newHeight = window_get_height() - 300;
    printf("setting initial canvas dimensions to %d x %d\n", newWidth, newHeight);
    setNewCanvasSize(newWidth, newHeight);
    CHECK_EMSCRIPTEN_RESULT(emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, 0, true, captureResizeEvent));
    emscripten_set_main_loop_arg(mainloop, 0,-1, 1);
#else
  while (1) {
	  bool receivedquit = false;
	  mainloop((void *)(&receivedquit));
	  if (receivedquit)
	  	break;
    // Delay to keep frame rate constant (using SDL).
    SDL_Delay(16); // delay in milliseconds
  }
#endif

    SDL_Log("Quitting\n");
    SDL_UnlockSurface(clouds);
    SDL_UnlockSurface(texture);
    SDL_FreeSurface(clouds);
    SDL_FreeSurface(texture);
    SDL_FreeSurface(screen);
    SDL_DestroyWindow(window);
    SDL_Quit();
    SDL_Log("Bye!\n");
    return 0;
}
