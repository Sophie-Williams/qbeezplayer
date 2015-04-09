#include <stdio.h>
#include <string.h>
#ifdef WIN32
#include <windows.h>
#else
#include <sys/time.h>
#include <unistd.h>
#endif
#include <assert.h>

#include "QBeezPlayer.h"

void QBeezPlayer::executeMove( Move* move ) {
}

int main( int argc, char** argv ) {
	if( argc != 3 ) {
		fprintf( stderr, "usage: qbeezplay ALG FILE\n" );
		return 1;
	}
	
	FILE* fp = fopen( argv[2], "r" );
	if( !fp ) {
		fprintf( stderr, "qbeezplay: cannot open %s\n", argv[1] );
		return 2;
	}

	QBeezPlayer* player = new QBeezPlayer(fp);

	printf("INITIAL SCREEN:\n");
	player->getScreen()->print(stdout);

	Strategy* strategy = 0;

	if( !strcmp( argv[1], "-script" ) )
		strategy = new ScriptedStrategy(fp);
	if( !strcmp( argv[1], "-greedy" ) )
		strategy = new GreedyStrategy();
	if( !strcmp( argv[1], "-search" ) )
		strategy = new ExhaustiveSearchStrategy();
	if( !strcmp( argv[1], "-limited" ) )
		strategy = new LimitedSearchStrategy();
	if( !strcmp( argv[1], "-heuristic" ) )
		strategy = new HeuristicSearchStrategy();

	if( ! strategy ) {
		fprintf( stderr, "Invalid strategy specified\n" );
		return 3;
	}
	
	player->play(strategy);
	
	delete player;
	return 0;
}

