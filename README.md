# QBeez Player

This application implemented a "bot" which played the Shockwave Flash game
QBeez (http://games.skunkstudios.com/skunk-originals/skunk-web-games/web-qbeez).
The application consisted of a Win32 app that read the screen and manipulated
the Mouse input queue to perform Moves and a Linux app that computed a game
strategy to play the levels.

The game engine can be tested using the `qbeezplay` tool.  The first argument
is the strategy to use, the 2nd argument is a text file giving an initial
screen layout

To play level 1 using the greedy strategy:
```
qbeezplay -greedy level1.txt
```

To play level 2 using the heuristic strategy:
```
qbeezplay -heuristic level2.txt
```
