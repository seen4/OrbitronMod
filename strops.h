#pragma once
#include<string.h>
#include <stdint.h>

void trimString(char* str) {
	char* start = str + strspn(str, " \t\n\r");
	char* end = str + strlen(str) - 1;

	while (end > start && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
		--end;
	}

	*(end + 1) = '\0';

	if (start > str) {
		memmove(str, start, end - start + 2);
	}
}


void CStrToPascalString(char* str) {
	uint8_t len=strlen(str);
	char *buf=(char*)calloc(len+1,sizeof(char));
	strcpy_s(buf,len+1,str);
	str[0]=len;
	memcpy(str+1,buf,len);
	return;
}
