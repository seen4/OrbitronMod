#pragma once

#ifdef __cplusplus
extern "C" {
#endif

	extern double ParseEpoch(const char* epoch);

	extern double Year2JulianDate(int Year);

	void JulianDate2Gregorian(double jd, uint16_t* year, uint16_t* month, uint16_t* day, uint16_t* hour, uint16_t* minute, uint16_t* second);

#ifdef __cplusplus
}
#endif

