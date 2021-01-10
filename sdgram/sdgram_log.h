//
// Logging to serial.
//
// author: aleksandar
//

#pragma once

#ifndef LOG_DEFINED

#define LOG_DEFINED

#define LogVerbose(...) Serial.print(__VA_ARGS__)
#define LogVerboseLn(...) Serial.println(__VA_ARGS__)

#ifdef TRACE_ALL
#define LogTrace(...) Serial.print(__VA_ARGS__)
#define LogTraceLn(...) Serial.println(__VA_ARGS__)
#else
#define LogTrace(...)
#define LogTraceLn(...)
#endif /* TRACE_ALL */

#endif /* LOG_DEFINED */
