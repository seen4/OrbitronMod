#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <malloc.h>
#include "kvn.h"
#include "../tool/string_operation.h"

#define KVN_LINE_BUF 512


/* Create a convenient scope to free string after use
*  @param1 pointer to line buffer
*  @param2 desired name to store value
*  @param3 code to proccess value
*/
#define READSCOPE(buf_ptr,name,code)    \
        {char* name = getvalue(buf_ptr);\
        code                            \
        free(name);}                    \



/* 从流读取一行(含换行); 行超长时丢弃剩余部分并警告。返回 1 成功 / 0 EOF。 */
static int read_next_line(FILE* fp, char* buf, size_t bufsz) {
    if (!fgets(buf, (int)bufsz, fp)) {
        //fprintf(stderr, "ERROR: %s\n", strerror(errno));
        return 0;}
    if (!strchr(buf, '\n') && !feof(fp)) {
        fprintf(stderr, "警告: 行超过 %u 字符, 已丢弃\n", (unsigned)bufsz);
        while (fgets(buf, (int)bufsz, fp)) {
            if (strchr(buf, '\n')) break;
        }
    }
    return 1;
}

static void revert(FILE *fp) {
    for (char chr; chr = fgetc(fp);) {
        if(chr=='\n') break;
        if(ftell(fp)<3) {rewind(fp);return;}
        if (fseek(fp, -2, SEEK_CUR)) {
            fprintf(stderr, "ERROR seeking: %s\n", strerror(errno));
            return;}
    }
}

static bool skip_empty_lines(FILE* fp) {
    char chr;
    do {
        chr=fgetc(fp);
        if(feof(fp)) return 0;
        if (chr != '\n') {
            revert(fp);
            return 1;
        }
    }while(chr);
}

static int keycmp(const char* linebuf, char* key) {
    char* buf = _alloca(strlen(linebuf));
    strncpy(buf, linebuf, strlen(linebuf));
    char* tok = strtok(buf, "=\n");
    if(!tok) return 1;
    trim(tok);
    return strcmp(key, tok);
}

static char* getvalue(const char* linebuf) {
    char* buf = malloc(strlen(linebuf));
    memcpy(buf, linebuf, strlen(linebuf));
    strtok(buf, "=\n");
    char* tok = strtok(NULL, "=\n");
    trim(tok);
    size_t len = strlen(tok);
    memmove(buf, tok, len);
    buf[len] = '\0';
    return buf;
}

static char linebuf[KVN_LINE_BUF];
static bool init = false;

void kvn_reset() {
    init=false;
}

_Bool kvn_detect(FILE* fp) {
    bool result=false;
    if(!skip_empty_lines(fp)) goto RETURN;
    if (!read_next_line(fp, &linebuf, _countof(linebuf))) {
       return false;
    }
    if (!keycmp(&linebuf, "CCSDS_OMM_VERS")) {
        result=true;
    }
    RETURN:
    rewind(fp);
    return result;
}

int kvn_parse(FILE* fp, omm_t* data) {
    static bool status=false;
    while (true) {
        if (!read_next_line(fp, &linebuf, sizeof(linebuf))) {
            if(status) {status=false;return 1;}
            return init ?  0 : -1;
        }
        if (!keycmp(&linebuf, "OBJECT_NAME")) {
            READSCOPE(&linebuf, value,
               strncpy(data->object_name,value,OMM_NAME_BUF);)
            status=true;
            continue;
        }
        if (!keycmp(&linebuf, "OBJECT_ID")) {
            READSCOPE(&linebuf, value,
                strncpy(data->object_id, value, _countof(data->object_id));)
            status = true;
            continue;
        }
        if (!keycmp(&linebuf, "EPOCH")) {
            READSCOPE(&linebuf, value,
                strncpy(data->epoch, value, _countof(data->epoch));)
            status = true;
            continue;
        }
        if (!keycmp(&linebuf, "MEAN_MOTION")) {
            READSCOPE(&linebuf, value,
                parse_dbl(value, &data->mean_motion);)
            status = true;
            continue;
        } 
        if (!keycmp(&linebuf, "ECCENTRICITY")) {
            READSCOPE(&linebuf, value,
                parse_dbl(value, &data->eccentricity);)
            status = true;
            continue;
        }
        if (!keycmp(&linebuf, "INCLINATION")) {
            READSCOPE(&linebuf, value,
                parse_dbl(value, &data->inclination);)
            status = true;
            continue;
        }
        if (!keycmp(&linebuf, "RA_OF_ASC_NODE")) {
            READSCOPE(&linebuf, value,
                parse_dbl(value, &data->raan);)
            status = true;
            continue;
        }
        if (!keycmp(&linebuf, "ARG_OF_PERICENTER")) {
            READSCOPE(&linebuf, value,
                parse_dbl(value, &data->argp);)
            status = true;
            continue;
        }
        if (!keycmp(&linebuf, "MEAN_ANOMALY")) {
            READSCOPE(&linebuf, value,
                parse_dbl(value, &data->mean_anomaly);)
            status = true;
            continue;
        }
        if (!keycmp(&linebuf, "EPHEMERIS_TYPE")) {
            READSCOPE(&linebuf, value,
                data->ephemeris_type=*value;)
            status = true;
            continue;
        }
        if (!keycmp(&linebuf, "CLASSIFICATION_TYPE")) {
            READSCOPE(&linebuf, value,
                data->classification=*value;)
            status = true;
            continue;
        }
        if (!keycmp(&linebuf, "NORAD_CAT_ID")) {
            READSCOPE(&linebuf, value,
                char* end = NULL;
                data->norad_id = strtoul(value, end, 10);
                if (end == value) return -1;)
            status = true;
            continue;
        }
        if (!keycmp(&linebuf, "ELEMENT_SET_NO")) {
            READSCOPE(&linebuf, value,
                parse_int(value, &data->element_set);)
            status = true;
            continue;
        }
        if (!keycmp(&linebuf, "REV_AT_EPOCH")) {
            READSCOPE(&linebuf, value,
                parse_int(value, &data->rev);)
            status = true;
            continue;
        }
        if (!keycmp(&linebuf, "BSTAR")) {
            READSCOPE(&linebuf, value,
                parse_dbl(value, &data->bstar);)
            status = true;
            continue;
        }
        if (!keycmp(&linebuf, "MEAN_MOTION_DOT")) {
            READSCOPE(&linebuf, value,
                parse_dbl(value, &data->ndot);)
            status = true;
            continue;
        }
        if (!keycmp(&linebuf, "MEAN_MOTION_DDOT")) {
            READSCOPE(&linebuf, value,
                parse_dbl(value, &data->nddot);)
            status = true;
            continue;
        }
        if (!keycmp(&linebuf, "CCSDS_OMM_VERS")) {
            status=false;
            if (init == false) {
                init = true; continue;
            }
            return 1;
        }
    }
}
