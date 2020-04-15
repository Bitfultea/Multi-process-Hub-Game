#include <stdbool.h>

#include "player.h"

/* Check if any player has reached treshold - 2 D cards
 *
 * @param game - main game struct
 * @return true iff at least one player has reached threshold - 2 D cards
 */
bool threshold_reached(Game* game) {
    for (int i = 0; i < game->numPlayers; i++) {
        if (game->dWon[i] >= game->threshold - 2) {
            return true;
        }
    }
    return false;
}

/* Check if any player has played a D card this round
 *
 * @param game - main game struct
 * @return true iff at least one player has played a D card this round
 */
bool played_d(Game* game) {
    for (int i = 0; i < game->playerCount; i++) {
        if (game->turn[(i + game->leadPlayer)
                % game->numPlayers].suit == 'D') {
            return true;
        }
    }
    return false;
}

/* Choose a card to play and return the index of its in the players hand
 * To be used by each player for their strategy
 *
 * @param game - main game struct
 * @return index of chosen card
 */
int choose_card(Game* game) {
    if (game->playerID == game->leadPlayer) {
        char suitPreference[] = {'D', 'H', 'S', 'C'};
        for (int i = 0; i < NUM_SUITS; i++) {
            // find lowest card in suit
            int cardIndex = find_lowest_suit(game, suitPreference[i]);
            if (cardIndex != -1) {
                return cardIndex;
            }
        }
    }

    if (threshold_reached(game) && played_d(game)) {
        // play highest card in lead suit
        int cardIndex = 
                find_highest_suit(game, game->turn[game->leadPlayer].suit);
        if (cardIndex != INVALID) {
            return cardIndex;
        }
        char suitPreference[] = {'S', 'C', 'H', 'D'};
        for (int i = 0; i < NUM_SUITS; i++) { 
            // find highest card in suit
            int cardIndex = find_lowest_suit(game, suitPreference[i]);
            if (cardIndex != INVALID) {
                return cardIndex;
            }
        }
    } else {
        // play lowest card in lead suit
        int cardIndex =
                find_lowest_suit(game, game->turn[game->leadPlayer].suit);
        if (cardIndex != INVALID) {
            return cardIndex;
        }
        char suitPreference[] = {'S', 'C', 'D', 'H'};
        for (int i = 0; i < NUM_SUITS; i++) {
            // find highest card in suit
            int cardIndex = find_highest_suit(game, suitPreference[i]);
            if (cardIndex != INVALID) {
                return cardIndex;
            }
        }
    }
    return -1; // Should never reach here
}
