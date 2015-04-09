
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#ifdef WIN32
#include <io.h>
#include <winsock.h>
#else
#include <sys/time.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif
#include <assert.h>

#include "QBeezPlayer.h"

#ifdef WIN32
typedef int socklen_t;
#else
#define SOCKET int
#define INVALID_SOCKET -1
#define closesocket close
#define _strnicmp strncasecmp
#endif

#define MODE_READ 0
#define MODE_WRITE 1

void QBeezPlayer::executeMove( Move* move ) {
}

//
// All socket classes derive from the ActiveSocket, and should
// override the process_read and process_write methods to handle
// read/write IO on the socket
//
class ActiveSocket
{
public:
	SOCKET s;
	ActiveSocket* pNextTmp;
	ActiveSocket* pNext;
	ActiveSocket** ppPrev;
	int mode;
	
	ActiveSocket( SOCKET _s ) :
		s(_s),
		pNext(0),
		pNextTmp(0),
		ppPrev(0),
		mode(MODE_READ)
		{
		}

	virtual ~ActiveSocket();

	virtual void process_read() {}
	virtual void process_write() {}

	void close();
};


//
// ActiveSocketList handles multiplexing of IO on several
// sockets.  select() is called on all the sockets in the
// list.  for each one that is in the read-able state,
// its process_read method is called.  for each socket in
// the writable state, its process_write method is called
//
// sometimes, a socket's state changes such the main linked
// list must be modified.  for example, a PortSocket, accepts
// a connection in its process_read, and that socket must
// be added to the linked list.  a DataSocket may have completed
// its request, and must be closed and removed from the main
// list.  in this case, the new sockets are added to the "temp"
// list, and after the main list is iterated, sockets in the
// temp list are handled.
//
class ActiveSocketList
{
private:
	// linked list of active sockets
	ActiveSocket* pHead;
	ActiveSocket** pTail;

	// temp list of sockets that are created 
	ActiveSocket* pListTemp;

public:

	ActiveSocketList() 	{
		pHead = 0;
		pTail = &pHead;
		pListTemp = 0;
	}

	inline void append( ActiveSocket* p );
	void select(void);
	void process_temp_list( void );
};

ActiveSocketList list;


ActiveSocket::~ActiveSocket()
{
}

void ActiveSocket::close()
{
	::closesocket(s);
	s = INVALID_SOCKET;
	list.append(this);
}


//
// Port socket is a socket which is bound to an address and port,
// and used in a listen() call to accept new connections.
//
class PortSocket : public ActiveSocket
{
public:

	PortSocket( SOCKET _s ) :
		ActiveSocket(_s)
		{
		}

	void process_read() ;
};


//
// DataSocket is a connected socket to an HTTP client.
//

class DataSocket : public ActiveSocket
{
private:
	char szRequest[1024];
	int  cbRequest;
	bool fEndOfRequest;

	char szData[100];
	int  cbData;
	int  cbDataSent;

	QBeezPlayer* player;
	Strategy* strategy;
	
public:
	DataSocket( SOCKET _s ) :
		ActiveSocket( _s ),
		cbRequest(0),
		fEndOfRequest(false),
		cbData(0)
		{
			player = 0;
			strategy = 0;
		}

	int bad_request();
	
	int process_data_request();
	void process_new_request_bytes( int n );
	void process_read();
	void process_write();
};


inline void ActiveSocketList::append( ActiveSocket* p )
{
	p->pNextTmp = pListTemp;
	pListTemp = p;
}


//
// process sockets put in the temp list.  if the socket is
// closed, it must be removed from the main list.  if the
// socket is open, it is a new data connection that must
// be added to the main list.
//
void ActiveSocketList::process_temp_list( void )
{
	ActiveSocket* p;
	ActiveSocket* pNext;

	for( p = pListTemp; p != 0; p = pNext ) {

		pNext = p->pNextTmp;
		p->pNextTmp = 0;

		if( p->s == INVALID_SOCKET ) {
			// removing from main list
			*(p->ppPrev) = p->pNext;
			if( p->pNext )
				p->pNext->ppPrev = p->ppPrev; 
			if( pTail == &p->pNext )
				pTail = p->ppPrev;
			delete p;
		} else {
			// adding to main list
			p->ppPrev = pTail;
			*pTail = p;
			pTail = &p->pNext;
			p->pNext = 0;
		}
	}

	pListTemp = 0;
}


//
// select across all of the sockets.  process the ones that
// are readable or writable.
//
void ActiveSocketList::select()
{
#ifdef WIN32    
	FD_SET fdsRead;
	FD_SET fdsWrite;
#else
	fd_set fdsRead;
	fd_set fdsWrite;
#endif
	ActiveSocket* p;
	int n = 0;
	struct timeval tv;
	
	FD_ZERO( &fdsRead );
	FD_ZERO( &fdsWrite );
	
	for( p = pHead; p != 0; p = p->pNext ) {
		if( p->mode == MODE_READ )
			FD_SET( p->s, &fdsRead );
		else 
			FD_SET( p->s, &fdsWrite );
#ifdef	WIN32
		n++;
#else	
		if( p->s > n )
			n = p->s;
#endif
	}

	if( n > 0 ) {
		n = ::select( n+1, &fdsRead, &fdsWrite, NULL, NULL );
	
		for( p = pHead; p != 0; p = p->pNext ) {
			if( FD_ISSET( p->s, &fdsRead ) ) {
				p->process_read();
			}
			if( p->s != -1 && FD_ISSET( p->s, &fdsWrite ) ) {
				p->process_write();
			}
		}
	}

    // there may be chnages to the main list, so handle these
	// now.
	process_temp_list();
}

void accept_requests()
{
	SOCKET sPort = INVALID_SOCKET;
	struct sockaddr_in sin;

	try {
		sPort = socket( AF_INET, SOCK_STREAM, 0 );
		if( sPort == INVALID_SOCKET ) {
			throw "create socket failed" ;
		}

		sin.sin_family = AF_INET;
		sin.sin_port = htons(15331);
#ifdef WIN32		
		sin.sin_addr.S_un.S_addr = INADDR_ANY;
#else
		sin.sin_addr.s_addr = INADDR_ANY;
#endif
		
		if( bind( sPort, (const sockaddr *)&sin, sizeof(sin) ) != 0 ) {
			throw "bind failed";
		}

		if( listen( sPort, 5 ) != 0 ) {
			throw "listen failed" ;
		}

		PortSocket* ps = new PortSocket(sPort);
		list.append( ps );
		sPort = INVALID_SOCKET;

		for(;;) {
			list.select();
		}
		
	} catch( const char* pstrError ) {
		printf( "error: %s\n", pstrError );
	}

	if( sPort != INVALID_SOCKET )
		closesocket( sPort );
}


int DataSocket::bad_request() {
	strcpy( szData, "ERR\n" );
	cbData  = 4;

	mode = MODE_WRITE;

	return 1;
}

//
// process the clients request
//
int DataSocket::process_data_request()
{
	int r,c;

	if( sscanf( szRequest, "%d,%d\n", &r, &c ) != 2 ) {
		return bad_request();
	}

	char *p;
	for( p = szRequest; *p && *p != '\n'; p++ )
		;

	if( !*p ) {
		return bad_request();
	}
	
	p++;

	int level = r - 5;
	
	strategy = new HeuristicSearchStrategy();
	player = new QBeezPlayer( r,c );
	if( player->load(p) != 0 ) 
	   return bad_request();

	printf("INITIAL SCREEN:\n");
	player->getScreen()->print(stdout);
	
	// processing the request is done.  Now we want to write.
	mode = MODE_WRITE;
	cbDataSent = 0;

	return 0;
}


//
// read some data from the HTTP client
//
void DataSocket::process_read( void )
{
	int n;
	
	if( cbRequest == sizeof(szRequest) ) {
		// the client has filled out input buffer.  In order to not have
		// a buffer overrun, we deny the clients request.
		close();
	} else {

		// read data into the request buffer
		n = recv( s, szRequest + cbRequest, sizeof(szRequest) - cbRequest, 0 );

		if( n <= 0 ) {
			// some kind of error happend here.  close the connection
			close();
		} else {
			// process the new data.
			process_new_request_bytes(n);
		}
	}
}


//
// process the input data.  we are looking for two '\n' characters in
// a row marking the end of the request.  this scans the newly
// read data, beginning at szRequest[cbRequest], for the newline characters
// if we find it, we have a complete request, and process_data_request is
// invoked to come up with a response.
// 
void DataSocket::process_new_request_bytes( int n )
{
	int cNL = 0;  

	if( cbRequest > 0 ) {
		// if the previous data
		if( szRequest[cbRequest-1] == '\n' )
			cNL++;
	}
	
	for( int i = 0; i < n; i++ ) {
		if( szRequest[ cbRequest + i ] == '\n' ) {
			// have we seen 2 consecutive newlines?
			if( ++cNL == 2 ) {
				// found CR-CR pair marking end of request
				fEndOfRequest = true;
				cbRequest += i;
				szRequest[cbRequest] = 0;
				process_data_request();
				return;
			}
		} else if( szRequest[ cbRequest+i] != '\r' ) {
			// anything other than a '\r' char resets our newline-count
			// back to 0
			cNL = 0;
		}
	}

	cbRequest += n;
}


//
// write some data to the DataSocket
//
void DataSocket::process_write()
{
	int n;
	const char *p;
	int cb;
	Move* m;

	// decide if we need to generate more data to send
	if( cbDataSent == cbData ) {
		if( ! (m = player->getNextMove( strategy ) ) ) {
			close();
			return;
		} else {
			if( m->getRow() != INVALID )
				sprintf( szData, "M:%d,%d\n", m->getRow(), m->getCol() );
			else
				sprintf( szData, "R:%c\n", m->getDirection() == LEFT ? 'L' : 'R' );
			p = szData;
			cbData = strlen(szData);
			cb = strlen(szData);
			cbDataSent = 0;
			delete m;
		}
		
	} else {
		p = szData + cbDataSent;
		cb = cbData - cbDataSent;
	}

	printf( "Sending %d bytes\n", cb );
	n = send( s, p, cb, 0 );
	printf( "Sent %d bytes successfully\n", n );
	if( n <= 0 ) {
		close();
	} else {
		cbDataSent += n;
	}
}


//
// PortSocket is read-able in select if there is a pending
// connection to accept. accept the connection and create a
// new DataSocket for it
//
void PortSocket::process_read( void )
{
	SOCKET sNew;
	sockaddr_in sin;
	socklen_t addrlen = sizeof(sin);
	
	sNew = accept( s, (sockaddr*)&sin, &addrlen );
	if( sNew != INVALID_SOCKET ) {
		DataSocket* pDataSock = new DataSocket(sNew);
		if( ! pDataSock )
			throw "out of memory";
		list.append( pDataSock );
	}
}


int main( void )
{
#ifdef WIN32
	// initialize the WinSock library if we are in a Win32 environment
	WORD wVersionRequested;
	WSADATA wsaData;
	int err;
 
	wVersionRequested = MAKEWORD( 2, 2 );
 
	err = WSAStartup( wVersionRequested, &wsaData );
	if ( err != 0 ) {
		printf("Winsock initialization failed.\n" );
		return -1;
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
		return -1; 
	}
#endif
    
	accept_requests();

#ifdef WIN32
	WSACleanup( );
#endif
	return 0;
}


