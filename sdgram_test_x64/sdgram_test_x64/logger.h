//
// Overriding original Arduino logger.
//
// author: aleksandar
//

#pragma once

#include <iostream>

#define LOG_DEFINED

#define LogVerbose(arg) do { std::cout << arg; } while(0)
#define LogVerboseLn(arg) do { std::cout << arg << std::endl; } while(0)

#ifdef TRACE_ALL
#define LogTrace(arg) do { std::cout << arg; } while(0)
#define LogTraceLn(arg) do { std::cout << arg << std::endl; } while(0)
#else
#define LogTrace(...)
#define LogTraceLn(...)
#endif /* TRACE_ALL */
