#include "profanity-filter.h"
#include <stdbool.h>
#include <string.h>

#define NUM_CURSE_WORDS 18
#define MAX_CURSE_WORD_LENGTH 64
const char curse_words[NUM_CURSE_WORDS][MAX_CURSE_WORD_LENGTH] = {
    "CUM*",
    "SEX*",
    "FAG*",
    "FUCK*",
    "SHIT*",
    "NIGG*",
    "DICK*",
    "PORN*",
    "COCK*",
    "SLUT*",
    "PUSSY*",
    "BITCH*",
    "DILDO*",
    "PENIS*",
    "VAGINA*",
    "ORGASM*",
    "BULLSH*",
    "MASTURBAT*",
};

typedef enum WordState {
    WORD_STILL_SCANNING = 0,
    WORD_NON_MATCHING = 1,
    WORD_MATCHED = 2
} WordState;

size_t get_filter_skip(const AprilToken *tokens, size_t curr_idx, size_t count) {
    WordState is_match[NUM_CURSE_WORDS] = { 0 };

    size_t num_to_skip = 0;
    size_t curr_character = 0;
    bool matched_badword = false;
    for(size_t i=curr_idx; i<count; i++){
        bool starts_w_space = (tokens[i].token[0] == ' ');
        if((i > curr_idx) && starts_w_space) {
            // Word boundary
            break;
        }

        num_to_skip++;

        if(matched_badword) continue;

        size_t token_len = strlen(tokens[i].token);
        for(size_t c=0; c<token_len; c++){
            if((c == 0) && (tokens[i].token[c] == ' ')) continue;

            for(size_t j=0; j<NUM_CURSE_WORDS; j++){
                const char *rule = curse_words[j];
                if((is_match[j] == WORD_STILL_SCANNING) && (rule[curr_character] != tokens[i].token[c])) {
                    is_match[j] = WORD_NON_MATCHING;
                }

                if((is_match[j] == WORD_STILL_SCANNING) && (rule[curr_character + 1] == '*')) {
                    is_match[j] = WORD_MATCHED;
                    matched_badword = true;
                    break;
                }
            }

            curr_character++;
        }
    }

    if(matched_badword) return num_to_skip;
    else return 0;
}