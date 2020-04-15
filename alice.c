#include "player.h"
#include <stdio.h>

/* Choose a card to play and return the index of its in the players hand
 * To be used by each player for their strategy
 *
 * @param game - main game struct
 * @return index of chosen card
 */
int choose_card(Game* game) {

    if (game->playerID == game->leadPlayer) {
        char suitPreference[] = {'S', 'C', 'D', 'H'};
        for (int i = 0; i < NUM_SUITS; i++) {
            // find highest suit
            int cardIndex = find_highest_suit(game, suitPreference[i]);
            if (cardIndex != INVALID) {
                return cardIndex;
            }
        }
    }

    // play lowest card in lead suit
    int cardIndex = find_lowest_suit(game, game->turn[game->leadPlayer].suit);
    if (cardIndex != -1) {
        return cardIndex;
    }

    // remaining choices

    char suitPreference[] = {'D', 'H', 'S', 'C'};
    for (int i = 0; i < NUM_SUITS; i++) {
        // find highest suit
        int cardIndex = find_highest_suit(game, suitPreference[i]);
        if (cardIndex != INVALID) {
            return cardIndex;
        }
    }
    return INVALID; // Should never get here
}
