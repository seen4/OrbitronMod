#define _CRT_SECURE_NO_WARNINGS
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<stdbool.h>
#include<ctype.h>
#include <malloc.h>
#include"xml.h"

#define KVN_LINE_BUF 512

#define ulong unsigned long
#define uchar unsigned char

#define READSCOPE(fp,code) {char* value=getvalue(fp);\
							code\
							free(value);}

typedef ulong file_pos_t;

/*Acquire a single char from filestream
* Return 0 when failed or EOF*/
static uchar xml_getc(FILE* fp) {
	uchar ch = fgetc(fp);
	if (!ch || feof(fp)) return 0;
	return ch;
}

/*Allocate string on the heap, do remeber to call free() after use
 Return 0 when failed */
static char* get_nodename(FILE* fp) {
	ulong head = ftell(fp);

	size_t nlen = 0;
	for (uchar ch = xml_getc(fp);
		ch != '>' && ch != ' ' && ch != '/'; ch = xml_getc(fp)) {
		if(nlen>32) return 0;
		if (ch == 0) return 0;
		nlen++;
	}
	char* buffer = (char*)malloc(++nlen);

	fseek(fp, head, SEEK_SET);
	fgets(buffer, nlen, fp);
	for (uchar ch = xml_getc(fp); ch != '>'; ch = xml_getc(fp)) {
		if (ch == 0) return 0;
	}
	return buffer;
}

static int keycmp(const char* Str1, const char* Str2) {
	char* buf1 = _alloca(strlen(Str1));
	for (size_t i = 0; i < strlen(Str1); i++) {
		buf1[i] = toupper(Str1[i]);
	}
	char* buf2 = _alloca(strlen(Str1));
	for (size_t i = 0; i < strlen(Str1); i++) {
		buf2[i] = toupper(Str2[i]);
	}
	return strcmp(Str1, Str2);
}

static char* getvalue(FILE* fp) {
	file_pos_t floc = ftell(fp);

	size_t nlen = 0;
	for (uchar ch = xml_getc(fp);
		ch != '<' ; ch = xml_getc(fp)) {
		if (ch == 0) return 0;
		nlen++;
	}
	char* buffer = (char*)malloc(++nlen);

	fseek(fp, floc, SEEK_SET);
	fgets(buffer, nlen, fp);
	return buffer;
}

static inline void revert(FILE* fp) {
	fseek(fp, -1, SEEK_CUR);
}

static char* xml_next_tag(FILE* fp) {
	for (uchar ch; ch = xml_getc(fp);) {
		if (ch == '<') if ((ch = xml_getc(fp)) != '/') {
			revert(fp);
			return get_nodename(fp);
		}
	}
	return 0;
}

static bool skip_empty_lines(FILE* fp) {
	char chr;
	do {
		chr = fgetc(fp);
		if (feof(fp)) return 0;
		if (chr != '\n') {
			revert(fp);
			return 1;
		}
	} while (chr);
}

static bool init=false;
static bool eof=false;

void xml_reset() {
	init=false;
	eof=false;
}

bool xml_detect(FILE* fp) {
	bool result = false;
	if (!skip_empty_lines(fp)) goto RETURN;
	if (xml_getc(fp) != '<') goto RETURN;
	char* name=get_nodename(fp);
	if (!name) goto RETURN;
	if (!keycmp(name, "?xml")) {
		result = true;
	}
	free(name);
RETURN:
	rewind(fp);
	return result;
}

static int getkeys(FILE* fp, omm_t* data) {
	for (char* tag; tag = xml_next_tag(fp);) {
		if (!keycmp(tag, "OBJECT_NAME")) {
			READSCOPE(fp,
				strncpy(data->object_name, value, OMM_NAME_BUF);)
				free(tag);
			continue;
		}
		if (!keycmp(tag, "OBJECT_ID")) {
			READSCOPE(fp,
				strncpy(data->object_id, value, _countof(data->object_id));
				)
		}
		if (!keycmp(tag, "EPOCH")) {
			READSCOPE(fp,
				strncpy(data->epoch, value, _countof(data->epoch));
				)
		}
		if (!keycmp(tag, "MEAN_MOTION")) {
			READSCOPE(fp,
				parse_dbl(value, &data->mean_motion);)
		}
		if (!keycmp(tag, "ECCENTRICITY")) {
			READSCOPE(fp,
				parse_dbl(value, &data->eccentricity);)
		}
		if (!keycmp(tag, "INCLINATION")) {
			READSCOPE(fp,
				parse_dbl(value, &data->inclination);)
		}
		if (!keycmp(tag, "RA_OF_ASC_NODE")) {
			READSCOPE(fp,
				parse_dbl(value, &data->raan);)
		}
		if (!keycmp(tag, "ARG_OF_PERICENTER")) {
			READSCOPE(fp,
				parse_dbl(value, &data->argp);)
		}
		if (!keycmp(tag, "MEAN_ANOMALY")) {
			READSCOPE(fp,
				parse_dbl(value, &data->mean_anomaly);)
		}
		if (!keycmp(tag, "EPHEMERIS_TYPE")) {
			READSCOPE(fp,
				data->ephemeris_type = *value;)
		}
		if (!keycmp(tag, "CLASSIFICATION_TYPE")) {
			READSCOPE(fp,
				data->classification = *value;)
		}
		if (!keycmp(tag, "NORAD_CAT_ID")) {
			READSCOPE(fp,
				char* end = NULL;
			data->norad_id = strtoul(value, end, 10);)
		}
		if (!keycmp(tag, "ELEMENT_SET_NO")) {
			READSCOPE(fp,
				parse_int(value, &data->element_set);)
		}
		if (!keycmp(tag, "REV_AT_EPOCH")) {
			READSCOPE(fp,
				parse_int(value, &data->rev);)
		}
		if (!keycmp(tag, "BSTAR")) {
			READSCOPE(fp,
				parse_dbl(value, &data->bstar);)
		}
		if (!keycmp(tag, "MEAN_MOTION_DOT")) {
			READSCOPE(fp,
				parse_dbl(value, &data->ndot);)
		}
		if (!keycmp(tag, "MEAN_MOTION_DDOT")) {
			READSCOPE(fp,
				parse_dbl(value, &data->nddot);)
		}
		if (!keycmp(tag, "omm")) {
			free(tag);
			return 1;
		}
		if (!keycmp(tag, "ndm")) {
			puts("EOF\n");
			return 0;
		}
		free(tag);
	}
	return 0;
}

int xml_parse(FILE* fp, omm_t* data) {
	if(eof) return 0;
	while (!init) {
		char* name = xml_next_tag(fp);
		if(!name) return -1;
		if (!keycmp(name, "omm")) {
			init=true;
			free(name);
			break;
		}
		free(name);
	}

	int result = getkeys(fp, data);
	if(result==0) eof=true;

	return 1;
}