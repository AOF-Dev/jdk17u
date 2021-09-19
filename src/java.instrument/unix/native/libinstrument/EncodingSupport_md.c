/*
 * Copyright (c) 2004, 2018, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <locale.h>
#include <langinfo.h>
#include <iconv.h>

#if defined(__ANDROID__) && __ANDROID_API__ < 26
static char* nl_langinfo(nl_item item) {
  const char* result = "";
  switch (item) {
    case CODESET: result = (MB_CUR_MAX == 1) ? "ASCII" : "UTF-8"; break;

    case D_T_FMT: result = "%F %T %z"; break;
    case D_FMT: result = "%F"; break;
    case T_FMT: result = "%T"; break;
    case T_FMT_AMPM: result = "%I:%M:%S %p"; break;
    case AM_STR: result = "AM"; break;
    case PM_STR: result = "PM"; break;
    case DAY_1: result = "Sunday"; break;
    case DAY_2: result = "Monday"; break;
    case DAY_3: result = "Tuesday"; break;
    case DAY_4: result = "Wednesday"; break;
    case DAY_5: result = "Thursday"; break;
    case DAY_6: result = "Friday"; break;
    case DAY_7: result = "Saturday"; break;
    case ABDAY_1: result = "Sun"; break;
    case ABDAY_2: result = "Mon"; break;
    case ABDAY_3: result = "Tue"; break;
    case ABDAY_4: result = "Wed"; break;
    case ABDAY_5: result = "Thu"; break;
    case ABDAY_6: result = "Fri"; break;
    case ABDAY_7: result = "Sat"; break;
    case MON_1: result = "January"; break;
    case MON_2: result = "February"; break;
    case MON_3: result = "March"; break;
    case MON_4: result = "April"; break;
    case MON_5: result = "May"; break;
    case MON_6: result = "June"; break;
    case MON_7: result = "July"; break;
    case MON_8: result = "August"; break;
    case MON_9: result = "September"; break;
    case MON_10: result = "October"; break;
    case MON_11: result = "November"; break;
    case MON_12: result = "December"; break;
    case ABMON_1: result = "Jan"; break;
    case ABMON_2: result = "Feb"; break;
    case ABMON_3: result = "Mar"; break;
    case ABMON_4: result = "Apr"; break;
    case ABMON_5: result = "May"; break;
    case ABMON_6: result = "Jun"; break;
    case ABMON_7: result = "Jul"; break;
    case ABMON_8: result = "Aug"; break;
    case ABMON_9: result = "Sep"; break;
    case ABMON_10: result = "Oct"; break;
    case ABMON_11: result = "Nov"; break;
    case ABMON_12: result = "Dec"; break;
    case ERA: result = ""; break;
    case ERA_D_FMT: result = ""; break;
    case ERA_D_T_FMT: result = ""; break;
    case ERA_T_FMT: result = ""; break;
    case ALT_DIGITS: result = ""; break;

    case RADIXCHAR: result = "."; break;
    case THOUSEP: result = ""; break;

    case YESEXPR: result = "^[yY]"; break;
    case NOEXPR: result = "^[nN]"; break;

    case CRNCYSTR: result = ""; break;

    default: break;
  }
  return (char*)result;
}
#endif
#if defined(__ANDROID__) && __ANDROID_API__ < 28
iconv_t libiconv_open(const char* __src_encoding, const char* __dst_encoding);
size_t libiconv(iconv_t __converter, char** __src_buf, size_t* __src_bytes_left, char** __dst_buf, size_t* __dst_bytes_left);
int libiconv_close(iconv_t __converter);
#define iconv_open libiconv_open
#define iconv libiconv
#define iconv_close libiconv_close
#endif

/* Routines to convert back and forth between Platform Encoding and UTF-8 */

/* Error and assert macros */
#define UTF_ERROR(m) utfError(__FILE__, __LINE__,  m)
#define UTF_ASSERT(x) ( (x)==0 ? UTF_ERROR("ASSERT ERROR " #x) : (void)0 )
#define UTF_DEBUG(x)

/* Global variables */
static iconv_t iconvToPlatform          = (iconv_t)-1;
static iconv_t iconvFromPlatform        = (iconv_t)-1;

/*
 * Error handler
 */
static void
utfError(char *file, int line, char *message)
{
    (void)fprintf(stderr, "UTF ERROR [\"%s\":%d]: %s\n", file, line, message);
    abort();
}

/*
 * Initialize all utf processing.
 */
static void
utfInitialize(void)
{
    const char* codeset;

    /* Set the locale from the environment */
    (void)setlocale(LC_ALL, "");

    /* Get the codeset name */
    codeset = (char*)nl_langinfo(CODESET);
    if ( codeset == NULL || codeset[0] == 0 ) {
        UTF_DEBUG(("NO codeset returned by nl_langinfo(CODESET)\n"));
        return;
    }

    UTF_DEBUG(("Codeset = %s\n", codeset));

#ifdef MACOSX
    /* On Mac, if US-ASCII, but with no env hints, use UTF-8 */
    const char* env_lang = getenv("LANG");
    const char* env_lc_all = getenv("LC_ALL");
    const char* env_lc_ctype = getenv("LC_CTYPE");

    if (strcmp(codeset,"US-ASCII") == 0 &&
        (env_lang == NULL || strlen(env_lang) == 0) &&
        (env_lc_all == NULL || strlen(env_lc_all) == 0) &&
        (env_lc_ctype == NULL || strlen(env_lc_ctype) == 0)) {
        codeset = "UTF-8";
    }
#endif

    /* If we don't need this, skip it */
    if (strcmp(codeset, "UTF-8") == 0 || strcmp(codeset, "utf8") == 0 ) {
        UTF_DEBUG(("NO iconv() being used because it is not needed\n"));
        return;
    }

    /* Open conversion descriptors */
    iconvToPlatform   = iconv_open(codeset, "UTF-8");
    if ( iconvToPlatform == (iconv_t)-1 ) {
        UTF_ERROR("Failed to complete iconv_open() setup");
    }
    iconvFromPlatform = iconv_open("UTF-8", codeset);
    if ( iconvFromPlatform == (iconv_t)-1 ) {
        UTF_ERROR("Failed to complete iconv_open() setup");
    }
}

/*
 * Terminate all utf processing
 */
static void
utfTerminate(void)
{
    if ( iconvFromPlatform!=(iconv_t)-1 ) {
        (void)iconv_close(iconvFromPlatform);
    }
    if ( iconvToPlatform!=(iconv_t)-1 ) {
        (void)iconv_close(iconvToPlatform);
    }
    iconvToPlatform   = (iconv_t)-1;
    iconvFromPlatform = (iconv_t)-1;
}

/*
 * Do iconv() conversion.
 *    Returns length or -1 if output overflows.
 */
static int
iconvConvert(iconv_t ic, char *bytes, int len, char *output, int outputMaxLen)
{
    int outputLen = 0;

    UTF_ASSERT(bytes);
    UTF_ASSERT(len>=0);
    UTF_ASSERT(output);
    UTF_ASSERT(outputMaxLen>len);

    output[0] = 0;
    outputLen = 0;

    if ( ic != (iconv_t)-1 ) {
        int          returnValue;
        size_t       inLeft;
        size_t       outLeft;
        char        *inbuf;
        char        *outbuf;

        inbuf        = bytes;
        outbuf       = output;
        inLeft       = len;
        outLeft      = outputMaxLen;
        returnValue  = iconv(ic, (void*)&inbuf, &inLeft, &outbuf, &outLeft);
        if ( returnValue >= 0 && inLeft==0 ) {
            outputLen = outputMaxLen-outLeft;
            output[outputLen] = 0;
            return outputLen;
        }

        /* Failed to do the conversion */
        UTF_DEBUG(("iconv() failed to do the conversion\n"));
        return -1;
    }

    /* Just copy bytes */
    outputLen = len;
    (void)memcpy(output, bytes, len);
    output[len] = 0;
    return outputLen;
}

/*
 * Convert UTF-8 to Platform Encoding.
 *    Returns length or -1 if output overflows.
 */
static int
utf8ToPlatform(char *utf8, int len, char *output, int outputMaxLen)
{
    return iconvConvert(iconvToPlatform, utf8, len, output, outputMaxLen);
}

/*
 * Convert Platform Encoding to UTF-8.
 *    Returns length or -1 if output overflows.
 */
static int
platformToUtf8(char *str, int len, char *output, int outputMaxLen)
{
    return iconvConvert(iconvFromPlatform, str, len, output, outputMaxLen);
}

int
convertUft8ToPlatformString(char* utf8_str, int utf8_len, char* platform_str, int platform_len) {
    if (iconvToPlatform ==  (iconv_t)-1) {
        utfInitialize();
    }
    return utf8ToPlatform(utf8_str, utf8_len, platform_str, platform_len);
}
