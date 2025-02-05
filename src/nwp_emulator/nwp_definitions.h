#pragma once


#ifdef WITH_NWP_EMULATOR_SINGLE_PRECISION
#define FIELD_TYPE_REAL float
#else
#define FIELD_TYPE_REAL double
#endif