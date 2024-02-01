#ifndef PCH_H
#define PCH_H

#include <iostream>
#include <vector>
#include <string>
#include <stack>
#include <map>
#include <algorithm>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#elif defined(_LINUX) // <- realy?
#include <X11.h>
#else
#error Unsupported platform!
#endif

#include <Bite.h>
using bite::stacklist;
using bite::stackptr;


#endif //PCH_H
