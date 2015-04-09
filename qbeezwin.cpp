#include <windows.h>
#include <winuser.h>
#include <stdio.h>
#include <conio.h>
#include "QBeezPlayer.h"

HWND hwndQBeezFrame;

HWND FindQbeezWindow() {
	HWND hwndParent = GetDesktopWindow(), hw;
	
	for( hw = ::GetWindow(hwndParent,GW_CHILD); hw != NULL; hw = ::GetWindow(hw,GW_HWNDNEXT) ) {
		char szWndName[256], szClassName[256];
		GetWindowText( hw, szWndName, sizeof(szWndName) );
		GetClassName( hw, szClassName, sizeof(szClassName) );
		if( !strcmp( szClassName, "IEFrame" ) && !strcmp( szWndName, "http://www.shockwave.com - QBz - Microsoft Internet Explorer" ) ) {
			printf( "Found QBeez IEFrame: HWND = %08X", hw );
			hwndQBeezFrame = hw;
			hwndParent = hw;
			for( hw = ::GetWindow(hwndParent,GW_CHILD); hw != NULL; hw = ::GetWindow(hw,GW_HWNDNEXT) ) {
				GetClassName( hw, szClassName, sizeof(szClassName) );
				printf( "  IEFrame child window: %08X %s\n", hw, szClassName );

				if( !strcmp( szClassName, "Shell DocObject View" ) ) {
					printf( "  Found DocObjectView window: HWND = %08X\n", hw );
					hwndParent = ::GetWindow(hw,GW_CHILD);
					GetClassName( hwndParent, szClassName, sizeof(szClassName) );
					printf( "    SHELL DOV child window: %08X %s\n", hwndParent, szClassName );
					
					for( hw = ::GetWindow(hwndParent,GW_CHILD); hw != NULL; hw = ::GetWindow(hw,GW_HWNDNEXT) ) {
						GetClassName( hw, szClassName, sizeof(szClassName) );
						printf( "       IE Server child window: %08X %s\n", hw, szClassName );

						if( !strncmp( szClassName, "Shockwave", 9 ) ) {
							printf( "       Found Shockwave window: HWND = %08X\n", hw );

							for( hwndParent = hw, hw = ::GetWindow(hwndParent,GW_CHILD); hw != NULL; hwndParent = hw, hw = ::GetWindow(hwndParent,GW_CHILD) )
								;
							
							printf( "           QBEEZ ImlWinCls Window : HWND = %08X\n", hwndParent );
							return hwndParent;
						}
						
					}
					break;
				}
			}
			break;
		}
	}

	return 0;
}


#define BOARD_MIN_X (264-15)
#define BOARD_MIN_Y (139-123)

#define LEFT_ROTATION_X 334
#define LEFT_ROTATION_Y 428

#define RIGHT_ROTATION_X 399
#define RIGHT_ROTATION_Y 428


SIZE qbeeSizes[] = {
	{ 76, 76 },
	{ 63, 63 },
	{ 54, 54 },
	{ 46, 46 },
	{ 42, 42 },
	{ 38, 38 },
	{ 35, 35 },
	{ 32, 32 },
	{ 29, 29 },
	{ 27, 27 }
};

class ColorTable {
private:
	struct Entry {
		COLORREF color;
		int		 count;
	} ;

	Entry*		entries;
	
	int			size;
	int 		count;

public:
	ColorTable( int s = 64 ) : size(s), count(0) {
		entries = new Entry[size];
		memset( entries, 0, size*sizeof(Entry) );
	}

	~ColorTable() {
		delete[] entries;
	}

	void add( COLORREF cr ) {
		if( cr == 0 )
			return;
		for( int i = 0; i < count; i++ )
			if( entries[i].color == cr ) {
				entries[i].count++;
				return;
			}
		
		if( count == size ) {
			printf( "GROWING COLOR TABLE\n" );
			Entry * temp = new Entry[size*2];
			memcpy( temp, entries, size*sizeof(Entry) );
			memset( temp+size, 0, size*sizeof(Entry) );
			size *= 2;
			delete[] entries;
			entries = temp;
		}

		entries[count].color = cr;
		entries[count].count = 1;
		count++;
	}

	void print( int min ) {
		printf("ColorTable size is %d of %d\n", count, size );
		for( int i = 0; i < count; i++ ) {
			if( entries[i].count >= min )
				printf( "%02x/%02x/%02x : %d pixels\n",
						GetRValue(entries[i].color), GetGValue(entries[i].color), GetBValue(entries[i].color),
						entries[i].count );
		}
	}

	COLORREF dominantColor() {
		int max = 0;
		for( int i = 1; i < count; i++ ) {
			if( entries[i].count >= entries[max].count )
				max = i;
		}
		return entries[max].color;
	}
};
		
struct ColorMap {
	COLORREF cr;
	char	 c;
} colormap[] = {
	{ 0x000098ff, 'O' },
	{ 0x00009cff, 'O' },
	{ 0x00980098, 'P' },
	{ 0x009c009c, 'P' },
	{ 0x00ededed, 'W' },
	{ 0x00cbcbcb, 'W' },
	{ 0x00cecece, 'W' },
	{ 0x00ffcb32, 'C' },
	{ 0x00ffce31, 'C' },
	{ 0x0000ed00, 'G' },
	{ 0x0000ef00, 'G' },
	{ 0x00ed0000, 'B' },
	{ 0x00ef0000, 'B' },
	{ 0x00ff3200, 'B' },
	{ 0x0000ffff, 'Y' },
	{ 0x000000ff, 'R' },
	{ 0x00cb00ff, 'M' }
  
};

#define COLORS (sizeof(colormap)/sizeof(*colormap))

char mapColor( COLORREF cr ) {
	for( int i = 0; i < COLORS; i++ ) {
		if( colormap[i].cr == cr )
			return colormap[i].c;
	}
	return ' ';
}


int Screen::load( int level, HWND hwnd ) {
	int allMatched = 1;
	HDC hdc = GetDC( hwnd );
	for( int r = 0; r < level+5; r++ ) {
		for( int c = 0; c < level+5; c++ ) {
			//printf( "**Examining location at %d,%d\n", r,c );
			ColorTable ct;
			for( int x = BOARD_MIN_X + c*qbeeSizes[level].cx; x < BOARD_MIN_X + (c+1) * qbeeSizes[level].cx; x += 4 ) {
				for( int y = BOARD_MIN_Y + r*qbeeSizes[level].cy; y < BOARD_MIN_Y + (r+1) * qbeeSizes[level].cy; y += 4 ) {
					COLORREF cr = GetPixel( hdc, x, y );
					if( cr != 0 )
						ct.add(cr);
				}
			}
			// ct.print(200);
			COLORREF cr = ct.dominantColor();
			char color = mapColor(cr);
			/*
			printf( "Dominant color at %d,%d is %08X (%02x/%02x/%02x).  Maps to '%c'\n", r, c,
					cr, GetRValue(cr), GetGValue(cr), GetBValue(cr), color );
			*/
			if( color == ' ' )
				allMatched = 0;
			set(r,c,color);
		}
	}
	
	ReleaseDC(hwnd,hdc);

	/*
	if( allMatched == 0 ) {
		printf( "Not able to match all QBEEZ!\nPress any key to exit.\n" );
		_getche();
		return 1;
	}
	*/
	
	return 0;
}


QBeezPlayer::QBeezPlayer( int l, HWND h ) : level(l), score( 0 ), hwnd(h) {
	screen = new Screen( level+5, level+5 );
	if( screen->load( level, hwnd ) != 0 ) {
		fprintf( stderr, "qbeezplay: unable to read screen\n" );
		exit(-1);
	}
	
	screen->computeGroups();
}

long screenWidth, screenHeight;

void MoveMouse( HWND hwnd, int x, int y ) {
	INPUT in;

	POINT pt ;
	pt.x = x;
	pt.y = y;
	ClientToScreen( hwnd, &pt );

	in.type = INPUT_MOUSE;
	in.mi.dx = (int) (((double)pt.x * 65535.0) / (double)screenWidth);
	in.mi.dy = (int) (((double)pt.y * 65535.0) / (double)screenHeight);
	in.mi.mouseData = 0;
	in.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;
	in.mi.time = GetTickCount();
	in.mi.dwExtraInfo = 0;

	printf( "moving mouse to %d,%d\n", in.mi.dx, in.mi.dy );
	SendInput( 1, &in, sizeof(INPUT) );
}


void ClickMouse() {
	INPUT in;
	
	in.type = INPUT_MOUSE;
	in.mi.dx = 0;
	in.mi.dy = 0;
	in.mi.mouseData = 0;
	in.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
	in.mi.time = GetTickCount();
	in.mi.dwExtraInfo = 0;

	Sleep(50);
	
	SendInput( 1, &in, sizeof(INPUT) );

	Sleep(50);
	
	in.mi.time = GetTickCount();
	in.mi.dwFlags = MOUSEEVENTF_LEFTUP;

	SendInput( 1, &in, sizeof(INPUT) );
}

int moves;
int nextDelay;
struct timeval tvLastMove;

void QBeezPlayer::executeMove( Move* move ) {
	int x, y ;

	if( moves++ > 0 ) {
		timeval tvNow, tvDiff;
		gettimeofday( &tvNow, 0 );
		timediff( tvDiff, tvNow, tvLastMove );
		long msecDiff = tvDiff.tv_sec*1000 + tvDiff.tv_usec/1000;
		printf( "Settling board. %d msec elapsed since last move.\n", msecDiff );
		if( msecDiff < nextDelay ) {
			printf( "Sleeping for %d msec...", nextDelay-msecDiff );
			Sleep(nextDelay-msecDiff);
		} else {
			printf( "No sleep necessary..." );
		}
		fflush(stdout);
		
		printf( "OK\n" );
	}
	
	if( move->getDirection() == LEFT ) {
		printf( "EXECUTING: LEFT rotation\n" );
		x = LEFT_ROTATION_X;
		y = LEFT_ROTATION_Y;
		nextDelay = 2000;
	} else if( move->getDirection() == RIGHT ) {
		printf( "EXECUTING: RIGHT rotation\n" );
		x = RIGHT_ROTATION_X;
		y = RIGHT_ROTATION_Y;
		nextDelay = 2000;
	} else {
		printf( "EXECUTING: select QBEE %d,%d\n", move->getRow(), move->getCol() );
		x = BOARD_MIN_X + (qbeeSizes[level].cx * move->getCol()) + (qbeeSizes[level].cx / 2);
		y = BOARD_MIN_Y + (qbeeSizes[level].cy * move->getRow()) + (qbeeSizes[level].cy / 2);

		unsigned int size = screen->getGroupSize( move->getRow(), move->getCol() );
		if( size < 3 ) {
			nextDelay = 1000;
		} else if( size < 5 ) {
			nextDelay = 1500;
		} else {
			nextDelay = 2000;
		}
	}

	if( screen->getLeft() > 100 )
		nextDelay = (5*nextDelay)/4;
	else if( screen->getLeft() > 50 )
		nextDelay = (6*nextDelay)/5;
	
	printf( "click at %d,%d\n", x, y );

	MoveMouse( hwnd, x, y );
	ClickMouse();

	gettimeofday( &tvLastMove, 0 );

}


void execute_local_player( HWND hwndQBeez, char* alg ) {
	for(int level = 0; level < 10; level++ ) {
		printf( "Expecting to play LEVEL %d\n", level + 1 );
		printf( "Press Q to exit, R to restart level, level number, or any other key to play\n" );
		char ch = toupper(_getch());
		if( ch == 'Q' )
			break;
		if( ch == 'R' ) {
			level = -1;
			continue;
		}
		if( '0' == ch )
			level = 9;
		if( '1' <= ch && ch <= '9' )
			level = ch - '1';

		QBeezPlayer* player = new QBeezPlayer(level,hwndQBeez);

		printf("INITIAL SCREEN:\n");
		player->getScreen()->print(stdout);

		Strategy* strategy = 0;

		if( !strcmp( alg, "-greedy" ) )
			strategy = new GreedyStrategy();
		if( !strcmp( alg, "-search" ) )
			strategy = new ExhaustiveSearchStrategy();
		if( !strcmp( alg, "-limited" ) )
			strategy = new LimitedSearchStrategy();
		if( !strcmp( alg, "-heuristic" ) )
			strategy = new HeuristicSearchStrategy();

		if( ! strategy ) {
			fprintf( stderr, "Invalid strategy specified\n" );
			exit(3);
		}

		moves = 0;
		player->play(strategy);
	
		delete player;
	}
}


void execute_remote_player( HWND hwndQBeez, char* server ) {

	// initialize the WinSock library if we are in a Win32 environment
	WORD wVersionRequested;
	WSADATA wsaData;
	int err;
 
	wVersionRequested = MAKEWORD( 2, 2 );
 
	err = WSAStartup( wVersionRequested, &wsaData );
	if ( err != 0 ) {
		printf("Winsock initialization failed.\n" );
		return;
	}
 
	/* Confirm that the WinSock DLL supports 2.2.*/
	/* Note that if the DLL supports versions greater    */
	/* than 2.2 in addition to 2.2, it will still return */
	/* 2.2 in wVersion since that is the version we      */
	/* requested.                                        */

	if ( LOBYTE( wsaData.wVersion ) != 2 ||
		 HIBYTE( wsaData.wVersion ) != 2 ) {
		/* Tell the user that we could not find a usable */
		/* WinSock DLL.                                  */
		printf( "Winsock version mismatch.\n" );
		WSACleanup( );
		return; 
	}

	struct hostent *h;
	
	if( ! (h = gethostbyname( server )) ) {
		printf( "Cannot get address of %s\n", server );
		exit(-3);
	}
	
	for(int level = 0; level < 10; level++ ) {
		printf( "Expecting to play LEVEL %d\n", level + 1 );
		printf( "Press Q to exit, R to restart level, level number, or any other key to play\n" );
		char ch = toupper(_getch());
		if( ch == 'Q' )
			break;
		if( ch == 'R' ) {
			level = -1;
			continue;
		}
		if( '0' == ch )
			level = 9;
		if( '1' <= ch && ch <= '9' )
			level = ch - '1';

		QBeezPlayer* player = new QBeezPlayer(level,hwndQBeez);

		printf("INITIAL SCREEN:\n");
		player->getScreen()->print(stdout);

		moves = 0;
		nextDelay = 0;

		SOCKET s = socket( AF_INET, SOCK_STREAM, 0 );
		if( s == -1 ) {
			printf( "Error creating socket\n" );
			exit(-2);
		}
		
		struct sockaddr_in sin;
		sin.sin_family = AF_INET;
		sin.sin_port = htons(15331);
		sin.sin_addr.s_addr = ((struct in_addr*)h->h_addr_list[0])->s_addr;
		
		if( connect( s, (struct sockaddr*) &sin , sizeof(struct sockaddr_in) ) != 0 ) {
			printf( "Cannot connect to %s\n", server );
			exit( -4 );
		}
		
		player->play( s );
		
		delete player;

		closesocket(s);
	}

	WSACleanup( );

}



int main( int argc, char** argv ) {
	if( argc != 2 ) {
		fprintf( stderr, "usage: qbeezplay -ALG | ipaddr\n" );
		return 1;
	}
	
	HWND hwndQBeez = FindQbeezWindow();
	if( ! hwndQBeez ) {
		fprintf( stderr, "Could not find QBEEZ Window\n" );
		return -1;
	}

	screenHeight = (long) GetSystemMetrics( SM_CYSCREEN );
	screenWidth  = (long) GetSystemMetrics( SM_CXSCREEN );

	printf( "Screen is %dx%d\n", screenWidth, screenHeight );
	
	if( *argv[1] == '-' )
		execute_local_player( hwndQBeez, argv[1] );
	else
		execute_remote_player( hwndQBeez, argv[1] );

	return 0;
}
