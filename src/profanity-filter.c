/* profanity-filter.c
 * Implements the function that performs profanity filtering
 *
 * Copyright 2022 abb128
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "profanity-filter.h"
#include <stdbool.h>
#include <string.h>

#define MAX_CURSE_WORD_LENGTH 64

#define NUM_CURSE_WORDS 22
const char curse_words[NUM_CURSE_WORDS][MAX_CURSE_WORD_LENGTH] = {
    "FAG*",
    "HOMO",
    "SLUT*",
    "NIGG*",
    "PUSSY*",
    "TRANN*",
    "\1",
    "CUM*",
    "SEX*",
    "FUCK*",
    "SHIT*",
    "DICK*",
    "PORN*",
    "COCK*",
    "BITCH*",
    "DILDO*",
    "PENIS*",
    "VAGINA*",
    "ORGASM*",
    "BULLSH*",
    "MOTHERFUC*",
    "MASTURBAT*",
};

typedef enum WordState {
    WORD_STILL_SCANNING = 0,
    WORD_NON_MATCHING = 1,
    WORD_MATCHED = 2
} WordState;

size_t get_filter_skip(const AprilToken *tokens, size_t curr_idx, size_t count, FilterMode mode) {
    if(mode <= FILTER_NONE) return 0;

    WordState is_match[NUM_CURSE_WORDS] = { 0 };

    size_t num_to_skip = 0;
    size_t curr_character = 0;
    bool matched_badword = false;
    bool still_scanning_any = true;
    for(size_t i=curr_idx; i<count; i++){
        if((i > curr_idx) && (tokens[i].flags & APRIL_TOKEN_FLAG_WORD_BOUNDARY_BIT)) {
            // Once we've arrived at the next word, stop looking.
            // we only want to filter the word starting at curr_idx
            break;
        }

        num_to_skip++;

        if(matched_badword) continue;

        still_scanning_any = false;
        size_t token_len = strlen(tokens[i].token);
        for(size_t c=0; c<token_len; c++){
            if(curr_character >= MAX_CURSE_WORD_LENGTH) break;

            if((c == 0) && (tokens[i].token[c] == ' ')) continue;

            for(size_t j=0; j<NUM_CURSE_WORDS; j++){
                const char *rule = curse_words[j];
                if((rule[0] == '\1') && mode < FILTER_PROFANITY) break;

                if(is_match[j] == WORD_STILL_SCANNING) {
                    still_scanning_any = true;

                    if(rule[curr_character] != tokens[i].token[c]) {
                        is_match[j] = WORD_NON_MATCHING;
                        continue;
                    }

                    if(rule[curr_character + 1] == '*') {
                        is_match[j] = WORD_MATCHED;
                        matched_badword = true;
                        break;
                    }else if(rule[curr_character + 1] == '\0') {
                        // if doesn't end in *, must be the end of word to match
                        if(((i + 1) >= count) || (tokens[i + 1].flags & APRIL_TOKEN_FLAG_WORD_BOUNDARY_BIT)) {
                            is_match[j] = WORD_MATCHED;
                            matched_badword = true;
                            break;
                        }else{
                            is_match[j] = WORD_NON_MATCHING;
                            continue;
                        }
                    }
                }
            }

            curr_character++;
        }

        if(!still_scanning_any) break;
    }


    if(matched_badword) return num_to_skip;
    else return 0;
}

