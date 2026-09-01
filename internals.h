#pragma once

__declspec(naked) int __fastcall LStrFromString(char*, const char*) {
	__asm {
		mov eax, ecx
		mov ecx, 0x404040
		jmp ecx
	}
}

__declspec(naked) int __fastcall LstrFromArray(char* Dest, const char* Source, int Length) {
	__asm {
		mov eax, ecx
		mov ecx, DWORD ptr[esp + 4]
		push ebx
		mov ebx, 0x40404C
		call ebx
		pop ebx
		ret 4
	}
}

void SetTLEReadState(WORD value) {
	__asm {
		mov ax,value
		mov WORD ptr ds:[0x4FCDD4],ax
	}
}

FILE** GetTLEfd() {
	FILE **fd=NULL;
	__asm {
		mov fd,0x4FCC88
	}
	return fd;
}
