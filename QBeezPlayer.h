class Group;

#define MAXROWS 14
#define MAXCOLS 14

#define INVALID	(0xff)
#define NOGROUP (0xff)
#define EMPTY   ' '

#define LEFT 0
#define RIGHT 1

extern int maxSearchDepth;

#ifdef WIN32
void gettimeofday( struct timeval *tv, int );
#endif

typedef int COLORCOUNT;

class QBee {
public:
	char		color;
	unsigned int		r, c;
	unsigned int		group;
	unsigned int		next;
};

class Group {
public:
	unsigned int		n;		
	unsigned int		count;
	unsigned int		list;

};

class Screen {
private:
	int			rows, cols;
	int			left;
	QBee		qbeez[MAXROWS*MAXCOLS];
	int			groups;
	Group		group[MAXROWS*MAXCOLS];
	int			bonusScore;
	COLORCOUNT	colorCounts[26];
	
	void combineGroups(int r1, int c1, int r2, int c2 );
	
public:
	Screen( int r, int c );
	Screen( Screen* s );
	Screen( Screen* s, unsigned int dir );
	
	~Screen();

	int load( FILE* fp );
	int load( char* p );

#ifdef WIN32
	int load( int level, HWND hwnd );
#endif

	inline int getRows() { return rows; }
	inline int getCols() { return cols; }
	
	inline void set( unsigned int r, unsigned int c, char q );
	
	inline char get( unsigned int r, unsigned int c ) {
		return qbeez[r*cols+c].color;
	}

	inline void incColorCount( char color ) ;
	inline void decColorCount( char color ) ;
	
	inline void setGroup( unsigned int r, unsigned int c, unsigned int g );
	inline unsigned int getGroup( unsigned int r, unsigned int c );
	inline unsigned int getGroupSize( unsigned int r, unsigned int c );
	inline unsigned int getGroupSize( unsigned int g );

	inline int getLeft() { return left; }
	
	int getFreeGroup() ;

	void dropBlocks();
	void compress();
	
	void computeGroups();
	void removeGroup( unsigned int r, unsigned int c );
	void removeFromGroup( unsigned int p, unsigned int g );

	QBee* getGroupMember( unsigned int g );
	
	int getGroupCount() { return groups; }

	int getBonus();

	int computeHeuristicValue();
	
	void print(FILE* fp);
};

class Move {
protected:
	Screen*		screen;
	unsigned int		row, col;
	int			score;
	unsigned int		direction;

	Move*		next;

	Move() {}
	
public:
	Move( Screen* s, int initialScore, unsigned int r, unsigned int c );
	~Move();

	virtual void computeNextScreen( Screen* s );
	
	inline Screen* getScreen() { return screen; }
	Screen* takeScreen();
	int computeGroupScore( int size );

	inline unsigned int getDirection() { return direction; }
	inline unsigned int getRow() { return row; }
	inline unsigned int getCol() { return col; }
	
	inline int getScore() { return score; }
	int getFinalScore();
	virtual int getFinalHeuristicScore();
	inline void setNextMove( Move* m ) { next = m; }
	inline Move* getNextMove() { return next; }
};


class RotateMove : public Move {
private:
	int specialLeft;
	
public:
	RotateMove( Screen* s, int initialScore, unsigned int dir, int specialLeft );
	~RotateMove();

	virtual void computeNextScreen( Screen* s );
	virtual int getFinalHeuristicScore();
};


class Strategy {
public:
	virtual Move* getNextMove( Screen* s, int score, int special, int moveNumber ) = 0;
};

class SearchStrategy : public Strategy {
protected:
	int			searchDepth;
	int			count;
#ifdef PRINTSEARCH	
	int			sequence;
	int			bestSequenceScore;
	Move*		movestack[255];
#endif

	Move* searchMoves( Screen* s, int score, int special, int suppressSpecial );
	
	virtual int checkSearchDepth( int depth ) = 0 ;
	virtual int getScore( Move* m );

	SearchStrategy();
};

class ExhaustiveSearchStrategy : public SearchStrategy {
private:
	Move*		moves;
	int			searched;

protected:
	virtual int checkSearchDepth( int depth ) ;
	
public:
	ExhaustiveSearchStrategy();
	Move* getNextMove( Screen* s, int score, int special, int moveNumber ) ;
};


class LimitedSearchStrategy : public SearchStrategy {
protected:
	virtual int checkSearchDepth( int depth ) ;
	
public:
	LimitedSearchStrategy();
	Move* getNextMove( Screen* s, int score, int special, int moveNumber ) ;
};

class HeuristicSearchStrategy : public SearchStrategy {
protected:
	virtual int checkSearchDepth( int depth ) ;

	virtual int getScore( Move* m );

public:
	HeuristicSearchStrategy();
	Move* getNextMove( Screen* s, int score, int special, int moveNumber ) ;
};


class GreedyStrategy : public Strategy {
public:
	Move* getNextMove( Screen* s, int score, int special, int moveNumber ) ;
};

class ScriptedStrategy : public Strategy {
private:
	FILE* fp;
	
public:
	ScriptedStrategy( FILE* f ) : fp( f ) {}
	Move* getNextMove( Screen* s, int score, int special, int moveNumber ) ;
};


class QBeezPlayer {
private:
	Screen* screen;
	int score;
	int moveNumber;
	int special;
	int level;
#ifdef WIN32
	HWND hwnd;
#endif	

	void printMove( Move* move );
	
public:

	QBeezPlayer( FILE* fp );
	QBeezPlayer( int r, int c );
#ifdef WIN32
	QBeezPlayer( int level, HWND hwnd );
#endif
	
	int load( char* p );
	
	inline Screen* getScreen() {
		return screen;
	}
	
	void play( Strategy* strategy );
	void play( int s );

	Move* getNextMove( Strategy* strategy );

	void writeScreen( int s ) ;
	Move* readMove( int s ) ;
	
	void executeMove( Move* move );
};


// compute e - s, put result in r
inline void timediff( struct timeval &r,
					  const struct timeval &e,
					  const struct timeval &s ) {
	r.tv_usec = e.tv_usec - s.tv_usec ;
	r.tv_sec  = e.tv_sec  - s.tv_sec;
	if( r.tv_usec < 0 ) {
		r.tv_sec--;
		r.tv_usec += 1000000;
	}
}




inline void Screen::incColorCount( char color ) {
	if( colorCounts[color-'A'] == -1 )
		colorCounts[color-'A'] = 1 ;
	else
		colorCounts[color-'A'] += 1 ; 
}

inline void Screen::decColorCount( char color ) {
	colorCounts[color-'A'] -= 1 ; 
}

inline void Screen::set( unsigned int r, unsigned int c, char color ) {
	int p = r*cols+c;
	
	if( qbeez[p].color == EMPTY && color != EMPTY )
		left ++;

	if( qbeez[p].color != EMPTY && color == EMPTY )
		left --;

	if( qbeez[p].color != EMPTY )
		decColorCount(qbeez[p].color);

	if( color != EMPTY )
		incColorCount(color);
	
	qbeez[p].color = color;
	if( qbeez[p].group != INVALID )
		removeFromGroup( p, qbeez[p].group );
	qbeez[p].group = INVALID;
	qbeez[p].next = INVALID;
}

inline void Screen::setGroup( unsigned int r, unsigned int c, unsigned int g ) {
	unsigned int p = r*cols+c;
	qbeez[p].group = g;
	qbeez[p].next = group[g].list;
	group[g].list = p;
	group[g].count++;
}

inline unsigned int Screen::getGroup( unsigned int r, unsigned int c ) {
	return qbeez[r*cols+c].group;
}

inline unsigned int Screen::getGroupSize( unsigned int r, unsigned int c ) {
	return group[getGroup(r,c)].count;
}

inline unsigned int Screen::getGroupSize( unsigned int g ) {
	return group[g].count;
}

inline QBee* Screen::getGroupMember( unsigned int g ) {
	return qbeez + group[g].list;
}

