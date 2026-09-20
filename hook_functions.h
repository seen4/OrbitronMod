#pragma once
#include "internals.h"
#include "strops.h"
#include "omm2tle.h"
#include "data.h"
#include "date.h"

const char OpenDialogFilterName[] = "\x17\0\0\0OMM files (*.TXT,*.CSV)";//字符串长度,4字节补齐
const char OpenDialogFilterType[] = "\x25\0\0\0|*.tle;*.txt;*.mtl;*.sat;*.csv;*.omm|";

typedef int(__fastcall* GetNORAD_ORG) (const char* TLE_DATA, char* NORAD_ID);

GetNORAD_ORG GetNORADID_ORG = nullptr;

sat_data SatList;

//获取卫星数据
omm_t get_satellite_data(const char* TLE_DATA) {
	//获取ID
	char id[6];
	memcpy_s(id, sizeof(id), TLE_DATA + 3, 5);
	id[5] = '\0';
	size_t sat_id = atoi(id);

	omm_t o;
	sat_access(SatList, &o, sat_id);
	return o;
}

int GetNORAD(const char *TLE_DATA, char *NORAD_ID) {
	omm_t o=get_satellite_data(TLE_DATA);

	//获取NORAD ID
	char string[10];
	_itoa_s(o.norad_id, string, 10, 10);
	LstrFromArray(NORAD_ID, string, 9);
	return 0;
}

__declspec(naked) int __fastcall GetNORAD_wrap(const char* TLE_DATA, char* NORAD_ID) {
	__asm {
		mov ecx, eax
		jmp GetNORAD
	}
}

int get_norad_for_sorting(const char* TLE_DATA, char* NORAD_ID) {
	omm_t o = get_satellite_data(TLE_DATA);

	//获取NORAD ID
	char string[10];
	sprintf_s(string,sizeof(string),"%09d", o.norad_id);
	LstrFromArray(NORAD_ID, string, 9);
	return 0;
}

__declspec(naked) int __fastcall get_norad_for_sorting_wrap(const char* TLE_DATA, char* NORAD_ID) {
	__asm {
		mov ecx, eax
		jmp get_norad_for_sorting
	}
}


typedef int (__fastcall* GetCOSPAR_ORG)(const char* TLE_DATA, char* COSPAR_ID);
int GetCOSPAR(const char* TLE_DATA, char* COSPAR_ID) {
	omm_t o = get_satellite_data(TLE_DATA);

	//获取COSPAR ID
	LstrFromArray(COSPAR_ID, o.object_id, 32);
	return 0;
}

__declspec(naked) int __fastcall GetCOSPAR_wrap(const char* TLE_DATA, char* COSPAR_ID) {
	__asm {
		mov ecx, eax
		jmp GetCOSPAR
	}
}

double Epoch2JulianDate(const char* TLE_DATA) {
	omm_t o = get_satellite_data(TLE_DATA);

	return o.epoch_jd;
}

__declspec(naked) double __fastcall Epoch2JulianDate_wrap(const char* TLE_DATA) {
	__asm {
		mov ecx, eax
		jmp Epoch2JulianDate
	}
}

void JD2GE(uint16_t *date, double jd) {
	JulianDate2Gregorian(jd,&date[0],&date[1],&date[2],&date[3],&date[4],&date[5]);
	return;
}

__declspec(naked) void __fastcall JD2GE_wrap(uint16_t, double) {
	__asm {
		mov ecx,eax
		jmp JD2GE
	}
}

//这个函数挂钩在TLE总读取函数的入口处
//把Satdata清理和TLE读取函数分开
//来解决同时加载多个文件的问题
typedef void (__fastcall* SatData_cleanup_ORG)();
SatData_cleanup_ORG Satdata_cleanup_ORG = nullptr;
__declspec(noreturn) void SatData_cleanup() {
	if (SatList.data) sat_destroy(&SatList);
	SatList = sat_create();
	__asm {
		mov esp,ebp				; manually recover stack frame
		pop ebp					; ugly as fuck, but necessary
		jmp Satdata_cleanup_ORG	; if u dont want to fuck up the stack frame
	}
}

bool CreateTLEFileHandle(FILE** fd, const char* FilePath) {

	printf("Loading: %s\n",FilePath);
	if(fopen_s(fd, FilePath, "r")) {
		MessageBoxA(NULL, "Failed Opening OMM!", "Error", MB_OK | MB_ICONERROR);
		return FALSE;
	}
	SetTLEReadState(1);

	omm_set_options(0, 0, 0, 0);

	return TRUE;
}

__declspec(naked) bool __fastcall CreateTLEFileHandle_wrap(FILE*, const char*) {
	__asm {
		mov ecx, eax
		call CreateTLEFileHandle
		mov ebx,eax
		ret
	}
}

typedef int(__fastcall* ReadTLEData_ORG) (char*, char*, char* );

//WARNING:All strings must be in pascal style( size+data, without \0 truncate )
//SatelliteName is delphi LStr
uint8_t ReadTLEData(char* SatelliteName, char* Moclzan, char* TLEDataBuffer) {
	
	FILE **fd=GetTLEfd();
	char SatName[256];
	char line1[75];
	char line2[75];

	int rc;
	if( (rc=omm_next_tle(*fd, SatName, line1, line2),rc) == 0){
		return -1;
	}
	if (rc < 0) {
		MessageBoxA(NULL, "Failed Parsing OMM!", "Error", MB_OK|MB_ICONERROR);
		return 0;
	}

	omm_t d;
	omm_get_object_data(&d);
	d.epoch_jd=ParseEpoch(d.epoch);
	size_t id=sat_push(&SatList,d);

	char tle_id[6];
	sprintf_s(tle_id,6,"%05d",id);
	memcpy_s(line1+2,71,tle_id,5);


	trimString(SatName);
	LstrFromArray(SatelliteName,SatName,strlen(SatName));

	trimString(line1);
	CStrToPascalString(line1);
	memcpy(TLEDataBuffer,line1,70);

	trimString(line2);
	CStrToPascalString(line2);
	memcpy(TLEDataBuffer+70, line2, 70);

	return TRUE;
}

__declspec(naked) uint8_t __fastcall ReadTLEData_wrap(char*, char*, char*) {
	__asm {
		push ecx
		mov ecx, eax
		call ReadTLEData
		ret
	}
}

void CloseTLEFile(FILE **fd) {
	omm_reset();
	fclose(*fd);
}

__declspec(naked) void __fastcall CloseTLEFile_wrap(FILE **fd){
	__asm {
		mov ecx,eax
		jmp CloseTLEFile
	}
}