/******************************************************************************
Copyright (C) 2015 by jackun <jack.un@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#pragma once
#include "resource.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <emmintrin.h> 
#include <stdio.h>
#include <algorithm>
#include <string>
#include <iostream>
#include <sstream>
#include <vector>
#include <map>
#include <chrono>
#include <thread>
#include <vfw.h>
#include <assert.h>

inline bool ends_with(std::string const & value, std::string const & ending)
{
	if (ending.size() > value.size()) return false;
	return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

#include "core\Factory.h"
#include "components\VideoEncoderVCE.h"
#include "components\VideoConverter.h"
