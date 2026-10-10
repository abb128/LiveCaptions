/* common.h
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

#define SWEAR_REPLACEMENT " [__]"

#include "livecaptions-config.h"
#define LIVECAPTIONS_VERSION PACKAGE_VERSION

// Recognition runs without time compression (ASYNC_NO_RT), so anything below
// 1.0x real time accumulates a backlog until audio is dropped. Require
// headroom for the load of the rest of the desktop.
#define MINIMUM_BENCHMARK_RESULT (1.2)
// Below this, captions keep up only with little headroom
#define GOOD_BENCHMARK_RESULT (1.5)
#define GET_MODEL_PATH() (getenv("APRIL_MODEL_PATH") == NULL) ? "/app/LiveCaptions/models/aprilv0_en-us.april" : getenv("APRIL_MODEL_PATH")
