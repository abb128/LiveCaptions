/* profanity-filter.h
 * Declares the function that performs profanity filtering
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

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <april_api.h>

typedef enum FilterMode {
    FILTER_NONE = 0,
    FILTER_SLURS = 1,
    FILTER_PROFANITY = 2
} FilterMode;

// Takes in an array of tokens, the current index in the list of tokens, and
// token count. The current index should be at a word boundary.
// Returns the number of tokens to skip after the current index if filtered,
// or 0 if not filtered.
size_t get_filter_skip(const AprilToken *tokens,
                       size_t curr_idx,
                       size_t count,
                       FilterMode mode);
