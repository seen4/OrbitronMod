#include <stdio.h>
#include "OrbitronMod.h"
#include <stdbool.h>
#include <stdint.h>
#include <memory.h>
#include <stdlib.h>
#include "omm.h"
#include"internals.h"
#include "hook_functions.h"

#pragma warning(disable:6387)


void HookAssignFile() {
	//劫持TLE文件读取函数
	uint8_t instruction[5];
	instruction[0] = 0xE8;//E8 relative call
	size_t AssignFile = (size_t)0x462718;
	*(long*)&instruction[1] = (BYTE*)CreateTLEFileHandle_wrap - (BYTE*)AssignFile - 5;//计算相对地址,拼接成完整call指令
	VirtualProtect((void*)AssignFile, 6, PAGE_EXECUTE_READWRITE, NULL);
	WriteProcessMemory(GetCurrentProcess(), (void*)AssignFile, (void*)instruction, 5, NULL);

	//将后面的代码置nop,防止代码尝试读取滚木句柄崩程序
	uint8_t* nops[20];
	memset(nops,0x90,20);
	VirtualProtect((void*)(AssignFile + 5), 24, PAGE_EXECUTE_READWRITE, NULL);
	WriteProcessMemory(GetCurrentProcess(), (void*)(AssignFile + 5), (void*)nops, 20, NULL);

	return;
}

void HookCloseTLEFile() {
	//劫持TLE文件关闭函数
	uint8_t instruction[5];
	instruction[0] = 0xE8;//E8 relative call
	size_t AssignFile = (size_t)0x46276D;
	*(long*)&instruction[1] = (BYTE*)CloseTLEFile_wrap - (BYTE*)AssignFile - 5;//计算相对地址,拼接成完整call指令
	VirtualProtect((void*)AssignFile, 6, PAGE_EXECUTE_READWRITE, NULL);
	WriteProcessMemory(GetCurrentProcess(), (void*)AssignFile, (void*)instruction, 5, NULL);
}

void HookGetEpoch() {
	uint8_t instruction[11] = { 0x89,0xF8,0x59,0x90,0x90,0x90,0xE8 };//注意平栈!!!
	size_t ConvertEpoch = 0x004682EB;
	*(long*)&instruction[7] = (BYTE*)Epoch2JulianDate_wrap - (BYTE*)ConvertEpoch - 6 - 5 ;//计算相对地址,拼接成完整call指令
	VirtualProtect((void*)ConvertEpoch, 12, PAGE_EXECUTE_READWRITE, NULL);
	WriteProcessMemory(GetCurrentProcess(), (void*)ConvertEpoch, (void*)instruction, 11, NULL);
}

void Disable2055Limit() {
	uint8_t jmp=0xEB;
	VirtualProtect((void*)0x004DCC25, 2, PAGE_EXECUTE_READWRITE, NULL);
	WriteProcessMemory(GetCurrentProcess(), (void*)0x004DCC25, (void*)&jmp, 1, NULL);
}

//设置钩子
void SetHook()
{
	if (MH_Initialize() == MB_OK)
	{
		MH_CreateHook((void*)0x0046166C, &GetNORAD_wrap, reinterpret_cast<void**>(&GetNORADID_ORG));
		MH_EnableHook((void*)0x0046166C);
		HookAssignFile();
		MH_CreateHook((void*)0x00462408, &ReadTLEData_wrap, reinterpret_cast<void**>(&GetNORADID_ORG));
		MH_EnableHook((void*)0x00462408);
		HookCloseTLEFile();
		MH_CreateHook((void*)0x004616C0, &GetCOSPAR_wrap, reinterpret_cast<void**>(&GetNORADID_ORG));
		MH_EnableHook((void*)0x004616C0);
		MH_CreateHook((void*)0x0046180C, &Epoch2JulianDate_wrap, reinterpret_cast<void**>(&GetNORADID_ORG));
		MH_EnableHook((void*)0x0046180C);
		HookGetEpoch();
		//MH_CreateHook((void*)0x0045A568, &JD2GE_wrap, reinterpret_cast<void**>(&GetNORADID_ORG));
		//MH_EnableHook((void*)0x0045A568);
		Disable2055Limit();
	}
}

// 卸载钩子
void UnHook()
{
		MH_Uninitialize();
}

BOOL APIENTRY DllMain(HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
	switch (ul_reason_for_call) {
	case DLL_PROCESS_ATTACH:
		SetHook();
		// 当DLL被加载时执行
		break;
	case DLL_PROCESS_DETACH:
		UnHook();
		sat_destroy(SatList);
		// 当DLL被卸载或进程终止时执行
		break;
	}
	return TRUE;
}