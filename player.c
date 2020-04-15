#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "player.h"
#include "util.h"

/* quit the game after printing the correct error message
 *
 * @param status - the exit status to use
 */
void quit_game(enum ExitStatus status) {
    if (status == USAGE) {
        fprintf(stderr, "Usage: player players myid threshold handsize\n");
    } else if (status == INV_PLAYERS) {
        fprintf(stderr, "Invalid players\n");
    } else if (status == INV_POSITION) {
        fprintf(stderr, "Invalid position\n");
    } else if (status == INV_THRESHOLD) {
        fprintf(stderr, "Invalid threshold\n");
    } else if (status == INV_HAND) {
        fprintf(stderr, "Invalid hand size\n");
    } else if (status == INV_MESS) {
        fprintf(stderr, "Invalid message\n");
    } else if (status == END_OF_FILE) {
        fprintf(stderr, "EOF\n");
    }
    exit(status);
}

/* Determine the type of message that was received
 * If it does not match a known type, returns INVALID_MESSAGE
 *
 * @param message - string containing message from hub
 * @return - type of message
 */
enum HubMessage categorise_message(char* message) {
    if (strncmp(message, "HAND", strlen("HAND")) == 0) {
        return HAND;
    } else if (strncmp(message, "NEWROUND", strlen("NEWROUND")) == 0) {
        return NEW_ROUND;
    } else if (strncmp(message, "PLAYED", strlen("PLAYED")) == 0) {
        return PLAYED;
    } else if (strncmp(message, "GAMEOVER", strlen("GAMEOVER")) == 0) {
        return GAME_OVER;
    } else {
        return INVALID_MESSAGE;
    }
}

/* Process a HAND message from the hub
 * Store the contents of message in the game struct
 *
 * WARNING - game struct modified before entire message is parsed
 *
 * @param message - message from hub (known to be HAND already)
 * @param game - main game struct
 * @return false if processing was successful, true otherwise
 */
// 0 for success, 1 for failure
bool process_hand_message(char* message, Game* game) {
    message += strlen("HAND");

    char* end;
    int handSize = strtol(message, &end, 10);
    if (handSize != game->turnsRemaining || *end != ',') {
        return true;
    }

    for (int i = 0; i < handSize; i++) {
        // skip over comma
        message = end + 1;

        if (message[0] != 'D' && message[0] != 'H' && message[0] != 'S'
                && message[0] != 'C') {
            return true;
        }
        game->hand[i].suit = message[0];

        // finished with suit, move to rank
        message++;
        int rank = strtol(message, &end, RANK_BASE);
        if (rank < MIN_RANK || rank > MAX_RANK ||
                (i < handSize - 1 && *end != ',') ||
                (i == handSize - 1 && *end != '\0')) {
            return true;
        }
        game->hand[i].rank = rank;
    }
    return false;
}

/* Process a NEWROUND message from the hub
 * store the contents of message in the game struct
 *
 * @param message - message from hub (known to be NEWROUND already)
 * @param game - main game struct
 * @return false if processing was successful, true otherwise
 */
bool process_new_round_message(char* message, Game* game) {
    message += strlen("NEWROUND");

    if (game->turnsRemaining == 0) {
        return true;
    }

    char* end;
    int leadPlayer = strtol(message, &end, 10);
    if (leadPlayer >= game->numPlayers || *end) {
        return true;
    }
    game->leadPlayer = leadPlayer;
    
    // We know this is the first player of the round
    game->playerCount = 0;
    return false;
}

/* Process a PLAYED message from the hub
 * store the contents of message in the game struct
 *
 * @param message - message from hub (known to be PLAYED already)
 * @param game - main game struct
 * @return false if processing was successful, true otherwise
 */
bool process_played_message(char* message, Game* game) {
    message += strlen("PLAYED");

    char* end;
    int playerNumber = strtol(message, &end, 10);
    if (playerNumber != ((game->playerCount + game->leadPlayer) 
            % game->numPlayers)
            || *end != ',') {
        return true;
    }

    message = end + 1;
    char suit = message[0];
    message++;
    int rank = strtol(message, &end, RANK_BASE);
    if ((suit != 'D' && suit != 'H' && suit != 'S' && suit != 'C') ||
            rank < MIN_RANK || rank > MAX_RANK || *end != '\0') {
        return true;
    }

    game->turn[playerNumber].suit = suit;
    game->turn[playerNumber].rank = rank;
    game->playerCount++;
    return false;
}

/* Process a GAMEOVER message from the hub
 * 
 * @param message - message from hub (known to be GAMEOVER already)
 * @param game - main game struct
 * @return false if processing was successful, true otherwise
 */
bool process_game_over_message(char* message, Game* game) {
    message += strlen("GAMEOVER");
    if (*message != '\0') {
        return true;
    }
    return false;
}

/* play a turn to the hub
 * chooses a card (based on player strategy)
 *
 * @param game - main game struct
 */
void play_turn(Game* game) {
    int chosenCard = choose_card(game);
    printf("PLAY%c%x\n", game->hand[chosenCard].suit,
            game->hand[chosenCard].rank);
    fflush(stdout);

    game->turn[game->playerID] = game->hand[chosenCard];
    // Disable card in hand
    game->hand[chosenCard].rank = INVALID;
    game->playerCount++;
}

/* Determine which player won a round and give them points and D cards
 *
 * @param game - main game struct
 */
void find_winner(Game* game) {
    char leadSuit = game->turn[game->leadPlayer].suit;
    int dPlayed = 0;

    int winnerIndex = game->leadPlayer;
    int maxRank = game->turn[game->leadPlayer].rank;

    for (int i = 0; i < game->numPlayers; i++) {
        if (game->turn[i].suit == leadSuit && game->turn[i].rank > maxRank) {
            maxRank = game->turn[i].rank;
            winnerIndex = i;
        }
        if (game->turn[i].suit == 'D') {
            dPlayed++;
        }
    }

    game->playerPoints[winnerIndex]++;
    game->dWon[winnerIndex] += dPlayed;
}

/* Print message to stderr at end of each round
 * Also handle end of round game state updates
 *
 * @param - main game struct
 */
void end_of_round(Game* game) {
    fprintf(stderr, "Lead player=%d: ", game->leadPlayer);

    for (int i = 0; i < game->numPlayers; i++) {
        int playerNum = (i + game->leadPlayer) % game->numPlayers;
        fprintf(stderr, "%c.%x", game->turn[playerNum].suit,
                game->turn[playerNum].rank);
        fprintf(stderr, "%c", i == game->numPlayers - 1 ? '\n' : ' ');
    }

    find_winner(game);
    game->turnsRemaining--;
}

/* Set up a new game struct based on command line argument values
 *
 * @param numPlayers - number of players in the game
 * @param playerID - ID of this player
 * @param threshold - threshold of diamond cards
 * @param handSize - number of cards in initial hand
 * @return newly populated game struct
 */
Game setup_game(int numPlayers, int playerID, int threshold, int handSize) {
    Game game;

    game.numPlayers = numPlayers;
    game.playerID = playerID;
    game.threshold = threshold;
    game.handSize = handSize;
    game.turnsRemaining = handSize;

    game.hand = malloc(sizeof(Card) * handSize);
    for (int i = 0; i < handSize; i++) {
        game.hand[i].rank = INVALID;
    }

    game.leadPlayer = -1; // not a valid player yet
    game.turn = malloc(sizeof(Card) * numPlayers);

    game.playerPoints = malloc(sizeof(int) * numPlayers);
    game.dWon = malloc(sizeof(int) * numPlayers);
    for (int i = 0; i < numPlayers; i++) {
        game.playerPoints[i] = 0;
        game.dWon[i] = 0;
    }

    return game;
}

/* Play a complete game
 * Features main game loop
 *
 * Will exit the game if EOF is received prematurely or a message is invalid
 *
 * @param game - main game struct
 */
void play_game(Game* game) {
    bool gameOver = false;
    while (!gameOver) {
        char* message;
        int length = read_line(stdin, &message);
        if (length == 0 && feof(stdin)) {
            quit_game(END_OF_FILE);
        }
        //fprintf(stderr, "%s\n", message);
        switch(categorise_message(message)) {
            case HAND:
                if (process_hand_message(message, game)) {
                    quit_game(INV_MESS);
                }
                break;
            case NEW_ROUND:
                if (process_new_round_message(message, game)) {
                    quit_game(INV_MESS);
                }
                if (game->leadPlayer == game->playerID) {
                    play_turn(game);
                }
                break;
            case PLAYED:
                if (process_played_message(message, game)) {
                    quit_game(INV_MESS);
                }
                if (game->playerCount == game->numPlayers) {
                    end_of_round(game);
                } else if ((game->leadPlayer + game->playerCount) 
                        % game->numPlayers == game->playerID) {
                    play_turn(game);
                    if (game->playerCount == game->numPlayers) {
                        end_of_round(game);
                    }
                }
                break;
            case GAME_OVER:
                if (process_game_over_message(message, game)) {
                    quit_game(INV_MESS);
                }
                gameOver = true;
                break;
            case INVALID_MESSAGE:
                quit_game(INV_MESS);
        }
    }
}

int main(int argc, char** argv) {
    if (argc != NUM_ARGS) {
        quit_game(USAGE);
    }
    // parse command line arguments
    char* end;
    int numPlayers = strtol(argv[1], &end, 10);
    if (numPlayers < 2 || *end) {
        quit_game(INV_PLAYERS);
    }
    int playerID = strtol(argv[2], &end, 10);
    if (playerID < 0 || playerID >= numPlayers || *end) {
        quit_game(INV_POSITION);
    }
    int threshold = strtol(argv[3], &end, 10);
    if (threshold < 2 || *end) {
        quit_game(INV_THRESHOLD);
    }
    int handSize = strtol(argv[4], &end, 10);
    if (handSize < 1 || *end) {
        quit_game(INV_HAND);
    }

    printf("@");
    fflush(stdout);

    Game game = setup_game(numPlayers, playerID, threshold, handSize);
    play_game(&game);
    quit_game(NORMAL);
}

/* Find the index corresponding to the highest card in the players
 * hand that belongs to the specific suit
 *
 * @param game - main game struct
 * @param suit - suit to look for
 * @return index of highest card matching suit or -1 if no card with suit
 */
int find_highest_suit(Game* game, char suit) {
    int maxIndex = INVALID;
    int maxRank = MIN_RANK - 1;

    for (int i = 0; i < game->handSize; i++) {
        if (game->hand[i].rank != INVALID && game->hand[i].suit == suit) {
            if (game->hand[i].rank > maxRank) {
                maxIndex = i;
                maxRank = game->hand[i].rank;
            }
        }
    }

    return maxIndex;
}

/* Find the index corresponding to the lowest card in the players
 * hand that belongs to the specific suit
 *
 * @param game - main game struct
 * @param suit - suit to look for
 * @return index of lowest card matching suit or -1 if no card with suit
 */
int find_lowest_suit(Game* game, char suit) {
    int minIndex = INVALID;
    int minRank = MAX_RANK + 1;
    for (int i = 0; i < game->handSize; i++) {
        if (game->hand[i].rank != INVALID && game->hand[i].suit == suit) {
            if (game->hand[i].rank < minRank) {
                minIndex = i;
                minRank = game->hand[i].rank;
            }
        }
    }
    return minIndex;
}
