#ifndef PLAYER_H
#define PLAYER_H

#define INVALID -1
#define NUM_SUITS 4
#define RANK_BASE 16
#define MIN_RANK 0x0 // lowest rank card
#define MAX_RANK 0xf // Highest rank card
#define NUM_ARGS 5

// Enum for all player exit statuses
enum ExitStatus {
    NORMAL = 0,
    USAGE = 1,
    INV_PLAYERS = 2,
    INV_POSITION = 3,
    INV_THRESHOLD = 4,
    INV_HAND = 5,
    INV_MESS = 6,
    END_OF_FILE = 7
};

// Stores a card
typedef struct {
    char suit;
    int rank; // -1 for invalid card
} Card;

// Categories of messages from hub
enum HubMessage {
    HAND,
    NEW_ROUND,
    PLAYED,
    GAME_OVER,
    INVALID_MESSAGE
};

// Stores all player information and game state
typedef struct {
    int numPlayers;
    int playerID;
    int threshold;
    int handSize;
    int turnsRemaining;
    Card* hand;
    int leadPlayer;
    Card* turn;
    int* playerPoints;
    int* dWon;
    int playerCount;
} Game;


/* Choose a card to play and return the index of its in the players hand
 * To be used by each player for their strategy
 *
 * @param game - main game struct
 * @return index of chosen card
 */
int choose_card(Game* game);

/* Find the index corresponding to the highest card in the players
 * hand that belongs to the specific suit
 *
 * @param game - main game struct
 * @param suit - suit to look for
 * @return index of highest card matching suit or -1 if no card with suit
 */
int find_highest_suit(Game* game, char suit);

/* Find the index corresponding to the lowest card in the players
 * hand that belongs to the specific suit
 *
 * @param game - main game struct
 * @param suit - suit to look for
 * @return index of lowest card matching suit or -1 if no card with suit
 */
int find_lowest_suit(Game* game, char suit);

#endif
