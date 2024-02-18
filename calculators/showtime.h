//
// This file is part of Alpertron Calculators.
//
// Copyright 2017-2021 Dario Alejandro Alpern
//
// Alpertron Calculators is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Alpertron Calculators is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Alpertron Calculators.  If not, see <http://www.gnu.org/licenses/>.
//
#ifndef _SHOWTIME_H
#define _SHOWTIME_H
#if defined(__EMSCRIPTEN__) || defined(__ANDROID__)
double tenths(void);
extern double originalTenthSecond;
extern int oldTimeElapsed;
void GetDHMS(char **pptrText, int seconds);
void GetDHMSt(char **pptrText, int tenths);
#endif
void showElapsedTime(char **pptrOutput);
void showElapsedTimeSec(char **pptrOutput);
#endif