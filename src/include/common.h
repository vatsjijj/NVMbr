#ifndef nvmbr_common_h
#define nvmbr_common_h
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/*
  Enables the "nouveau" (0.0.2) style of NVM.
  Disable this for the old style (pre 0.0.2) of NVM,
  which is applicable in certain edge cases.
*/
#define NAN_TAGGING

/*
  Enables the printing of the code structure during
  compilation time.
  Mostly for developers.
*/
// #define DEBUG_PRINT_CODE

/*
  Enables the printing of the execution stack during
  compilation time.
  Again, mostly for developers.
*/
// #define DEBUG_TRACE_EXEC

/*
  Enables the stress testing of the garbage collector.
  Don't enable unless there are garbage collector
  changes, and be careful.
*/
// #define DEBUG_STRESS_GC

/*
  Enables the printing of the garbage collector's
  log, used to check if the garbage collector is
  functioning properly.
*/
// #define DEBUG_LOG_GC
#define UINT8_COUNT (UINT8_MAX + 1)
#endif
