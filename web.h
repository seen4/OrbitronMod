#pragma once
#include <Windows.h>
#include <wininet.h>

enum protocol_type {
	PROTOCOL_TYPE_HTTP,
	PROTOCOL_TYPE_HTTPS
};

typedef struct {
	HINTERNET hConnect;
	protocol_type type;
} PTypeIdent;

void ParseURL_LStrPos_Hook(char* Str1, char* Str2);

PTypeIdent* __stdcall Downloader_InternetConnectA_Hook(HINTERNET hInternet,
	LPCSTR lpszServerName,
	INTERNET_PORT nServerPort,
	LPCSTR lpszUserName,
	LPCSTR lpszPassword,
	DWORD dwService,
	DWORD dwFlags,
	DWORD_PTR dwContext);

HINTERNET __stdcall Downloader_HttpOpenRequestA_Hook(PTypeIdent* ident,
	LPCSTR lpszVerb,
	LPCSTR lpszObjectName,
	LPCSTR lpszVersion,
	LPCSTR lpszReferrer,
	LPCSTR FAR* lplpszAcceptTypes,
	DWORD dwFlags,
	DWORD_PTR dwContext);