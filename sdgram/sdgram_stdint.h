//
// Find the right stdint.h definitions.
//
// author: aleksandar
//

#pragma once

#if defined(ARDUINO) && ARDUINO >= 100
  #include "Arduino.h"
#elif defined(ARDUINO)
  #include "WProgram.h"
#else
  #include <cstdint>
#endif
