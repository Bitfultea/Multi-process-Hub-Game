# Multi-process-Hub-Game
three C99 programs to play a game

One of the programs will be called 2310hub it will control the game. The other two programs (2310alice and 2310bob
will be players.
The hub will be responsible for running the player processes and communicating with them via pipes. These
pipes will be connected to the players’ standard ins and outs so from their point of view communication will be
via stdin and stdout.
