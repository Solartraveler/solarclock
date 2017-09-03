/* Matrix-Advancedclock
  (c) 2017 by Malte Marwedel
  www.marwedels.de/malte

  This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "main.h"

#include "debug.h"
#include "rs232.h"


void DbgPrintf_P(const char * string, ...) {
	char buffer[DEBUG_CHARS+1];
	va_list arglist;
	va_start(arglist, string);
	buffer[DEBUG_CHARS] = '\0';
	vsnprintf_P(buffer, DEBUG_CHARS, string, arglist);
	rs232_sendstring(buffer);
	va_end(arglist);
}
