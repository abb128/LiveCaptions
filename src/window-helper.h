/* window-helper.h
 * Declares a helper that tries to set GtkWindow to keep above other windows
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

#include <adwaita.h>

void override_keep_above_system(bool override);

bool is_keep_above_supported(GtkWindow *window);

// Returns false if impossible to set keep above
bool set_window_keep_above(GtkWindow *window, bool keep_above);
