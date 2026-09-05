#include <Windows.h>
#include <wininet.h>
#include "web.h"

#pragma comment(lib,"wininet.lib")

#define func_LstrPos 0x404388

const char HTTPSProtocolLabel[] = "\x8\0\0\0https://";

__declspec(naked) void ParseURL_LStrPos_Hook(char *Str1,char* Str2) { //Protocol Identifier@<EAX>, URL@<EDX>
	__asm {
		push edx
		push ebx
		mov ebx,func_LstrPos
		call ebx			;test HTTP
		pop ebx
		cmp eax,1
		jne testHTTPS
		add esp,4
		ret

		testHTTPS:
		mov edx,[esp]		;[esp]:URL
		lea eax,HTTPSProtocolLabel
		add eax,4
		push ebx
		mov ebx, func_LstrPos
		call ebx
		pop ebx
		cmp eax, 1
		jne Return
		push eax
		mov eax,[ebp + 8]
		mov WORD ptr[eax],443 ;Port
		pop eax
		mov edx, [ebp-4]	;[ebp-4]:URL
		add edx,7
		mov BYTE ptr[edx],'@'	;Mark as HTTPS

		Return:
		add esp,4
		ret

	}
}



PTypeIdent* __stdcall Downloader_InternetConnectA_Hook(HINTERNET hInternet,
	LPCSTR lpszServerName,
	INTERNET_PORT nServerPort,
	LPCSTR lpszUserName,
	LPCSTR lpszPassword,
	DWORD dwService,
	DWORD dwFlags,
	DWORD_PTR dwContext) {

	PTypeIdent *ident=(PTypeIdent*)malloc(sizeof(PTypeIdent));
	if (!ident) {
		exit(1);
	}
	if (*lpszServerName == '@') {
		ident->type=PROTOCOL_TYPE_HTTPS;
		memmove((void*)lpszServerName,lpszServerName+1,strlen(lpszServerName));
	}
	else {
		ident->type=PROTOCOL_TYPE_HTTP;
	}
	ident->hConnect=InternetConnectA(hInternet,lpszServerName,nServerPort,lpszUserName,lpszPassword,dwService,dwFlags,dwContext);
	return ident;
}

HINTERNET __stdcall Downloader_HttpOpenRequestA_Hook(PTypeIdent* ident,
	LPCSTR lpszVerb,
	LPCSTR lpszObjectName,
	LPCSTR lpszVersion,
	LPCSTR lpszReferrer,
	LPCSTR FAR* lplpszAcceptTypes,
	DWORD dwFlags,
	DWORD_PTR dwContext) {

	HINTERNET hRequest=0;

	if (ident->type == PROTOCOL_TYPE_HTTP) {
		hRequest=HttpOpenRequestA(ident->hConnect,lpszVerb,lpszObjectName,lpszVersion,lpszReferrer,lplpszAcceptTypes,dwFlags,dwContext);
	}
	else if (ident->type == PROTOCOL_TYPE_HTTPS) {
		hRequest=HttpOpenRequestA(ident->hConnect,lpszVerb,lpszObjectName,lpszVersion,lpszReferrer,lplpszAcceptTypes,dwFlags | INTERNET_FLAG_SECURE,dwContext);
	}

	free(ident);
	return hRequest;
}