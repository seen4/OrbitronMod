#include <stdio.h>
#include "OrbitronMod.h"
#include <stdbool.h>
#include <stdint.h>
#include <memory.h>
#include <stdlib.h>
#include "omm.h"
#include"internals.h"
#include "hook_functions.h"
#include "web.h"

#pragma warning(disable:6387)


//劫持TLE文件读取函数
void HookAssignFile() {
	uint8_t opcode[5];
	opcode[0] = 0xE8;//E8 relative call
	size_t AssignFile = (size_t)0x462718;
	*(long*)&opcode[1] = (BYTE*)CreateTLEFileHandle_wrap - (BYTE*)AssignFile - 5;//计算相对地址,拼接成完整call指令
	VirtualProtect((void*)AssignFile, 5, PAGE_EXECUTE_READWRITE, NULL);
	WriteProcessMemory(GetCurrentProcess(), (void*)AssignFile, (void*)opcode, 5, NULL);

	//将后面的代码置nop,防止代码尝试读取滚木句柄崩程序
	uint8_t* nops[22];
	memset(nops,0x90,22);
	VirtualProtect((void*)(AssignFile + 5), 22, PAGE_EXECUTE_READWRITE, NULL);
	WriteProcessMemory(GetCurrentProcess(), (void*)(AssignFile + 5), (void*)nops, 22, NULL);

	return;
}

//劫持TLE文件关闭函数
void HookCloseTLEFile() {
	uint8_t opcode[5];
	opcode[0] = 0xE8;//E8 relative call
	size_t AssignFile = (size_t)0x46276D;
	*(long*)&opcode[1] = (BYTE*)CloseTLEFile_wrap - (BYTE*)AssignFile - 5;//计算相对地址,拼接成完整call指令
	VirtualProtect((void*)AssignFile, 5, PAGE_EXECUTE_READWRITE, NULL);
	WriteProcessMemory(GetCurrentProcess(), (void*)AssignFile, (void*)opcode, 5, NULL);
}

void HookGetEpoch() {
	uint8_t opcode[11] = { 0x89,0xF8,0x59,0x90,0x90,0x90,0xE8 };//注意平栈!!!
	size_t ConvertEpoch = 0x004682EB;
	*(long*)&opcode[7] = (BYTE*)Epoch2JulianDate_wrap - (BYTE*)ConvertEpoch - 6 - 5 ;//计算相对地址,拼接成完整call指令
	VirtualProtect((void*)ConvertEpoch, 12, PAGE_EXECUTE_READWRITE, NULL);
	WriteProcessMemory(GetCurrentProcess(), (void*)ConvertEpoch, (void*)opcode, 11, NULL);
}

void Disable2055Limit() {
	uint8_t jmp=0xEB;
	VirtualProtect((void*)0x004DCC25, 1, PAGE_EXECUTE_READWRITE, NULL);
	WriteProcessMemory(GetCurrentProcess(), (void*)0x004DCC25, (void*)&jmp, 1, NULL);
}

//默认的打开文件对话框不会显示.csv后缀的文件
//这里给筛选器加入了.csv后缀
void HookOpenDialog() {
	uint8_t opcode[11];
	opcode[0]=0x68,opcode[5]=0x90,opcode[6]=0x68;
	uintptr_t HookAddr = 0x004E4B33;
	*(long*)&opcode[1]=(long)&OpenDialogFilterName+4;
	*(long*)&opcode[7] = (long)&OpenDialogFilterType+4;
	VirtualProtect((void*)HookAddr, 12, PAGE_EXECUTE_READWRITE, NULL);
	WriteProcessMemory(GetCurrentProcess(), (void*)HookAddr, (void*)&opcode, 11, NULL);
}

//给TLE下载器加入https支持
//默认的下载器只支持http
void HookWebDownloader(){
	uint8_t opcode1[5];
	opcode1[0]=0xE8;
	uintptr_t ParseURL_LStrPos= 0x0049F80D;
	*(long*)&opcode1[1] = (BYTE*)ParseURL_LStrPos_Hook - (BYTE*)ParseURL_LStrPos - 5;
	VirtualProtect((void*)ParseURL_LStrPos, 5, PAGE_EXECUTE_READWRITE, NULL);
	WriteProcessMemory(GetCurrentProcess(), (void*)ParseURL_LStrPos, (void*)opcode1, 5, NULL);

	uint8_t opcode2[5];
	opcode2[0] = 0xE8;
	uintptr_t Downloader_InternetConnectA = 0x0049FA31;
	*(long*)&opcode2[1] = (BYTE*)Downloader_InternetConnectA_Hook - (BYTE*)Downloader_InternetConnectA - 5;
	VirtualProtect((void*)Downloader_InternetConnectA, 5, PAGE_EXECUTE_READWRITE, NULL);
	WriteProcessMemory(GetCurrentProcess(), (void*)Downloader_InternetConnectA, (void*)opcode2, 5, NULL);

	uint8_t opcode3[5];
	opcode3[0] = 0xE8;
	uintptr_t Downloader_HttpOpenRequestA = 0x0049FA9E;
	*(long*)&opcode3[1] = (BYTE*)Downloader_HttpOpenRequestA_Hook - (BYTE*)Downloader_HttpOpenRequestA - 5;
	VirtualProtect((void*)Downloader_HttpOpenRequestA, 5, PAGE_EXECUTE_READWRITE, NULL);
	WriteProcessMemory(GetCurrentProcess(), (void*)Downloader_HttpOpenRequestA, (void*)opcode3, 5, NULL);
}

//程序中的排序算法是按字符串顺序排序，使用修改后的未补零NORAD ID会错误排序
//这里通过将NORAD ID补齐到9个字符来解决这个问题(没招了)
void Hook_FixNoradSorting() { 
	//劫持排序规则获取函数的NORAD ID读取部分
	uint8_t opcode[5];
	opcode[0] = 0xE8;//E8 relative call
	size_t GetNORAD = (size_t)0x004E7D88;
	*(long*)&opcode[1] = (BYTE*)get_norad_for_sorting_wrap - (BYTE*)GetNORAD - 5;//计算相对地址,拼接成完整call指令
	VirtualProtect((void*)GetNORAD, 5, PAGE_EXECUTE_READWRITE, NULL);
	WriteProcessMemory(GetCurrentProcess(), (void*)GetNORAD, (void*)opcode, 5, NULL);
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
		HookOpenDialog();
		HookWebDownloader();
		Hook_FixNoradSorting();
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
		if (SatList.data) sat_destroy(&SatList);
		// 当DLL被卸载或进程终止时执行
		break;
	}
	return TRUE;
}