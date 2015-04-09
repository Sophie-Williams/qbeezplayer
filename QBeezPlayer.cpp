#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef WIN32
#include <windows.h>
#else
#include <sys/time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#endif
#include <assert.h>

//#define PRINTSEARCH
#undef PRINTSEARCH

#include "QBeezPlayer.h"

#define MAX(a,b) ((a)>(b)?(a):(b))


#ifdef WIN32
void gettimeofday( struct timeval *tv, int ) {
	int t = GetTickCount();
	tv->tv_sec = t/1000;
	tv->tv_usec = (t%1000)*1000;
}
#endif

#define DBGG(x)  0		// group computation
#define DBGR(x)  0		// qbee removal
#define DBGS1(x)  0     // move search

#ifdef PRINTSEARCH
#define DBGS2(x)  printf x		// move search
#else
#define DBGS2(x)  0		// move search
#endif

int	maxSearchDepth = 6;

struct SearchDepths {
	int left;
	int depth;
};

SearchDepths mediumDepths [] = {
	{140,2},
 	{85,3},
	{46,4},
	{28,5},
	{28,6},
	{23,7},
	{0,8}
};

SearchDepths fastDepths [] = {
	{140,2},
 	{80,3},
	{44,4},
	{35,5},
	{26,6},
	{20,7},
	{0,8}
};

SearchDepths aggressiveDepths [] = {
 	{120,3},
	{70,4},
	{48,5},
	{40,6},
	{30,7},
	{0,8}
};

#define depths aggressiveDepths

#define DEPTHS (sizeof(depths)/sizeof(*depths))

void setSearchDepth( int qbeez ) {
	maxSearchDepth = 5;
	for( int i = 0; i < DEPTHS; i++ )
		if( qbeez >= depths[i].left ) {
			maxSearchDepth = depths[i].depth;
			printf( "%d QBeez Left. Setting search depth to %d.\n", qbeez, maxSearchDepth );
			break;
		}
}
			

Screen::Screen( int r, int c ) {
	if( r > MAXROWS ) {
	    fprintf( stderr, "Row size too large\n" );
		exit(-3);
	}

	if( c > MAXCOLS ) {
	    fprintf( stderr, "Row size too large\n" );
		exit(-3);
	}

	left = r*c;
	rows = r;
	cols = c;
	groups = 0;
	bonusScore = 0;
	int i;
	for( i = 0; i < rows*cols; i++ ) {
		qbeez[i].color = INVALID;
		qbeez[i].r = i/rows;
		qbeez[i].c = i%rows;
		qbeez[i].group = INVALID;
		qbeez[i].next = INVALID;
		
		group[i].n = (unsigned int)i;
		group[i].count = 0;
		group[i].list = INVALID;
	}

	for( i = 0; i < 26; i++ ) {
		colorCounts[i] = -1;
	}
}

Screen::Screen( Screen* s ) {
	rows = s->rows;
	cols = s->cols;
	groups = s->groups;
	left = s->left;
	bonusScore = 0;


	int i;
	for(int r = 0; r < rows; r++ ) {
		for(int c = 0; c < cols; c++ ) {
			i = r*cols+c;
			
			qbeez[i].color = s->qbeez[i].color;
			qbeez[i].r = r;
			qbeez[i].c = c;
			qbeez[i].group = s->qbeez[i].group;
			qbeez[i].next = INVALID;
		
			group[i].n = (unsigned int)i;
			group[i].count = 0;
			group[i].list = INVALID;
		}
	}

	memcpy( colorCounts, s->colorCounts, sizeof(colorCounts) );
}

Screen::Screen( Screen* s, unsigned int dir ) {
	rows = s->rows;
	cols = s->cols;
	groups = 0;
	left = s->left;
	bonusScore = 0;

	for( int r = 0; r < rows; r++ ) {
		for( int c = 0; c < cols; c++ ) {
			int i = r*cols+c;

			int l;
			if( dir == RIGHT )
				l = (rows-1-c)*cols+r;
			else
				l = (rows-1-r)+cols*c;
			
			qbeez[i].color = s->qbeez[l].color;
			qbeez[i].r = r;
			qbeez[i].c = c;
			qbeez[i].group = INVALID;
			qbeez[i].next = INVALID;
		
			group[i].n = (unsigned int)i;
			group[i].count = 0;
			group[i].list = INVALID;
			
		}
	}
	memcpy( colorCounts, s->colorCounts, sizeof(colorCounts) );
}

Screen::~Screen() {
}

void Screen::removeFromGroup( unsigned int p, unsigned int g ) {
	for( unsigned int* pp = &group[g].list ; *pp != INVALID; pp = &qbeez[*pp].next ) {
		if( *pp == p ) {
			*pp = qbeez[*pp].next;
			group[g].count--;
			break;
		}
	}
}

int Screen::getFreeGroup() {
	for( int i = 0; i < groups; i++ )
		if( group[i].count == 0 )
			return i;
	return groups++;
}

int Screen::load( FILE* fp ) {
	for( int i = 0; i < rows; i++ ) {
		for( int j = 0; j < cols; j++ ) {
			char q = fgetc(fp);
			set( i, j, q );
		}
		if( fgetc(fp) != '\n' )
			return -1;
	}
	return 0;
}

int Screen::load( char* p ) {
	for( int i = 0; i < rows; i++ ) {
		for( int j = 0; j < cols; j++ ) {
			char q = *p++;
			set( i, j, q );
		}
		
		if( *p == '\r' )
			p++;
		
		if( *p++ != '\n' ) {
			return -1;
		}
	}

	return 0;
}

int Screen::getBonus() {
	if( bonusScore != 0 )
		return bonusScore;

	if( left == 0 )
		bonusScore = 100000;
	else {
		bonusScore = 51000-(left*2000);
		if( bonusScore < 0 )
			bonusScore = 100;
	}
	return bonusScore;
}

void Screen::combineGroups( int r1, int c1, int r2, int c2 ) {
	unsigned int g1 = getGroup(r1,c1);
	unsigned int g2 = getGroup(r2,c2);

	if( g1 == g2 )
		return;
	
	DBGG(( "Putting (%d,%d) and (%d,%d) in same group\n", r1, c1, r2, c2 ));
	
	if( g1 != NOGROUP && g2 == NOGROUP ) {
		DBGG((" Putting QBEE at %d,%d in %02X\n", r2, c2, g1 ));
		setGroup( r2, c2, g1 );
	}

	if( g1 == NOGROUP && g2 != NOGROUP ) {
		DBGG((" Putting QBEE at %d,%d in %02X\n", r1, c1, g2 ));
		setGroup( r1, c1, g2 );
	}
		
	if( g1 != NOGROUP && g2 != NOGROUP ) {
		DBGG(( " Combining groups %02X and %02X\n", g1, g2 ));
		unsigned int pNext;
		for( unsigned int p = group[g1].list; p != INVALID; p = pNext ) {
			DBGG(("  Putting QBEE at %d,%d in %02X\n", qbeez[p].r, qbeez[p].c, g2 ));
			pNext = qbeez[p].next;
			qbeez[p].group = g2;
			qbeez[p].next = group[g2].list;
			group[g2].list = p;
			group[g2].count++;
		}
		group[g1].count = 0;
		group[g1].list = INVALID;
	}
}

void Screen::computeGroups() {
	DBGG(("assigning groups\n"));
	int i,j;

	for( i = 0; i < rows*cols; i++ ) {
		qbeez[i].group = INVALID;
	}
	
	for( i = 0; i < rows; i++ ) {
		for( j = 0; j < cols; j++ ) {

			char c = get(i,j);
			
			if( c == EMPTY )
				continue;
			
			// check left
			if( j > 0 )
				if( get(i,j-1) == c ) {
					combineGroups( i, j, i, j-1 );
				}

			// check above
			if( i > 0 )
				if( get(i-1,j) == c ) {
					combineGroups( i, j, i-1, j );
				}

			if( getGroup(i,j) == NOGROUP ) {
				unsigned int g = getFreeGroup();
				DBGG(("Putting QBEE at %d,%d in singleton group %02X\n", i, j, g ));
				setGroup(i,j,g);
			}
			
			// check right
			if( j+1 < cols )
				if( get(i,j+1) == c ) {
					combineGroups( i, j, i, j+1 );
				}

			// check below
			if( i+1 < rows )
				if( get(i+1,j) == c ) {
					combineGroups( i, j, i+1, j );
				}
		}
	}
}

void Screen::print(FILE* fp) {
	int i,j;
	
	for( j = 0; j < cols; j++ ) {
		fprintf( fp, "+-----" );
	}
	fprintf( fp, "+\n" );
	
	for( i = 0; i < rows; i++ ) {
		for( j = 0; j < cols; j++ ) {
			if( get(i,j) == ' ' )
				fprintf( fp, "|     " );
			else
				fprintf( fp, "|%c(%02X)", get(i,j), (int)getGroup(i,j) );
		}
		fprintf( fp, "|\n" );
	}

	for( j = 0; j < cols; j++ ) {
		fprintf( fp, "+-----" );
	}
	fprintf( fp, "+\nGROUP COUNTS:\n" );

	for( i = 0; i < groups; i++ ) {
		fprintf( fp, "Group %02X: %02d", i, group[i].count );
		if( (i+1)%4 == 0 )
			fprintf( fp, "\n" );
		else
			fprintf( fp, "\t" );
	}
	fprintf( fp, "\nCOLOR COUNTS:\n" );

	int n = 0;
	for( i = 0; i < 26; i++ ) {
		if( colorCounts[i] != -1 ) {
			fprintf( fp, "%c: %02d", i+'A', colorCounts[i] );
			if( (++n)%5 == 0 )
				fprintf( fp, "\n" );
			else
				fprintf( fp, "\t" );
		}
	}
	fprintf( fp, "\n" );
	
	
	fflush(fp);
}

void Screen::removeGroup( unsigned int r, unsigned int c ) {
	unsigned int g = getGroup( r, c );

	// remove QBEEZ group and drop from upper rows
	for( int i = 0; i < rows; i++ ) {
		for( int j = 0; j < cols; j++ ) {
			if( getGroup(i,j) == g ) {
				DBGR(( "Removing QBEE at %d,%d\n", i,j ));
				for( int n = i; n > 0; n-- ) {
					set( n, j, get( n-1, j ) );
				}
				set( 0, j, ' ' );
			}
		}
	}

	// now compress columns
	compress();
}

void Screen::dropBlocks() {
	for( int i = 0; i < rows; i++ ) {
		for( int j = 0; j < cols; j++ ) {
			if( get(i,j) == EMPTY ) {
				for( int n = i; n > 0; n-- ) {
					set( n, j, get( n-1, j ) );
				}
				set( 0, j, EMPTY );
			}
		}
	}
	compress();
}

void Screen::compress() {
	for( int i = cols-1; i > 0; i-- ) {
		if( get( rows-1, i ) == ' ' ) {
			DBGR(( "found empty column at %d\n", i ));
			// column i is empty.  search left for first non-empty column
			int t;
			for( t = i-1; t >= 0 && get( rows-1, t ) == ' '; t-- )
				;
			DBGR(( "first non-empty column is %d\n", t ));
			// shift over
			for( int c = 0; c <= t; c++ ) {
				DBGR(( "moving from column %d to column %d\n", t-c, i-c ));
				for( int r = 0; r < rows; r++ ) {
					set( r, i-c, get( r, t-c ) );
					set( r, t-c, ' ' );
				}
			}
		}
	}
}


//
// heruistic value:
//    for each group > 2, .6 * group score
//    a singleton = -50000
//    for each color that has 0, +5000
int Screen::computeHeuristicValue() {
	int i;
	double h = 0;
	
	for( i = 0; i < groups; i++ ) {
		if( group[i].count > 2 )
			h = h + (double)(group[i].count * group[i].count * 70);
		if( group[i].count == 1 )
			h = h - 1000.0;
	}

	int singleton = 0;
	for( i = 0; i < 26; i++ ) {
		if( colorCounts[i] == 1 )
			singleton = 1;
		if( colorCounts[i] == 0 )
			h += 5000.0;
		if( colorCounts[i] > 2 )
			h += colorCounts[i]*colorCounts[i]*10;
	}

	if( singleton )
		h -= 50000.0;
	
	return (int) h;
}

Move::Move( Screen* s, int initialScore, unsigned int r, unsigned int c ) : row(r), col(c) {
	score = initialScore + computeGroupScore( s->getGroupSize( row, col ) );
	screen = 0;
	next = 0;
	direction = INVALID;
}

void Move::computeNextScreen( Screen* s) {
	screen = new Screen( s );
	screen->removeGroup( row, col );
	screen->computeGroups();
}

Move::~Move() {
	delete screen;
	delete next;
}

int Move::getFinalScore() {
	if( next == 0 )
		return score + screen->getBonus();
	return next->getFinalScore();
}

int Move::getFinalHeuristicScore() {
	if( next == 0 )
		return score + screen->getBonus() + screen->computeHeuristicValue();
	return next->getFinalHeuristicScore();
}

int Move::computeGroupScore( int size ) {
	return size*size*100;
}

inline Screen* Move::takeScreen() {
	Screen* s = screen;
	screen = 0;
	return s;
}

RotateMove::RotateMove( Screen* s, int initialScore, unsigned int dir, int special ) {
	specialLeft = special;
	score = initialScore;
	screen = 0;
	next = 0;
	row = INVALID;
	col = INVALID;
	direction = dir;
}

void RotateMove::computeNextScreen( Screen* s ) {
	screen = new Screen( s, direction );
	screen->dropBlocks();
	screen->computeGroups();
}

int RotateMove::getFinalHeuristicScore() {
	int penalty = 0 ;
	switch( specialLeft ) {
	case 2:
		penalty = 10000;
		break;
	case 1:
		penalty = 25000;
		break;
	case 0:
		penalty = 35000;
		break;
	}
			
	if( next == 0 )
		return score + screen->getBonus() + screen->computeHeuristicValue() - penalty;
	return next->getFinalHeuristicScore() - penalty;
}



QBeezPlayer::QBeezPlayer( FILE* fp ) : score( 0 ) {
	moveNumber = 0;
	special = 3;
	
	int r,c;
	fscanf( fp, "%d,%d\n", &r, &c );
	screen = new Screen( r, c );
	if( screen->load( fp ) != 0 ) {
		fprintf( stderr, "qbeezplay: invalid screen file\n" );
		exit(-1);
	}
	
	screen->computeGroups();
}

QBeezPlayer::QBeezPlayer( int r, int c ) : score( 0 ) {
	screen = new Screen( r, c );
	moveNumber = 0;
	special = 3;
}

int QBeezPlayer::load( char* p ) {
	int result = screen->load(p);
	if( result != 0 )
		return result;
	screen->computeGroups();
	return 0;
}

void QBeezPlayer::printMove( Move* move ) {
	if( move->getRow() != INVALID )
		printf("\nMOVE: group %02X : %d %c's at (%d,%d) SCORE=%d HV=%d\n",
			   screen->getGroup( move->getRow(), move->getCol() ),
			   screen->getGroupSize( move->getRow(), move->getCol() ),				   
			   screen->get( move->getRow(), move->getCol() ),
			   move->getRow(), move->getCol(), move->getScore(),
			   move->getFinalHeuristicScore() );
	else {
		printf("\nROTATE %s SCORE=%d HV=%d\n", move->getDirection() == LEFT ? "LEFT" : "RIGHT",
			   move->getScore(),
			   move->getFinalHeuristicScore()  );
	}
}

void QBeezPlayer::play( Strategy* strategy ) {
	struct timeval tvStart, tvEnd, tvDiff;
	Move* move;
	
	moveNumber = 0;
	special = 3;

	gettimeofday(&tvStart,0);
	
	while( (move = strategy->getNextMove(screen,score,special,moveNumber++)) != 0 ) {

		printMove( move );

		if( move->getRow() == INVALID )
				special--;

		executeMove( move );
		
		delete screen;
		screen = move->takeScreen();
		screen->print(stdout);
		printf("\n");
		score = move->getScore();
		delete move;
	}

	gettimeofday(&tvEnd,0);

	printf( "%d QBeez left: Bonus = %d\n", screen->getLeft(), screen->getBonus() );
	printf( "FINAL SCORE = %d\n", score + screen->getBonus() );
	
	timediff( tvDiff, tvEnd, tvStart );
	printf("Level complete. %d.%03d sec\n", tvDiff.tv_sec, tvDiff.tv_usec / 1000 );
}

Move* QBeezPlayer::getNextMove( Strategy* strategy ) {
	Move* move;

	move = strategy->getNextMove(screen,score,special,moveNumber++);
	if( ! move )
		return 0;

	printMove( move );

	if( move->getRow() == INVALID )
		special--;

	delete screen;
	screen = move->takeScreen();
	screen->print(stdout);
	score = move->getScore();

	return move;
}


void QBeezPlayer::writeScreen( int s ) {
	char szBuf[512];
	char *p = szBuf;

	p += sprintf( p, "%d,%d\n", screen->getRows(), screen->getCols() );

	for( unsigned int r = 0; r < screen->getRows(); r++ ) {
		for( unsigned int c = 0; c < screen->getCols(); c++ ) 
			*p++ = screen->get( r, c );
		*p++ = '\n';
	}
	*p++ = '\n';
	*p = '\0';

	int len = p - szBuf;
	int sent = 0, res;
	while( sent < len ) {
		res = send( s, szBuf + sent, len - sent, 0 );
		if( res > 0 )
			sent += res;
		else
			break;
	}
}

Move* QBeezPlayer::readMove( int s ) {
	char szBuf[512];
	char *p = szBuf;

	while( recv( s, p, 1, 0 ) == 1 ) {
		if( *p == '\n' )
			break;
		p++;
	}

	*p = 0;

	if( *szBuf == '\0' )
		return 0;
	
	Move* move;
	if( *szBuf == 'M' ) {
		int r,c ;
		sscanf( szBuf+2, "%d,%d", &r, &c );
		move = new Move( screen, score, r, c );
	} else {
		move = new RotateMove( screen, score, *(szBuf+2) == 'R' ? RIGHT : LEFT, special-1 );
	}
	move->computeNextScreen( screen );

	return move;
}

//
// play where the moves are coming from the remote socket
//
void QBeezPlayer::play( int s ) {
	Move* move;
	int moveNumber = 0;
	int special = 3;

	writeScreen(s);
	while( (move = readMove(s)) != 0 ) {
		if( move->getRow() != INVALID )
			printf("\nMOVE: group %02X : %c at (%d,%d) SCORE=%d\n",
				   screen->getGroup( move->getRow(), move->getCol() ),
				   screen->get( move->getRow(), move->getCol() ),
				   move->getRow(), move->getCol(), move->getScore() );
		else {
			printf("\nROTATE %s\n", move->getDirection() == LEFT ? "LEFT" : "RIGHT" );
			special--;
		}

		executeMove( move );
		
		delete screen;
		screen = move->takeScreen();
		screen->print(stdout);
		score = move->getScore();
		delete move;
	}
	printf( "%d QBeez left: Bonus = %d\n", screen->getLeft(), screen->getBonus() );
	printf( "FINAL SCORE = %d\n", score + screen->getBonus() );
}



Move* ScriptedStrategy::getNextMove( Screen* s, int score, int special, int moveNumber ) {
	int r, c;
	if( fscanf( fp, "%d,%d\n", &r, &c ) > 0 ) {
		Move* m;
		if( r < 0 ) {
			m = new RotateMove( s, score, c < 0 ? LEFT : RIGHT, 0 );
		} else {
			m = new Move( s, score, r, c );
		}
		m->computeNextScreen( s );
		return m;
	}
	return 0;
}

Move* GreedyStrategy::getNextMove( Screen* s, int score, int special, int moveNumber ) {
	Move* mBest = 0;

	for( int i = 0; i < s->getGroupCount(); i++ ) {
		if( s->getGroupSize(i) > 1 ) {
			QBee* q = s->getGroupMember(i) ;
			DBGS1(( "Considering group %02X: %d QBeez at (%d,%d)\n",
					i, s->getGroupSize(i), q->r, q->c ));
			Move* m = new Move( s, score, q->r, q->c );
			if( mBest == 0 || m->getScore() > mBest->getScore() ) {
				delete mBest;
				mBest = m;
			} else {
				delete m;
			}
		}
	}
	
	if( mBest == 0 ) {
		DBGS1(( "NO MOVES AVAILABLE!\n" ));
		return 0;
	}

	DBGS1(( "Best Move is group %02X at %d,%d\n", s->getGroup( mBest->getRow(), mBest->getCol() ),
		   mBest->getRow(), mBest->getCol() ));
	
	mBest->computeNextScreen( s );
	
	return mBest;
}


SearchStrategy::SearchStrategy() : searchDepth(0) {
	count = 0;
}

Move* SearchStrategy::searchMoves( Screen* s, int score, int special, int suppressSpecial ) {
	Move* mBest = 0;
	int bestScore = 0;
	
	if( ! checkSearchDepth( searchDepth ) )
		return 0;

#ifdef PRINTSEARCH		
	if( searchDepth == 0 ) {
		sequence = 0;
		bestSequenceScore = 0;
	}
#endif
	
    searchDepth++;
	for( int i = 0; i < s->getGroupCount(); i++ ) {
		if( s->getGroupSize(i) > 1 ) {
			QBee* q = s->getGroupMember(i) ;
			DBGS1(( "EXS %d: Considering group %02X: %d QBeez at (%d,%d)\n",
				   searchDepth, i, s->getGroupSize(i), q->r, q->c ));

			Move* m = new Move( s, score, q->r, q->c );
			count++;

#ifdef PRINTSEARCH				
			movestack[searchDepth-1] = m;
#endif
			
			m->computeNextScreen( s );
			Move* mNext = searchMoves( m->getScreen(), m->getScore(), special, MAX(0,suppressSpecial-1) );
			m->setNextMove( mNext );
			int moveScore = getScore(m);
			
#ifdef PRINTSEARCH
			if( mNext == 0 ) {
				DBGS2(( "%4d : ", count ));
				for( int i = 0; i < searchDepth; i++ ) {
					if( movestack[i]->getDirection() == INVALID )
						DBGS2(( "(%d,%d) ", movestack[i]->getRow(), movestack[i]->getCol() ));
					else
						DBGS2(( "ROT%s ", movestack[i]->getDirection() == LEFT ? "LEFT" : "RIGHT" ));
				}
				DBGS2(( "= %d\n", moveScore ));

				if( moveScore > bestSequenceScore ) {
					bestSequenceScore = moveScore;
					sequence = count;
				}
			}
#endif				
			
#ifdef PRINTSEARCH				
			if( searchDepth == 1 )
			{
				if( ! mBest )
					DBGS2(( "NO MBEST yet\n" ));
				else
					DBGS2(( "MBEST = %d, M = %d\n", bestSequenceScore, moveScore ));
			}
#endif

			if( mBest == 0 || (moveScore > bestScore) ) {
				bestScore = moveScore;
				delete mBest;
				mBest = m;
				if( searchDepth == 1 ) {
					DBGS2(( "Setting MBEST to %d,%d\n", m->getRow(), m->getCol() ));
				}
			} else {
				delete m;
			}
		}
	}

	if( s->getLeft() > 2 && suppressSpecial == 0 && special > 0 ) {
		for( int i = 0; i <=1; i++ ) {
			Move* m;

			if( i == 0 )
				m = new RotateMove( s, score, RIGHT, special-1 );
			else
				m = new RotateMove( s, score, LEFT, special-1 );
			
			count++;
			
#ifdef PRINTSEARCH			
			movestack[searchDepth-1] = m;
#endif
			
			m->computeNextScreen( s );
			Move* mNext = searchMoves( m->getScreen(), m->getScore(), special-1, 1 );
			m->setNextMove( mNext );
			int moveScore = getScore(m);
#ifdef PRINTSEARCH
			if( mNext == 0 ) {
				DBGS2(( "%4d : ", count ));
				for( int i = 0; i < searchDepth; i++ ) {
					if( movestack[i]->getDirection() == INVALID )
						DBGS2(( "(%d,%d) ", movestack[i]->getRow(), movestack[i]->getCol() ));
					else
						DBGS2(( "ROT%s ", movestack[i]->getDirection() == LEFT ? "LEFT" : "RIGHT" ));
				}
				
				DBGS2(( "= %d\n", moveScore ));

				if( moveScore > bestSequenceScore ) {
					bestSequenceScore = moveScore;
					sequence = count;
				}
			}
#endif

			if( mBest == 0 || (moveScore > bestScore) ) {
				bestScore = moveScore;
				delete mBest;
				mBest = m;
				if( searchDepth == 1 ) {
					DBGS2(( "Setting MBEST to ROTATE %s\n", i == 0 ? "RIGHT" : "LEFT" ));
				}
			} else {
				delete m;
			}
		}
	}
	
    searchDepth--;

#ifdef PRINTSEARCH				
	if( searchDepth == 0 )
		DBGS2(( "Selecting sequence %d\n", sequence ));
#endif
	
	return mBest;
}

int SearchStrategy::getScore( Move* m ) {
	return m->getFinalScore();
}

ExhaustiveSearchStrategy::ExhaustiveSearchStrategy() : moves(0), searched(0){
}


int ExhaustiveSearchStrategy::checkSearchDepth( int depth ) {
	return 1;
}

Move* ExhaustiveSearchStrategy::getNextMove( Screen* s, int score, int special, int moveNumber ) {
	
	if( ! searched ) {
		struct timeval tvStart, tvEnd, tvDiff;
		gettimeofday(&tvStart,0);
		moves = searchMoves(s,score,special,MAX(5-moveNumber,0));
		gettimeofday(&tvEnd,0);
		timediff( tvDiff, tvEnd, tvStart );
		printf("Exhaustive search complete. %d moves considered, %d.%03d sec\n",
			   count, tvDiff.tv_sec, tvDiff.tv_usec / 1000 );
		searched = 1;
	}

	if( moves == 0 ) {
		printf( "NO MOVES AVAILABLE!\n" );
		return 0;
	}

	Move* mBest = moves;
	moves = moves->getNextMove();
	mBest->setNextMove(0);
	
	DBGS1(( "Best Move is group %02X at %d,%d\n", s->getGroup( mBest->getRow(), mBest->getCol() ),
		   mBest->getRow(), mBest->getCol() ));
	
	return mBest;
}


LimitedSearchStrategy::LimitedSearchStrategy() {
}

int LimitedSearchStrategy::checkSearchDepth( int depth ) {
	if( searchDepth == maxSearchDepth )
		return 0;
	return 1;
}

Move* LimitedSearchStrategy::getNextMove( Screen* s, int score, int special, int moveNumber ) {
	count = 0;
	
	struct timeval tvStart, tvEnd, tvDiff;
	gettimeofday(&tvStart,0);

	setSearchDepth( s->getLeft() );
	
	assert( searchDepth == 0 );
	Move* mBest = searchMoves(s,score,special,MAX(5-moveNumber,0));

	gettimeofday(&tvEnd,0);
	timediff( tvDiff, tvEnd, tvStart );
	printf("Search complete. %d moves considered, %d.%03d sec\n",
		   count, tvDiff.tv_sec, tvDiff.tv_usec / 1000 );

	if( mBest == 0 ) {
		printf( "NO MOVES AVAILABLE!\n" );
		return 0;
	}

	delete mBest->getNextMove();
	mBest->setNextMove(0);
	
	DBGS1(( "Best Move is group %02X at %d,%d\n", s->getGroup( mBest->getRow(), mBest->getCol() ),
		   mBest->getRow(), mBest->getCol() ));
	
	return mBest;
}



HeuristicSearchStrategy::HeuristicSearchStrategy() {
}

int HeuristicSearchStrategy::checkSearchDepth( int depth ) {
	if( searchDepth == maxSearchDepth )
		return 0;
	return 1;
}

int HeuristicSearchStrategy::getScore( Move* m ) {
	return m->getFinalHeuristicScore();
}


Move* HeuristicSearchStrategy::getNextMove( Screen* s, int score, int special, int moveNumber ) {
	count = 0;
	
	struct timeval tvStart, tvEnd, tvDiff;
	gettimeofday(&tvStart,0);

	setSearchDepth( s->getLeft() );
	
	assert( searchDepth == 0 );
	Move* mBest = searchMoves(s,score,special,MAX(5-moveNumber,0));

	gettimeofday(&tvEnd,0);
	timediff( tvDiff, tvEnd, tvStart );
	printf("Search complete. %d moves considered, %d.%03d sec\n",
		   count, tvDiff.tv_sec, tvDiff.tv_usec / 1000 );

	if( mBest == 0 ) {
		printf( "NO MOVES AVAILABLE!\n" );
		return 0;
	}

	delete mBest->getNextMove();
	mBest->setNextMove(0);
	
	DBGS1(( "Best Move is group %02X at %d,%d\n", s->getGroup( mBest->getRow(), mBest->getCol() ),
		   mBest->getRow(), mBest->getCol() ));
	
	return mBest;
}




