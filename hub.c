#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <signal.h>
#include <stdbool.h>
#include "util.h"
#include <string.h>

#define INVALID -1
#define MIN_RANK 0
#define MAX_RANK 15
#define ARG_SIZE 12 // fits any integer

// Enum for all hub exit statuses
enum ExitStatus {
    NORMAL = 0,
    USAGE = 1,
    INV_THRESHOLD = 2,
    DECK_ERROR = 3,
    INSUFF_CARDS = 4,
    PLAYER_ERROR = 5,
    PLAYER_EOF = 6,
    INV_MESSAGE = 7,
    INV_CARD_CHOICE = 8,
    SIGNAL_RECEIVED = 9
};

// Stores a card
typedef struct {
    char suit;
    int rank; // -1 for invalid card
} Card;

// Properties associated with each player
typedef struct {
    int points;
    int dWon;
    FILE* read;
    FILE* write;
    Card* hand;
    pid_t pid;
} Player;

// Main game state, stores all players
typedef struct {
    int numPlayers;
    Player* players;
    int deckSize;
    Card* deck;
    int threshold;
    int leadPlayer;
    int handSize;
    Card* round;
} Game;

// Global variable for handling sighup
Game* data;

/* quit the game after printing the correct error message
 *
 * @param status - the exit status to use
 */
void quit_game(enum ExitStatus status) {
    if (status == USAGE) {
        fprintf(stderr, "Usage: 2310hub deck threshold player0 {player1}\n");
    } else if (status == INV_THRESHOLD) {
        fprintf(stderr, "Invalid threshold\n");
    } else if (status == DECK_ERROR) {
        fprintf(stderr, "Deck error\n");
    } else if (status == INSUFF_CARDS) {
        fprintf(stderr, "Not enough cards\n");
    } else if (status == PLAYER_ERROR) {
        fprintf(stderr, "Player error\n");
    } else if (status == PLAYER_EOF) {
        fprintf(stderr, "Player EOF\n");
    } else if (status == INV_MESSAGE) {
        fprintf(stderr, "Invalid message\n");
    } else if (status == INV_CARD_CHOICE) {
        fprintf(stderr, "Invalid card choice\n");
    } else if (status == SIGNAL_RECEIVED) {
        fprintf(stderr, "Exit due to signal\n");
    }
    exit(status);
}

/* Handle SIGPIPE (to avoid errors on writing) by ignoring it
 *
 * @param signum - number of signal received
 */
static void sigpipe_handler(int signum) {
    return;
}

/* Hangle SIGHUP by killing players and exiting
 *
 * @param signum - number of signal received
 */
static void sighup_handler(int signum) {
    for (int i = 0; i < data->numPlayers; i++) {
        kill(data->players[i].pid, 9); // 9 is for SIGKILL
    }
    quit_game(SIGNAL_RECEIVED);
}

/* Read a deckfile and create an array of cards representing its contents
 *
 * @param filename - name of deck file
 * @param deckSize - pointer to store number of cards in deckfile into
 * @return array of cards of length deckSize or NULL if file was erroneous
 */
Card* read_deck_file(char* filename, int* deckSize) {
    FILE* deckFile = fopen(filename, "r");
    if (deckFile == NULL) {
        return NULL;
    }
    char* line;
    int length = read_line(deckFile, &line);
    char* end;
    int numCards = strtol(line, &end, 10);
    free(line);
    if (numCards <= 0 || *end) {
        return NULL;
    }

    Card* deck = malloc(sizeof(Card) * numCards);
    for (int i = 0; i < numCards; i++) {
        length = read_line(deckFile, &line);
        if (length != 2) {
            free(line);
            return NULL;
        }
        char suit = line[0];
        if (suit != 'D' && suit != 'C' && suit != 'H' && suit != 'S') {
            free(line);
            return NULL;
        }
        deck[i].suit = suit;
        int rank = strtol(line + 1, &end, 16);


        if (rank < MIN_RANK || rank > MAX_RANK || *end) {
            free(line);
            return NULL;
        }
        // Check rank is lower case
        char checker[2]; // needed to store single char rank + null terminator
        sprintf(checker, "%x", rank);
        if (strcmp(checker, line + 1) != 0) {
            free(line);
            return NULL;
        }
        deck[i].rank = rank;
        free(line);
    }
    *deckSize = numCards;
    return deck;
}

/* Set up a new game struct based on command line argument values
 *
 * @param threshold - threshold of diamond cards
 * @param deckSize - number of cards in deck
 * @param deck - array of cards from deckfile
 * @param numPlayers - number of players in game
 * @return main game struct
 */
Game setup_game(int threshold, int deckSize, Card* deck, int numPlayers) {
    Game game;

    game.numPlayers = numPlayers;

    game.players = malloc(sizeof(Player) * numPlayers);
    for (int i = 0; i < numPlayers; i++) {
        game.players[i].points = 0;
        game.players[i].dWon = 0;
        game.players[i].hand = malloc(sizeof(Card) * (deckSize / numPlayers));
    }

    game.deckSize = deckSize;
    game.deck = deck;
    game.threshold = threshold;
    game.leadPlayer = 0;
    game.handSize = game.deckSize / game.numPlayers;
    game.round = malloc(sizeof(Card) * numPlayers);

    return game;
}

/* Start all players in the game
 *
 * Exits game with PLAYER_ERROR if unable to start any player
 *
 * @param game - main game struct
 * @param playerExecutables - list of player executables to run, from argv
 */
void start_players(Game* game, char** playerExecutables) {
    int devNull = open("/dev/null", O_WRONLY);
    char numPlayersArg[ARG_SIZE], thresholdArg[ARG_SIZE], handArg[ARG_SIZE];
    sprintf(numPlayersArg, "%d", game->numPlayers);
    sprintf(thresholdArg, "%d", game->threshold);
    sprintf(handArg, "%d", game->deckSize / game->numPlayers);

    for (int i = 0; i < game->numPlayers; i++) {
        int hubToPlayer[2], playerToHub[2];
        if (pipe(hubToPlayer) || pipe(playerToHub)) {
            quit_game(PLAYER_ERROR);
        }

        pid_t pid = fork();
        if (pid == -1) {
            quit_game(PLAYER_ERROR);
        } else if (pid == 0) { // player process
            close(playerToHub[0]);
            close(hubToPlayer[1]);

            for (int j = 0; j < i; j++) {
                fclose(game->players[j].read);
                fclose(game->players[j].write);
            }

            dup2(hubToPlayer[0], 0);
            dup2(playerToHub[1], 1);
            dup2(devNull, 2);

            char playerIDArg[ARG_SIZE];
            sprintf(playerIDArg, "%d", i);

            execlp(playerExecutables[i], playerExecutables[i],
                    numPlayersArg, playerIDArg,
                    thresholdArg, handArg, (char*) 0);
            exit(0); // Shutdown if exec failed
        } else { // parent process
            game->players[i].pid = pid;
            game->players[i].read = fdopen(playerToHub[0], "r");
            game->players[i].write = fdopen(hubToPlayer[1], "a");
            close(playerToHub[1]);
            close(hubToPlayer[0]);
        }

        if (fgetc(game->players[i].read) != '@') {
            quit_game(PLAYER_ERROR);
        }
    }
}

/* Get a PLAY message from the current player
 *
 * Quits game if player is at EOF, or player message is invalid
 *       or player chooses an invalid card
 *
 * @param game - main game struct
 * @param player - player index to receive from
 * @return index of card the player chose
 */
int get_play(Game* game, int player) {
    char* message;
    read_line(game->players[player].read, &message);
    if (feof(game->players[player].read)) {
        quit_game(PLAYER_EOF);
    }
    char suit;
    unsigned int rank;
    char end;
    if (sscanf(message, "PLAY%c%x%c", &suit, &rank, &end) != 2 || 
            rank < MIN_RANK || rank > MAX_RANK ||
            (suit != 'D' && suit != 'C' && suit != 'H' && suit != 'S')) {
        quit_game(INV_MESSAGE);
    }

    char leadSuit = game->round[0].suit;
    bool hasLead = false;
    int cardIndex = INVALID;
    for (int i = 0; i < game->handSize; i++) {
        if (game->players[player].hand[i].suit == leadSuit) {
            hasLead = true;
        }
        if (game->players[player].hand[i].suit == suit &&
                game->players[player].hand[i].rank == rank) {
            // found correct card in hand
            cardIndex = i;
        }
    }
    if (cardIndex != INVALID) {
        if (player == game->leadPlayer) {
            return cardIndex;
        } else if (game->players[player].hand[cardIndex].suit == leadSuit) {
            // playing lead suit
            return cardIndex;
        } else if (hasLead == false) {
            // don't have lead suit
            return cardIndex;
        }
    }
    // don't have card
    quit_game(INV_CARD_CHOICE);
    return INVALID; // will never get here
}

/* Determine which player won a round
 *
 * @param game - main game struct
 * @return index of player who won the round
 */
int find_winner(Game* game) {
    char leadSuit = game->round[0].suit;
    int maxRank = game->round[0].rank;
    int winner = 0;
    for (int i = 1; i < game->numPlayers; i++) {
        if (game->round[i].suit == leadSuit) {
            if (game->round[i].rank > maxRank) {
                winner = i;
                maxRank = game->round[i].rank;
            }
        }
    }
    return (winner + game->leadPlayer) % game->numPlayers;
}

/* Play a complete hand
 *
 * @param game - main game struct
 */
void play_hand(Game* game) {
    // new round message
    for (int i = 0; i < game->numPlayers; i++) {
        fprintf(game->players[i].write, "NEWROUND%d\n", game->leadPlayer);
        fflush(game->players[i].write);
    }

    printf("Lead player=%d\n", game->leadPlayer);

    // 
    for (int i = 0; i < game->numPlayers; i++) {
        int currentPlayer = (game->leadPlayer + i) % game->numPlayers;
        int cardIndex = get_play(game, currentPlayer);
        // store card
        game->round[i] = game->players[currentPlayer].hand[cardIndex];
        // send info to other players
        for (int j = 0; j < game->numPlayers; j++) {
            if (j != currentPlayer) {
                fprintf(game->players[j].write, "PLAYED%d,%c%x\n",
                        currentPlayer, game->round[i].suit,
                        game->round[i].rank);
                fflush(game->players[j].write);
            }
        }
        // remove card from hand
        game->players[currentPlayer].hand[cardIndex].suit = 'z';
        game->players[currentPlayer].hand[cardIndex].rank = INVALID;
    }
    printf("Cards=");
    for (int i = 0; i < game->numPlayers; i++) {
        printf("%c.%x", game->round[i].suit, game->round[i].rank);
        if (i == game->numPlayers - 1) {
            printf("\n");
        } else {
            printf(" ");
        }
    }

    int winner = find_winner(game);
    game->players[winner].points++;
    game->leadPlayer = winner;
    for (int i = 0; i < game->numPlayers; i++) {
        if (game->round[i].suit == 'D') {
            game->players[winner].dWon++;
        }
    }
}

/* Play entire game
 *
 * @param game - main game struct
 */
void play_game(Game* game) {
    // send each player their hand
    int handSize = game->deckSize / game->numPlayers;
    for (int i = 0; i < game->numPlayers; i++) {
        fprintf(game->players[i].write, "HAND%d", handSize);
        for (int j = 0; j < handSize; j++) {
            Card card = game->deck[i * handSize + j];
            fprintf(game->players[i].write, ",%c%x", card.suit, card.rank);
            game->players[i].hand[j] = card;
        }
        fprintf(game->players[i].write, "\n");
        fflush(game->players[i].write);
    }

    // play each hand
    for (int i = 0; i < handSize; i++) {
        play_hand(game);
    }

    // game over
    for (int i = 0; i < game->numPlayers; i++) {
        fprintf(game->players[i].write, "GAMEOVER\n");
        fflush(game->players[i].write);
    }

    // print final scores
    for (int i = 0; i < game->numPlayers; i++) {
        int score;
        if (game->players[i].dWon < game->threshold) {
            score = game->players[i].points - game->players[i].dWon;
        } else {
            score = game->players[i].points + game->players[i].dWon;
        }
        printf("%d:%d", i, score);
        if (i == game->numPlayers - 1) {
            printf("\n");
        } else {
            printf(" ");
        }
    }
}

int main(int argc, char** argv) {

    if (argc < 4) {
        quit_game(USAGE);
    }

    char* end;
    int threshold = strtol(argv[2], &end, 10);
    if (threshold < 2 || *end) {
        quit_game(INV_THRESHOLD);
    }

    int deckSize;
    Card* deck = read_deck_file(argv[1], &deckSize);
    if (deck == NULL) {
        quit_game(DECK_ERROR);
    }

    int numPlayers = argc - 3;
    if (deckSize < numPlayers) {
        quit_game(INSUFF_CARDS);
    }

    Game game = setup_game(threshold, deckSize, deck, numPlayers);
    data = &game;
    
    // Set up signal handlers
    struct sigaction saPipe;
    saPipe.sa_handler = sigpipe_handler;
    saPipe.sa_flags = SA_RESTART;
    sigaction(SIGPIPE, &saPipe, NULL);

    struct sigaction saHup;
    saHup.sa_handler = sighup_handler;
    saHup.sa_flags = SA_RESTART;
    sigaction(SIGHUP, &saHup, NULL);
    
    start_players(&game, argv + 3);

    play_game(&game);

    quit_game(NORMAL);
}
