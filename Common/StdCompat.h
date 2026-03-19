/*
 *   Copyright (C) 2024 by the DStarRepeater contributors
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * Compatibility header that provides the standard C++17 includes used
 * throughout the codebase.  This replaces the former wx/wx.h dependency so
 * that non-GUI translation units compile without wxWidgets.  Include this
 * instead of individual standard headers wherever the full set is needed.
 */

#ifndef StdCompat_H
#define StdCompat_H

#include <string>
#include <vector>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <mutex>
#include <thread>
#include <chrono>
#include <algorithm>
#include <unordered_map>

#endif
