 /* 
  * CPTInfo - Corel PhotoPaint file information tool.
  * Copyright (c) 2006-2008 Jakub Argasiński (argasek@gmail.com).
  * 
  * This is a part of CPTInfo.
  *
  * CPTInfo is free software; you can redistribute it and/or modify it
  * under the terms of the GNU Lesser General Public License as published by
  * the Free Software Foundation; either version 2 of the License, or (at your
  * option) any later version.
  * 
  * This program is distributed in the hope that it will be useful, but WITHOUT
  * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
  * License for more details.
  * 
  * You should have received a copy of the GNU Lesser General Public License
  * along with this library; if not, write to the Free Software Foundation,
  * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
  */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <inttypes.h>
#include <locale.h>
#include <glib.h>
#include <math.h>
#include <string.h>
#include <sys/stat.h>           // mkdir()
#include <sys/types.h>
#ifdef WIN32
#include <windows.h>
#endif


#include "cpt.h"
#include "cpt6.h"

#define CI_VERSION              "0.039"     // CPTInfo version

#ifdef WIN32
#define CI_PATH_SEPARATOR       '\\'
#else
#define CI_PATH_SEPARATOR       '/'
#endif

// Command line arguments
#define CI_ARG_CHARSET          "-c"
#define CI_ARG_SHORT_OUTPUT     "-s"
#define CI_ARG_MORE_VERBOSE     "-v"
#define CI_ARG_BLOCK_RANGE      "-br"
#define CI_ARG_DUMP_ICC         "-di"
#define CI_ARG_DUMP_BLOCKS      "-db"
#define CI_ARG_DUMP_PAL         "-dp"
#define CI_ARG_OUTPUT_DATA      "-od"
#define CI_ARG_OUTPUT_RESV      "-or"
#define CI_ARG_OUTPUT_CHUNK     "-oc"
#define CI_ARG_SHORT_NOHEAD     "-sh"

// Macros to retrieve 32-bit or 16-bit unsigned/signed
// value from (buf+addr) byte offset 
#define GETu32(buf,addr) (*((uint32_t *) (buf+addr)))
#define GETu16(buf,addr) (*((uint16_t *) (buf+addr)))
#define GETs32(buf,addr) (*((int32_t *) (buf+addr)))
#define GETs16(buf,addr) (*((int16_t *) (buf+addr)))

#define CI_CPTVER_78(ver) (ver == 0x700 || ver == 0x701 || ver == 0x800)


// Variables holding read information about .cpt
typedef struct _CI_info {
    uint32_t    version;
    uint8_t     pal_ok;
    uint16_t    pal_entries;
    uint16_t    xdpi;
    uint16_t    ydpi;
    uint32_t    blocks_num;
    uint32_t    blocks_table_offs;
    uint32_t    wcomment_offs;
    uint16_t    emb_wcomment;
    uint16_t    emb_icc;
    uint8_t     flag_hi;
    uint8_t     flag_lo;
} CI_info;

// Config variables
typedef struct _CI_config {
    uint32_t    verbose;
    uint32_t    verbose2;
    uint32_t    dump_icc;
    uint32_t    dump_blocks;
    uint32_t    dump_palette;
    uint32_t    output_data;
    uint32_t    block_range;    // true/false
    uint32_t    block_1st;
    uint32_t    block_last;
    uint32_t    output_chunks;
    uint32_t    output_reserved;
    uint32_t    verbosity_level;
    uint32_t    silent_header;
    uint32_t    silent_blocks;
    char        *charset;
} CI_cfg;

// Structure of argument array member
typedef struct _CI_arg {
    const uint8_t   subnum;     // number of subparameters
    uint8_t         pos;        // found at position pos of command line
    const char      *name;      // name of command line arg (w/o '-' prefix)
    const char      *help;      // use one-liner
    uint32_t        *var;       // variable assigned to argument, NULL if none
    const uint32_t  val;        // variable value if argument found
} CI_arg;


// --- Global Variables and Named Contants ---

const char ci_msg_yes[] = "yes";
const char ci_msg_no[] = "no";
const char ci_msg_wnmark[] = " [!]";
const char ci_msg_9mark[] = " [9]";
const char ci_msg_chunk_var_tab[] = "         ";
const char ci_error_str[] = "ERROR:";
const char ci_warning_str[] = "WARNING:";
const char ci_default_charset_a[] = "cp1250";
const char ci_error_file_corrupt_str[] = "%s File is corrupt!\n";
const char ci_error_file_notcpt_str[] = "%s File is not in CPT format!\n";
const char ci_path_separator = CI_PATH_SEPARATOR;
const char ci_msg_welcome[] = "CPTInfo v"CI_VERSION" (c) 2006-2008 Jakub Argasinski (argasek@gmail.com).\n" \
    "This program comes with ABSOLUTELY NO WARRANTY. This is free software,\n" \
    "and you are welcome to redistribute it under conditions of\n" \
    "GNU Lesser General Public License v3.\n";

CI_cfg ci_cfg = { 0, 0, 0, 0, 0 };

CI_arg ci_arg[] = {
    { 1, 0, CI_ARG_CHARSET,      "<charset> specify charset of .cpt file (default: cp1250)", NULL, 0 },
    { 0, 0, CI_ARG_SHORT_OUTPUT, "shortened output, useful for script parsing", &ci_cfg.verbose, 0 },
    { 0, 0, CI_ARG_SHORT_NOHEAD, "don't print file header info when in -s mode", &ci_cfg.silent_header, 0 },
    { 0, 0, CI_ARG_MORE_VERBOSE, "more detailed data", &ci_cfg.verbose2, 1 },
    { 1, 0, CI_ARG_BLOCK_RANGE,  "<n|n-m> output info only for blocks n-m (default: all blocks)", NULL, 1 },
    { 0, 0, CI_ARG_DUMP_ICC,     "dump ICC profile if present as file.icc", &ci_cfg.dump_icc, 1 },
    { 0, 0, CI_ARG_DUMP_BLOCKS,  "dump blocks as files (file.000, file.001, ...)", &ci_cfg.dump_blocks, 1 },
    { 0, 0, CI_ARG_DUMP_PAL,     "dump palette as file.pal (8-bit RGB only)", &ci_cfg.dump_palette, 1 },
    { 0, 0, CI_ARG_OUTPUT_DATA,  "output data block pairs", &ci_cfg.output_data, 1 },
    { 0, 0, CI_ARG_OUTPUT_RESV,  "output reserved fields info (default: unusual only)", &ci_cfg.output_reserved, 1 },
    { 0, 0, CI_ARG_OUTPUT_CHUNK, "output chunks information", &ci_cfg.output_chunks, 1 },
    { 0, 0, NULL, NULL }
};

uint32_t ci_filename_pos;               // argc of filename
char *ci_filename = NULL;               // Full file name with path
char *ci_filename_short = NULL;         // File name with path stripped
char *ci_filename_short__ = NULL;       // Like above, ' ' -> '_'
char *ci_basename = NULL;               // Like ci_filename_short, but .ext stripped
char *ci_tempname = NULL;               // Like basename + 4 bytes for new '.ext' 
uint8_t *ci_data = NULL;                // raw file data
const gchar *ci_charset;                // Default charset to use if not specified via -c
FILE *f = NULL;                         // .cpt file handle
int32_t ci_filesize;                    // .cpt file size
uint32_t ci_blocks_table_offs_eval;     // evaluated block_table_offs value, for comparision

CI_info ci;

CPT_FileHeader          *ci_f_header;
CPT_Palette             *ci_palette;
CPT_BlockTableEntry     *ci_blocks_table;
CPT_ICC                 *ci_icc;
CPT_WideComment         *ci_wcomment;


// --- Helper Functions ---

// Prints a message, according to given verbosity level(s). Levels as bits:
// 1 == verbose
// 2 == more verbose
// 4 == silent header
// 8 == silent blocks

void ci_msg(uint32_t level, const char *msg, ...) {
#ifndef SHUT_UP
    va_list ap;
    if ((level & ci_cfg.verbosity_level) == level) {
        va_start(ap, msg);
        vprintf(msg, ap);
        va_end(ap);
    }
#endif
}



// Measures length of UCS-2 string.
uint32_t ci_strlen_w(const CPT_wchar *buf) {
    uint32_t i = 0;
    while (*buf) { i++; buf++; }
    return i;
}

// Gets *file file size.
int32_t ci_FileSize(FILE *file) {
    fseek(file, 0, SEEK_END);
    int32_t size = ftell(file);
    rewind(file);
    return size;
}

// Displays 32-bit dword as ASCII.
char *ci_Ascii32(uint32_t x) {
    static char a[5];
    uint32_t mask = 0x000000FF;
    a[3] = x & mask;
    a[2] = (x >> 8) & mask;
    a[1] = (x >> 16) & mask;
    a[0] = (x >> 24) & mask;
    // TODO: actually on Win32 only some values should be
    // replaced by ' ', because most chars have some images
    if (a[0] <= 32 || a[0] >= 127) a[0] = ' ';
    if (a[1] <= 32 || a[1] >= 127) a[1] = ' ';
    if (a[2] <= 32 || a[2] >= 127) a[2] = ' ';
    if (a[3] <= 32 || a[3] >= 127) a[3] = ' ';
    a[4] = '\0';
    return a;
}

// Returns argc of given argument, or 0 if not found.
uint32_t ci_FindArg(char *arg) {
    uint32_t j;
    for (j=0; ci_arg[j].name; j++) {
        // Argument found, save it's position
        if (!strcmp(ci_arg[j].name, arg)) return ci_arg[j].pos;
    }
    return 0;
}

// Returns 1 if given argc is in arguments table.
uint32_t ci_FindPos(uint32_t argnum) {
    uint32_t i;
    for (i=0; ci_arg[i].name; i++) {
        // Argument found, save it's position
        if (ci_arg[i].pos == argnum) return 1;
    }
    return 0;
}

// Checks if argument of given argc is a valid subargument.
// Return 1 if OK.
uint32_t ci_IsSubArg(uint32_t argnum) {
    if (argnum != ci_filename_pos && !ci_FindPos(argnum)) return 1;
    return 0;
}

// ------------------------- PROGRAM BODY -------------------------

void ci_ProcessArguments(int argc, char **argv) {
    uint32_t i, j, k, found, err = 0;
    // Not enough arguments - no fun
    if (argc < 2) {
        printf(ci_msg_welcome);
        printf("Usage: %s [options...] <file.cpt>\n", argv[0]);
        uint8_t maxlen = 0;
        for (i=0; ci_arg[i].name; i++) if (strlen(ci_arg[i].name) > maxlen) maxlen = strlen(ci_arg[i].name);
        for (i=0; ci_arg[i].name; i++) {
            printf("   %s", ci_arg[i].name);
            for (j=0; j < maxlen+1-strlen(ci_arg[i].name); j++) printf(" ");
            printf("%s\n", ci_arg[i].help);
        }
        exit(EXIT_SUCCESS);
    }
    // Process program arguments
    for (i=1; i < argc; i++) {
        found = 0;
        for (j=0; ci_arg[j].name; j++) {
            // Argument found, save it's position
            if (!strcmp(ci_arg[j].name, argv[i])) {
                ci_arg[j].pos = i;
                if (ci_arg[j].var) *ci_arg[j].var = ci_arg[j].val;
                i += ci_arg[j].subnum;
                found = 1;
                break;
            }
        }
        // If argument found, look up next one
        if (found) continue;
        // Not found, so probably it's a filename
        // However if already filename found, it's an error
        if (!found && ci_filename)  {
            ci_filename = NULL;
            break;
        }
        // This is a filename then
        ci_filename_pos = i;
        ci_filename = argv[i];
    }
    // If no file name provided, report as error
    if (!ci_filename) {
        printf("%s Invalid command line parameters given!\n", ci_error_str);
        exit(EXIT_FAILURE);
    }
    // Get short file name
    ci_filename_short = strrchr(ci_filename, ci_path_separator);
    if (!ci_filename_short) ci_filename_short = ci_filename;
    else ci_filename_short++;
    // Make short file name with '_'
    k = strlen(ci_filename_short);
    ci_filename_short__ = (char *) malloc(k+1);
    strcpy(ci_filename_short__, ci_filename_short);
    for (i=0; i < k; i++) if (ci_filename_short__[i] == ' ') ci_filename_short__[i] = '_';

    char *dot = strrchr(ci_filename_short, '.');
    uint32_t dotpos;
    if (dot) dotpos = dot - ci_filename_short;
	else dotpos = strlen(ci_filename_short);
    ci_basename = (char *) malloc(dotpos+1);            // basename + \0
    ci_tempname = (char *) malloc(dotpos+1+4);          // basename + .ext + \0
    memcpy(ci_basename, ci_filename_short, dotpos);
    ci_basename[dotpos] = '\0';

    // Get -c <charset> value
    uint32_t arg_pos;
    arg_pos = ci_FindArg(CI_ARG_CHARSET);
    if (arg_pos) ci_cfg.charset = argv[++arg_pos];

    // Block range
    arg_pos = ci_FindArg(CI_ARG_BLOCK_RANGE);
    if (arg_pos) {
        arg_pos++;
        // Check if subargument given
        if (ci_IsSubArg(arg_pos)) {
            ci_cfg.block_range = 1;
            // Check if it just one block
            if (!strchr(argv[arg_pos], '-')) {
                ci_cfg.block_1st = atoi(argv[arg_pos]);
                ci_cfg.block_last = ci_cfg.block_1st;
            // No, it's a range then
            } else {
                sscanf(argv[arg_pos], "%u-%u", &ci_cfg.block_1st, &ci_cfg.block_last);
                if (ci_cfg.block_1st > ci_cfg.block_last) {
                    uint32_t tmp = ci_cfg.block_1st;
                    ci_cfg.block_1st = ci_cfg.block_last;
                    ci_cfg.block_last = tmp;
                }
            }
        } else {
            ci_cfg.block_range = 0;
        }
    }
    
    if (!ci_cfg.silent_header) ci_cfg.verbose = 0;
    if (ci_cfg.verbose) {
        ci_cfg.silent_blocks = 0;
        ci_cfg.silent_header = 0;
    }

    ci_cfg.verbosity_level |= ci_cfg.verbose;
    ci_cfg.verbosity_level |= ci_cfg.verbose2 << 1;
    ci_cfg.verbosity_level |= ci_cfg.silent_header << 2;
    ci_cfg.verbosity_level |= ci_cfg.silent_blocks << 3;
}

void ci_ReadFileContents(void) {
    uint32_t i, is_cpt = 0;
    // Try to open .cpt file, get file size.
    if (!(f = fopen(ci_filename, "rb"))) {
        printf("%s Can't open file %s!\n", ci_error_str, ci_filename);
        exit(EXIT_FAILURE);
    }
    ci_filesize = ci_FileSize(f);
    // If filesize smaller then size of header, file
    // is corrupt for sure. Will check more later
    if (ci_filesize < CPT_FileHeader_sz) {
        printf(ci_error_file_corrupt_str, ci_error_str);
        exit(EXIT_FAILURE);
    }
    
    printf("%d costam\n");
    
    // Read the header
    ci_data = (uint8_t *) malloc(CPT_FileHeader_sz);
    fread(ci_data, 1, CPT_FileHeader_sz, f);
    ci_f_header = (CPT_FileHeader *) ci_data;
    // Is it CPT7-CPT9 file?    
    for (i=0; i < CPT_VERSIONS_NUM; i++) {
        if (!strncmp(ci_f_header->magic, cpt_version[i].magic, cpt_version[i].len)) {
            ci.version = cpt_version[i].version;
            // Corel Photo-Paint acts like this...
            if ((ci_f_header->flags & CPT_VERSION_7_01_MASK) == CPT_VERSION_7_01
                && ci.version == 0x700) ci.version = 0x701;
            is_cpt = 1;
            break;
        }
    }
    // If not, maybe it's CPT6
    if (!is_cpt) {
        if (!strncmp(ci_f_header->magic, cpt6_magic, CPT6_MAGIC_sz) &&
            !strncmp(ci_data + cpt6_version_offs, cpt6_version, CPT6_VERSION_sz)) {
            ci.version = 0x600;
            is_cpt = 1;
        }
    }
    // It's not CPT, thus an error
    if (!is_cpt) {
        printf(ci_error_file_notcpt_str, ci_error_str);
        exit(EXIT_FAILURE);
    }
    // Read the rest of the file
    ci_data = (uint8_t *) realloc(ci_data, ci_filesize);
    ci_f_header = (CPT_FileHeader *) ci_data;
    fread(ci_data+CPT_FileHeader_sz, 1, ci_filesize-CPT_FileHeader_sz, f);
    
    fclose(f); f = NULL;
}


void ci_ProcessFileHeader(void) {
    uint32_t is_mask = 1, dpi_warn = 0, pal_warn = 0;
    uint32_t bt_warn = 0, bt_cpt9 = 2, bn_warn = 0;
    uint32_t res_warn[5] = { 0, 0, 0, 0, 0};
    uint32_t unk_warn[3] = { 0, 0, 0 };
    uint32_t i;
    
    ci_msg(1, "CPT file: %s (%d bytes)\n", ci_filename, ci_filesize);
    ci_msg(4, "%s %d", ci_filename_short__, ci_filesize);
    // --- Version detection
    ci_msg(1, "CPT file format: ");
    switch (ci.version) {
        case 0x600: ci_msg(1, "6.0"); ci_msg(4, " CPT6"); break;
        case 0x700: ci_msg(1, "7.0"); ci_msg(4, " CPT7"); break;
        case 0x701: ci_msg(1, "7.01"); ci_msg(4, " CPT701"); break;
        case 0x800: ci_msg(1, "8.0"); ci_msg(4, " CPT8"); break;
        case 0x900: ci_msg(1, "9.0-13.0"); ci_msg(4, " CPT9"); break;
    }
    ci_msg(1,"\n");
    // TODO: handle Corel PhotoPaint 6 files.
    if (ci.version == 0x600) {
        ci_msg(1, "This version of CPTInfo doesn't handle CPT 6.0 files yet. :(\n");
        ci_msg(12, " ver!");
        exit(EXIT_SUCCESS);
    }
    ci.flag_hi = (uint8_t) (ci_f_header->flags & 0xFF00) >> 8;
    ci.flag_lo = (uint8_t) (ci_f_header->flags & 0x00FF);
    ci_msg(1, "CPT creator version: ");
    switch (ci.flag_lo) {
        case CPT_AV_7: 
        case CPT_AV_8: 
        case CPT_AV_9: ci_msg(1, "Corel Photo-Paint "); break;
        default: ci_msg(1, "Unknown [!]");
    }
    switch (ci.flag_lo) {
        case CPT_AV_7: ci_msg(1, "7.0"); break;
        case CPT_AV_8: ci_msg(1, "8.0"); break;
        case CPT_AV_9: ci_msg(1, "9.0+"); break;
    }
    ci_msg(1, "\n");

    // --- Color depth detection  
    ci_msg(1, "CPT color model: ");
    switch (ci_f_header->color_model) {
        case CPT_BW1    : ci_msg(1, "1-bit black&white"); ci_msg(4, " BW1"); break;
        case CPT_GRAY8  : ci_msg(1, "8-bit grayscale"); ci_msg(4, " GRAY8"); break;
        case CPT_RGB8   : ci_msg(1, "8-bit paletted"); ci_msg(4, " PAL8"); break;
        case CPT_GRAY16 : ci_msg(1, "16-bit grayscale"); ci_msg(4, " GRAY16"); break;
        case CPT_RGB24  : ci_msg(1, "24-bit RGB"); ci_msg(4, " RGB24"); break;
        case CPT_LAB24  : ci_msg(1, "24-bit Lab"); ci_msg(4, " LAB24"); break;
        case CPT_CMYK32 : ci_msg(1, "32-bit CMYK"); ci_msg(4, " CMYK32"); break;
        case CPT_RGB48  : ci_msg(1, "48-bit RGB"); ci_msg(4, " RGB48"); break;
        default: ci_msg(1, "unknown!"); ci_msg(4, "UNK!");
    }
    ci_msg(1, "\n");

    // --- DPI resolution detection
    ci.xdpi = lround((double) (ci_f_header->xdpi) * cpt_dpi_scale);
    ci.ydpi = lround((double) (ci_f_header->ydpi) * cpt_dpi_scale);
    // Mask file detection
    if (!ci.xdpi && !ci.ydpi) {
        is_mask = 1;
    } else {
        // PhotoPaint doesn't allow res < 10 and > 10000 DPI
        if ((ci.xdpi < CPT_DPI_MIN || ci.xdpi > CPT_DPI_MAX) ||
            (ci.ydpi < CPT_DPI_MIN || ci.ydpi > CPT_DPI_MAX)) {
            dpi_warn = 1;
        }
    }
    // Normal mode
    ci_msg(1, "CPT resolution: %ux%u DPI", ci.xdpi, ci.ydpi);
    if (is_mask) ci_msg(1, " (mask)");
    if (dpi_warn) ci_msg(1, ci_msg_wnmark);
    ci_msg(1, "\n");
    // Short mode
    ci_msg(4," %ux%u%s", ci.xdpi, ci.ydpi, (dpi_warn ? "!" : ""));
    
    // --- Flags
    ci.emb_wcomment = ci_f_header->flags & CPT_EMB_WIDE_COMMENT;
    ci.emb_icc = ci_f_header->flags & CPT_EMB_ICC_PROFILE;
    // Normal mode / short mode
    ci_msg(1,"CPT has embedded wide comment: ");
    ci_msg(1, "%s\n", (ci.emb_wcomment ? ci_msg_yes : ci_msg_no));
    ci_msg(1,"CPT has embedded ICC profile: ");
    ci_msg(1, "%s\n", (ci.emb_icc ? ci_msg_yes : ci_msg_no));
    ci_msg(1,"CPT flags value: 0x%02x 0x%02x", ci.flag_hi, ci.flag_lo);
    if ((ci_f_header->flags & CPT_UNKNOWN_FILE_FLAGS)) ci_msg(1,ci_msg_wnmark);
    ci_msg(1,"\n");
    ci_msg(4, " %c", (ci.emb_icc ? 'y' : 'n'));
    ci_msg(4, " %c", (ci.emb_wcomment ? 'y' : 'n'));
    ci_msg(4," %02x %02x", ci.flag_hi, ci.flag_lo);

    // ****** 'After Header' data ******
    ci.wcomment_offs = CPT_FileHeader_sz;
    
    // --- ICC part
    if (!CPT_ICC_ALLOWED(ci_f_header->color_model) && ci.emb_icc) {
        ci_msg(1, "%s ICC embedded bit set, but color model doesn't allow ICC data!\n", ci_error_str);
        ci_msg(12, " iccbit!");
        exit(EXIT_FAILURE);
    }
    if (ci.emb_icc) {
        // It seems to be the first 'after header' block
        ci_icc = (CPT_ICC *) (ci_data + CPT_FileHeader_sz);
        if (ci_icc->magic == CPT_ICC_MAGIC) {
            // If magic ok, increase comment offset
            ci.wcomment_offs += CPT_ICC_sz;
            // Print some data
            ci_msg(1, "CPT ICC profile data type: ");
            switch (ci_icc->type) {
                case CPT_ICC_EMBEDDED: ci_msg(1, "embedded"); break;
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7: ci_msg(1, "%s", cpt_internal_icc_type[ci_icc->type]); break;
                default: ci_msg(1, "unknown%s", ci_msg_wnmark);
            }
            ci_msg(1,"\n");
            if (ci_icc->type == CPT_ICC_EMBEDDED) {
                // TODO: check len if not too big / too small
                ci_msg(1, "CPT ICC profile file size: %u bytes\n", ci_icc->len);
                // Increase comment offset by length of embedded file
                ci.wcomment_offs += ci_icc->len;
            }
            ci_msg(1, "CPT ICC unknown vars: 0x%08x 0x%08x 0x%08x\n",
                ci_icc->unk[0], ci_icc->unk[1], ci_icc->unk[2]);
        } else {
            ci_msg(1, "%s ICC magic incorrect!\n", ci_error_str);
            ci_msg(12, " iccmagic!");
            exit(EXIT_FAILURE);
        }
    }
    // ICC dumping
    if (ci_cfg.dump_icc) {
        if (!ci.emb_icc) {
            ci_msg(1,"%s image doesn't have ICC profile, file not dumped!\n", ci_warning_str);
        } else if (ci_icc->type != CPT_ICC_EMBEDDED) {
            ci_msg(1,"%s image has internal ICC or unknown magic, file not dumped!\n", ci_warning_str);
        } else {
            // TODO: check len if not too big / too small
            sprintf(ci_tempname, "%s.icc", ci_basename);
            FILE *w = fopen(ci_tempname, "wb");
            fwrite(&ci_icc->data, 1, ci_icc->len, w);
            fclose(w);
        }
    }
        
    // --- Number of palette colors (for 8-bit paletted images)
    ci.pal_entries = 0; // just in case
    if (ci_f_header->color_model == CPT_RGB8) {
        // Check if number of entries is proper
	pal_warn |= (ci_f_header->palette_entries < 3 || ci_f_header->palette_entries > 768);
	pal_warn |= (ci_f_header->palette_entries % 3);
	ci.pal_entries = ci_f_header->palette_entries / 3;
        ci_palette = (CPT_Palette *) (ci_data + CPT_FileHeader_sz);
        // Increase wide comment offset
        ci.wcomment_offs += ci_f_header->palette_entries;
    }
    // Palette dumping
    if (ci_cfg.dump_palette) {
        if (ci_f_header->color_model != CPT_RGB8) {
            ci_msg(1,"%s image type not 8-bit paletted, not dumping palette!\n", ci_warning_str);
        } else if (!ci.pal_entries) {
            ci_msg(1,"%s palette entries number is 0, not dumping palette!\n", ci_warning_str);
        } else {
            if (pal_warn) ci_msg(1, "%s strange number of palette entries, dumping anyway...\n", ci_warning_str);
            sprintf(ci_tempname, "%s.pal", ci_basename);
            FILE *w = fopen(ci_tempname, "wb");
            fwrite(ci_palette, CPT_RGB_sz, ci.pal_entries, w);
            fclose(w);
        }
    }
    
    // Print color entries number anyway
    ci_msg(1, "CPT palette entries number: ");
    ci_msg(1, "%u color(s)%s\n", ci.pal_entries, (pal_warn ? ci_msg_wnmark : ""));
    ci_msg(4, " %u%s", ci.pal_entries, (pal_warn ? "!" : ""));
    
    // --- File comment if present
    ci_wcomment = (CPT_WideComment *) (ci_data + ci.wcomment_offs);
    // acomment
    if (*ci_f_header->notes) {
        gchar *com_ansi = g_convert(ci_f_header->notes, CPT_NOTE_LEN_A, ci_charset, ci_cfg.charset, NULL, NULL, NULL);
        ci_msg(1, "CPT comment (ANSI): ");
        if (com_ansi) { ci_msg(1, "%s\n", com_ansi); g_free(com_ansi); }
        else ci_msg(1, "[conv failed]\n"); 
        // wcomment
        if (ci_wcomment->magic == CPT_WIDE_COMMENT_MAGIC) {
            gchar *com_wide = g_convert((gchar *)&ci_wcomment->notes, CPT_NOTE_LEN_W, ci_charset, CPT_WIDE_CHARSET, NULL, NULL, NULL);
            ci_msg(1, "CPT comment (UCS-2): ");
            if (com_wide) { ci_msg(1, "%s\n", com_wide); g_free(com_wide); }
            else ci_msg(1, "[conv failed]\n"); 
        }
    }
    // If number of colors incorrect, stop. Not stopping here
    // could break up block table offset calculation for CPT7
    if (pal_warn) {
        ci_msg(1, "%s Palette entries number is incorrect!\n", ci_error_str);
        ci_msg(4, " palnum!");
        exit(EXIT_FAILURE);
    }
    
    // --- Block table position
    ci.blocks_table_offs = ci_f_header->blocks_table_offs;      // block table offset read from .cpt

    // But let's calculate it anyway (for safety)
    ci_blocks_table_offs_eval =
        CPT_FileHeader_sz +
        ci_f_header->palette_entries +  // may be 0
        (ci.emb_wcomment && ci_wcomment->magic == CPT_WIDE_COMMENT_MAGIC ? CPT_WideComment_sz : 0);

    if (CI_CPTVER_78(ci.version)) {
        // CPT7 offset table = always 0, workaround this case. However, files
        // saved by PhotoPaint 9 as CPT7 files have this field non-zero.
        if (ci.blocks_table_offs) bt_cpt9 = 1;
        else ci.blocks_table_offs = ci_blocks_table_offs_eval;
    } else {
        // If address is earlier than header and 'after header' data, it's an error
        if (ci.blocks_table_offs < ci_blocks_table_offs_eval) bt_warn = 1;
        // Address has to be smaller than filesize minus size of 1 entry
        if (ci.blocks_table_offs > ci_filesize - CPT9_Block_sz) bt_warn = 1;
    }
    ci_msg(1, "CPT block table offset: 0x%08x%s%s\n", ci.blocks_table_offs, (bt_warn?ci_msg_wnmark:""), (bt_cpt9?ci_msg_9mark:""));
    ci_msg(4, " %x%s", ci.blocks_table_offs,(bt_warn?" !":""));

    if (bt_warn) {
        ci_msg(1, "%s Incorrect block table offset!\n", ci_error_str);
        exit(EXIT_FAILURE);
    }
    // BIG [TODO] check the 'lthm' case - are the following offsets relative?
    // Hope so. Calculate the memory address. 
    ci_blocks_table = (CPT_BlockTableEntry *) (ci_data + ci.blocks_table_offs);
    // --- Blocks number
    ci.blocks_num = ci_f_header->blocks_num;
    if (!ci.blocks_num) bn_warn = 1;
    if (!ci_blocks_table[0].offs) bn_warn = 1;
    // Number of blocks from header should be equal with real number of blocks
    if (ci.blocks_num * CPT_BlockTableEntry_sz + ci.blocks_table_offs != ci_blocks_table[0].offs) bn_warn = 1;
    ci_msg(1, "CPT blocks number: %u%s\n", ci.blocks_num, (bn_warn ? ci_msg_wnmark:""));
    if (bn_warn) {
        ci_msg(4," !");
        ci_msg(1, "%s Block number from header doesn't equal real block number!\n", ci_error_str);
        exit(EXIT_FAILURE);
    } else {
        ci_msg(4," %u", ci.blocks_num);
    }

    // --- Unknown fields
    if (ci_f_header->unk00 != 0x00010000) {
        // This is what Corel Photo-Paint does
        ci_f_header->unk00 = 0x00010000;
        unk_warn[0] = 1;
    }
    ci_msg(1,"CPT unknown field 00: 0x%08x (%u)", ci_f_header->unk00, ci_f_header->unk00);
    if (unk_warn[0]) { ci_msg(1, "%s\n", ci_msg_wnmark); ci_msg(4," !"); }
    else { ci_msg(1,"\n"); ci_msg(4," %08x", ci_f_header->unk00); }
    
    // --- Reserved fields, 0 always
    if (ci_f_header->reserved00[0] || ci_f_header->reserved00[1]) res_warn[0] = 1;
    if (ci_f_header->reserved01[0] || ci_f_header->reserved01[1]) res_warn[1] = 1;
    if (ci_f_header->reserved02) res_warn[2] = 1;
    if (ci_cfg.output_reserved || res_warn[0]) {
        ci_msg(1,"CPT reserved 00: 0x%08x 0x%08x", ci_f_header->reserved00[0], ci_f_header->reserved00[1]);
        if (res_warn[0]) ci_msg(1,ci_msg_wnmark); ci_msg(1,"\n");
    }
    if (ci_cfg.output_reserved || res_warn[1]) {
        ci_msg(1,"CPT reserved 01: 0x%08x 0x%08x", ci_f_header->reserved01[0], ci_f_header->reserved01[1]);
        if (res_warn[1]) ci_msg(1,ci_msg_wnmark); ci_msg(1,"\n");
    }
    if (ci_cfg.output_reserved || res_warn[3]) {
        ci_msg(1,"CPT reserved 02: 0x%08x 0x%08x", ci_f_header->reserved02, ci_f_header->reserved02);
        if (res_warn[2]) ci_msg(1,ci_msg_wnmark); ci_msg(1,"\n");
    }
    if (!ci_f_header->reserved00[0]) ci_msg(4," 0"); else ci_msg(4," !");
    if (!ci_f_header->reserved00[1]) ci_msg(4," 0"); else ci_msg(4," !");
    if (!ci_f_header->reserved01[0]) ci_msg(4," 0"); else ci_msg(4," !");
    if (!ci_f_header->reserved01[1]) ci_msg(4," 0"); else ci_msg(4," !");
    if (!ci_f_header->reserved02) ci_msg(4," 0"); else ci_msg(4," !");
}


// Check if a chunk is a chunk ;-)
uint32_t ci_IsChunk(uint32_t chunk) {
    uint32_t i;
    for (i=0; i < CPT9_CHUNK_NUM; i++) {
        if (chunk == cpt9_chunk_name[i].id) return 1;
    }
    return 0;
}

// TODO: verify calculations precision
void ci_ProcessChunkGrid(uint8_t *buf, uint32_t len) {
    CPT9_CGrid *grid = (CPT9_CGrid *) buf;
    ci_msg(3,"%sGrid density:", ci_msg_chunk_var_tab);
    double gridx, gridy;
    gridx = cpt9_grid_table[grid->xunit]*grid->xdensity;
    if (grid->xunit == CPT9_GRID_UNIT_PIXEL) gridx *= ci.xdpi;
    gridy = cpt9_grid_table[grid->yunit]*grid->ydensity;
    if (grid->yunit == CPT9_GRID_UNIT_PIXEL) gridy *= ci.ydpi;

    ci_msg(3," %.4f ", gridx);
    switch (grid->xunit) {
        case CPT9_GRID_UNIT_INCH: ci_msg(3, "inch"); break;
        case CPT9_GRID_UNIT_MM: ci_msg(3, "mm"); break;
        case CPT9_GRID_UNIT_PICA_POINT: ci_msg(3, "pica;point"); break;
        case CPT9_GRID_UNIT_POINT: ci_msg(3, "point"); break;
        case CPT9_GRID_UNIT_CM: ci_msg(3, "cm"); break;
        case CPT9_GRID_UNIT_PIXEL: ci_msg(3, "pixel"); break;
        case CPT9_GRID_UNIT_CICERO_DIDOT: ci_msg(3, "cicero;didot"); break;
        case CPT9_GRID_UNIT_DIDOT: ci_msg(3, "didot"); break;
        default: ci_msg(3, "unknown [!]"); break;
    }
    ci_msg(3," /");
    ci_msg(3," %.4f ", gridy);
    switch (grid->yunit) {
        case CPT9_GRID_UNIT_INCH: ci_msg(3, "inch"); break;
        case CPT9_GRID_UNIT_MM: ci_msg(3, "mm"); break;
        case CPT9_GRID_UNIT_PICA_POINT: ci_msg(3, "pica;point"); break;
        case CPT9_GRID_UNIT_POINT: ci_msg(3, "point"); break;
        case CPT9_GRID_UNIT_CM: ci_msg(3, "cm"); break;
        case CPT9_GRID_UNIT_PIXEL: ci_msg(3, "pixel"); break;
        case CPT9_GRID_UNIT_CICERO_DIDOT: ci_msg(3, "cicero;didot"); break;
        case CPT9_GRID_UNIT_DIDOT: ci_msg(3, "didot"); break;
        default: ci_msg(3, "unknown [!]"); break;
    }
    ci_msg(3,"\n");
    ci_msg(3,"%sUnknown var 00: %08x %08x (%u %u)\n",
        ci_msg_chunk_var_tab,
        grid->unk00[0],grid->unk00[1],
        grid->unk00[0],grid->unk00[1]
    );
    ci_msg(3,"%sUnknown var 01: %u %u %u %u %u %u %u %u\n",
        ci_msg_chunk_var_tab,
        grid->unk01[0],grid->unk01[1],grid->unk01[2],grid->unk01[3],
        grid->unk01[4],grid->unk01[5],grid->unk01[6],grid->unk01[7]
    );

}

// 'path'
// FIXME: len is wrong, should be some constant probably
void ci_ProcessChunkPath(uint8_t *buf, uint32_t len) {
    CPT9_CPath *path = (CPT9_CPath *) buf;
    char *name = (char *)&path->name;
    gchar *name_ansi = g_convert(name, len, ci_charset, ci_cfg.charset, NULL, NULL, NULL);
    ci_msg(3,"%sPath name ANSI: ", ci_msg_chunk_var_tab);
    if (name_ansi) { ci_msg(3, "%s\n", name_ansi); g_free(name_ansi); }
    else ci_msg(3, "[conv failed]\n"); 
    ci_msg(3,"%sUnknown var 00..04: %d %d %d %d %d\n",
        ci_msg_chunk_var_tab,
        path->unk00,path->unk01,path->unk02,path->unk03,path->unk04
    );
}

// 'pthw'
// Here len is OK, because pthw contains UCS-2 string only
void ci_ProcessChunkPthw(uint8_t *buf, uint32_t len) {
    CPT9_CPthw *path = (CPT9_CPthw *) buf;
    char *name = (char *)&path->name;
    gchar *name_ucs = g_convert(name, len, ci_charset, CPT_WIDE_CHARSET, NULL, NULL, NULL);
    ci_msg(3,"%sPath name UCS-2: ", ci_msg_chunk_var_tab);
    if (name_ucs) { ci_msg(3, "%s\n", name_ucs); g_free(name_ucs); }
    else ci_msg(3, "[conv failed]\n"); 
}


// 'bnam'
void ci_ProcessChunkBnam(uint8_t *buf, uint32_t len) {
    char *name = (char *) buf;
    gchar *name_ansi = g_convert(name, len, ci_charset, ci_cfg.charset, NULL, NULL, NULL);
    ci_msg(3,"%sBackground name ANSI: ", ci_msg_chunk_var_tab);
    if (name_ansi) { ci_msg(3, "%s\n", name_ansi); g_free(name_ansi); }
    else ci_msg(3, "[conv failed]\n"); 
}

// 'bnwm'
void ci_ProcessChunkBnwm(uint8_t *buf, uint32_t len) {
    char *name = (char *) buf;
    gchar *name_ucs = g_convert(name, len, ci_charset, CPT_WIDE_CHARSET, NULL, NULL, NULL);
    ci_msg(3,"%sBackground name UCS-2: ", ci_msg_chunk_var_tab);
    if (name_ucs) { ci_msg(3, "%s\n", name_ucs); g_free(name_ucs); }
    else ci_msg(3, "[conv failed]\n"); 
}

// 'oinf'
void ci_ProcessChunkOinf(uint8_t *buf, uint32_t len) {
    CPT9_COinf *oinf = (CPT9_COinf *) buf;
    char *name_a = (char *)&oinf->name_a;
    char *name_w = (char *)&oinf->name_w;
    gchar *name_ucs = g_convert(name_w, CPT9_OINF_NAME_LEN_W, ci_charset, CPT_WIDE_CHARSET, NULL, NULL, NULL);
    gchar *name_ansi = g_convert(name_a, CPT9_OINF_NAME_LEN_A, ci_charset, ci_cfg.charset, NULL, NULL, NULL);
    ci_msg(3,"%sObject name ANSI: ", ci_msg_chunk_var_tab);
    if (name_ansi) { ci_msg(3, "%s\n", name_ansi); g_free(name_ansi); }
    else ci_msg(3, "[conv failed]\n"); 
    ci_msg(3,"%sObject name UCS-2: ", ci_msg_chunk_var_tab);
    if (name_ucs) { ci_msg(3, "%s\n", name_ucs); g_free(name_ucs); }
    else ci_msg(3, "[conv failed]\n"); 

    ci_msg(3,"%sUnknown var 00: %d %d %d %d %d %d\n",
        ci_msg_chunk_var_tab,
        oinf->unk00[0],oinf->unk00[1],oinf->unk00[2],oinf->unk00[3],oinf->unk00[4],oinf->unk00[5]
    );
    ci_msg(3,"%sUnknown var 01: %d %d %d %d %d %d\n",
        ci_msg_chunk_var_tab,
        oinf->unk01[0],oinf->unk01[1],oinf->unk01[2],oinf->unk01[3],oinf->unk01[4],oinf->unk01[5]
    );
    ci_msg(3,"%sUnknown var 02: %d %d %d %d %d %d %d\n",
        ci_msg_chunk_var_tab,
        oinf->unk02[0],oinf->unk02[1],oinf->unk02[2],oinf->unk02[3],oinf->unk02[4],oinf->unk02[5]
    );

}



// Verbose output is pretty readable. Short output:
// bpp sizex sizey | unknown dwords
void ci_ProcessBlock9(uint32_t offs, uint32_t size, uint32_t id) {
    uint32_t offset, val, chnk, len=0, i;
    uint8_t *buf = (uint8_t *) (ci_data + offs);
    CPT9_Block *block = (CPT9_Block *) (ci_data + offs);
    if (!ci_cfg.verbose && ci_cfg.silent_header) printf(" | ");
//    ci_msg(8, " | ");

    // Block header info
    ci_msg(1,"[*] BLOCK %04x @ 0x%08x (%u bytes)\n", id, offs, size);
    ci_msg(1,"    Block dimensions: %ux%u pixels\n", block->width, block->height);
    ci_msg(1,"    [?] Tile dimensions: %ux%u pixels\n", block->tile_w, block->tile_h);
    ci_msg(1,"    Bits per pixel: %u bpp\n", block->bpp);
    ci_msg(1,"    Unknown field 00: 0x%08x (%u)\n", block->unk00, block->unk00);
    ci_msg(1,"    Unknown field 01: 0x%08x (%u)\n", block->unk01, block->unk01);
    ci_msg(1,"    Unknown field 02: 0x%08x (%u)%s\n",
        block->unk02, block->unk02, (block->unk02==1 ? " [object]" : ""));
    ci_msg(1,"    [?] Chunk area size: %u bytes\n", block->size1);
    ci_msg(1,"    Palette data size: %u bytes\n", block->pal_size);
    ci_msg(1,"    Unknown field 03[5]: %u %u %u %u %u\n",
        block->unk03[0], block->unk03[1], block->unk03[2], block->unk03[3], block->unk03[4]);
    ci_msg(8,"%u %u %u %u %u %u %u %u %u %u %u %u %u %u %u",
        block->width, block->height, block->tile_w, block->tile_h, block->bpp,
        block->unk00, block->unk01, block->unk02, block->size1, block->pal_size,
        block->unk03[0], block->unk03[1], block->unk03[2], block->unk03[3], block->unk03[4]
    );

    // Find chunks in block
    uint32_t chunk_area_size = 0;
    
    if (ci_cfg.output_chunks) ci_msg(8," |");
    // If it's non-zero, let's try to read chunk info
    uint32_t area_size;
    uint32_t area_unk;  // notice: == block->unk01 (?)
    if (block->size1) {
        area_size = GETu32(buf, CPT9_Block_sz);
        area_unk = GETu32(buf, CPT9_Block_sz+4);
        if (ci_cfg.output_chunks) {
            ci_msg(1,"    Chunk table size (block info/area info+pal_size): %u/%u\n", block->size1, area_size+block->pal_size);
            ci_msg(1,"    Chunk table unknown variable: %u (%08x) \n", area_unk, area_unk);
            ci_msg(8," %u %u", area_size, area_unk);
        }
        // It's propably by design, not an error
        if (block->size1 != area_size+block->pal_size) {
            ci_msg(1,"%s Chunk table size differ, ", ci_warning_str);
            if (ci_cfg.output_chunks) ci_msg(1,"see above!");
            else ci_msg(1,"use "CI_ARG_OUTPUT_CHUNK" option for more details!");
            ci_msg(1," Using area info.\n");
            ci_msg(8," chk_sz0!");
        }
        // Chunk area is something like this:
        // uint32_t asize
        // uint32_t unk (always 1)
        // An then:
        // uint32_t chunk_len;    
        // uint32_t chunk_id;
        // uint8_t data[chunk_len];
        // ...
        chunk_area_size = 8;
        for (i=0, offset=CPT9_Block_sz+8; offset < size; offset+=len+8, i++) {
            // OK, this was propably last chunk, don't go beyond
            // TODO: I put this check here, because I've met situations:
            // block->size1 == 0 -> direct skip to data, but also:
            // block->size1 == 8 -> we search for chunks, but 8 bytes is too small for any
            if (chunk_area_size == area_size) {
                if (ci_cfg.output_chunks)
                    ci_msg(1, "    [--] END of chunks (%u found, data follows @ 0x%08x)\n", i, CPT9_Block_sz + area_size);
                break;
            }

            len = GETu32(buf, offset);      // get chunk len
            chnk = GETu32(buf, offset+4);   // get chunk id
            // If len == 0 something's not OK
            if (!len) {
                ci_msg(1, "%s Chunk corrupt?! (len=0)\n", ci_error_str);
                ci_msg(4, " chk_len0!");
                exit(EXIT_FAILURE);
            }
            // Add to chunk area size for checking
            chunk_area_size += len + 8;
            // If chunk id found in our table, it's fine
            if (ci_cfg.output_chunks) {
                if (ci_IsChunk(chnk)) {
                    ci_msg(1, "    [**] CHUNK: '%s' @ 0x%08x (%u=%u+8 bytes)\n", ci_Ascii32(chnk), offset, len+8, len);
                    ci_msg(10, " %s", ci_Ascii32(chnk));
                } else { // Whoa, what's this then? New type chunk? :-)
                    ci_msg(1, "    [**] ?????: '%s' @ 0x%08x (%u=%u+8 bytes)\n", ci_Ascii32(chnk), offset, len+8, len);
                    ci_msg(10, " ????");
                }
                uint8_t *chnk_offs = buf+offset+8;
                switch (chnk) {
                    case CPT9_CHUNK_GRID: ci_ProcessChunkGrid(chnk_offs, len); break;
                    case CPT9_CHUNK_BNAM: ci_ProcessChunkBnam(chnk_offs, len); break;
                    case CPT9_CHUNK_BNWM: ci_ProcessChunkBnwm(chnk_offs, len); break;
                    case CPT9_CHUNK_PATH: ci_ProcessChunkPath(chnk_offs, len); break;
                    case CPT9_CHUNK_PTHW: ci_ProcessChunkPthw(chnk_offs, len); break;
                    case CPT9_CHUNK_OINF: ci_ProcessChunkOinf(chnk_offs, len); break;
                }
            }
        }
    } else {
        if (ci_cfg.output_chunks) {
            ci_msg(1,"    Chunk table size is 0, skipping...\n");
            ci_msg(8," 0 ?");
        }
    }
    
    // If there were any chunks, we skipped them now
    offset = CPT9_Block_sz + block->size1;
    // We should be at data offset now. However, let's check for sure
    // if it's not a chunk in case of some pathological files
    if (offset+16 <= size) {
        chnk = GETu32(buf, offset+12);   // get 'chunk' id
        if (ci_IsChunk(chnk)) {
            ci_msg(1, "%s Something's wrong, size1==0 but chunk found!\n", ci_error_str);
            ci_msg(8, " chk_fnd!");
            exit(EXIT_FAILURE);
        }
    }

    // The data area seems to be build like this:
    // uint32_t phys_offset1;
    // uint32_t len1;
    // uint32_t phys_offset2;
    // uint32_t len2;
    // ...
    // uint32_t marker; // @phys_offset1; optional
    // uint8_t *data;
    // Marker may indicate type of compression; values:
    // 0, 1, 4, 5, 0x00030005, but also no marker
    if (ci_cfg.output_data) {
        ci_msg(8, " |");
        uint32_t pair[3], val;
        uint32_t data_start = GETu32(buf, offset);
        uint32_t printed[3];
        printed[0] = 0;
        printed[1] = 0;
        printed[2] = 0;
        for (i=0; buf+offset+i < ci_data + data_start; i+=8) {
            pair[0] = GETu32(buf, offset+i);
            pair[1] = GETu32(buf, offset+i+4);
            val = GETu32(ci_data, pair[0]);
            ci_msg(1, "    [**] 0x%08x (% 5u bytes): 0x%08x\n", pair[0], pair[1], val);
            ci_msg(10, " %08x %08x",pair[0], pair[1]);
            switch (val) {
                case 0x00000004: if (!printed[0]) { printed[0] = 1; ci_msg(8, " %08x", val); } break;
                case 0x00000005: if (!printed[1]) { printed[1] = 1; ci_msg(8, " %08x", val); } break;
                case 0x00030005: if (!printed[2]) { printed[2] = 1; ci_msg(8, " %08x", val); } break;
//                    ci_msg(10, " %08x", val);
                default: ci_msg(8, " %08x", val);
            }
        }
        ci_msg(1,"    [--] END of list (%u element(s))", i>>3);
        ci_msg(10," %u", i>>3, pair[2]);
        // Check if there may be more offsets and warn
        if (offset+i+12 <= size) {
            pair[2] = GETu32(buf, offset+i+8);
            if (pair[0]+pair[1] == pair[2]) {
                ci_msg(1, ci_msg_wnmark);
                ci_msg(8, "!");
            }
        }
        ci_msg(1, "\n");
    }
    if (!ci_cfg.verbose && !ci_cfg.silent_header) printf("\n");
}


// When calling this function we assume following variables are correct:
//      * ci.blocks_num == number of blocks
//      * ci_blocks_table == pointer to table of blocks.
void ci_ProcessFileBlocks(void) {
    uint32_t i, size;
    FILE *w;
    // --- Blocks dumping ---
    if (ci_cfg.dump_blocks) {
        // Directory, 1(.) + 1(/) + basename + 7'.blocks' + \0
        char *dirname = (char *) malloc(10+strlen(ci_basename));
        // Directory + file 1(.)+1(/)+basename+1(/)+basename+4(.ext)+
        char *pathname = (char *) malloc(16+2*strlen(ci_basename));
        sprintf(dirname, ".%c%s.blocks", ci_path_separator, ci_basename);
        mkdir(dirname, 0755);
        // Process all blocks
        for (i=0; i < ci.blocks_num; i++) {
            sprintf(pathname, "%s%c%s.%04x", dirname, ci_path_separator, ci_basename, i);
            w = fopen(pathname, "wb");
            // Size of block = difference between next offset and current,
            // with exception of last block (file size - current offset)
            size = (i < ci.blocks_num-1 ?
                ci_blocks_table[i+1].offs - ci_blocks_table[i].offs :
                ci_filesize - ci_blocks_table[i].offs
            );
            fwrite(ci_data + ci_blocks_table[i].offs, 1, size, w);
            fclose(w);
        }
        free(pathname);
        free(dirname);
    }

    // If user specified a range of blocks using '-br'...
    if (ci_cfg.block_range) {
        // Check if ranges are sane
        if (ci_cfg.block_1st > ci.blocks_num-1) 
            ci_cfg.block_1st = ci.blocks_num-1;
        if (ci_cfg.block_last > ci.blocks_num-1) 
            ci_cfg.block_last = ci.blocks_num-1;
        ci_msg(1,"Specified "CI_ARG_BLOCK_RANGE" option, scanning block");
        if (ci_cfg.block_1st == ci_cfg.block_last) ci_msg(1," %u", ci_cfg.block_1st);
        else ci_msg(1, "s %u-%u", ci_cfg.block_1st, ci_cfg.block_last);
        ci_msg(1, "...\n");
    } else {
        ci_cfg.block_1st = 0;
        ci_cfg.block_last = ci.blocks_num-1;
    }
    // Process all, or just given blocks
    for (i=ci_cfg.block_1st; i <= ci_cfg.block_last; i++) {
        size = (i < ci.blocks_num-1 ? ci_blocks_table[i+1].offs - ci_blocks_table[i].offs : ci_filesize - ci_blocks_table[i].offs );
        switch (ci.version) {
            case 0x700:
            case 0x701:         
            case 0x800: break;  // TODO - CPT78 ci_ProcessBlock ?
            case 0x900: ci_ProcessBlock9(ci_blocks_table[i].offs, size, i); break;
        }
    }
}

void ci_AtExit(void) {
    if (!ci_cfg.verbose && ci_cfg.silent_header) printf("\n");
//    ci_msg(8, "\n");
    if (f) fclose(f);
    free(ci_filename_short__);
    free(ci_tempname);
    free(ci_basename);
    free(ci_data);
}


int main(int argc, char *argv[]) {
    uint32_t i, j;

    // Default parameters - verbosity configuration
    ci_cfg.verbose = 1;
    ci_cfg.verbose2 = 0;
    ci_cfg.silent_header = 1;
    ci_cfg.silent_blocks = 1;
    ci_cfg.verbosity_level = 0; // standard: clean
    ci_cfg.charset = (char *)&ci_default_charset_a;  // assumed .cpt charset

    // Set CPTInfo locale charset to system locale charset
    setlocale(LC_CTYPE, "");
    g_get_charset(&ci_charset);
#ifdef WIN32
    SetConsoleOutputCP(atoi(ci_charset+2));
#endif
    setlocale(LC_CTYPE, "C");
    
    // Initialization, processing of command line
    ci_ProcessArguments(argc, argv);
    atexit(ci_AtExit);

    // Some info
    ci_msg(1, ci_msg_welcome);
    ci_msg(3, "Command line:");
    for (i=0; i < argc; i++) ci_msg(3," %s", argv[i]);
    ci_msg(3,"\n");

    // Actual data reading handling
    ci_ReadFileContents();
    ci_ProcessFileHeader();
    ci_ProcessFileBlocks();

    return EXIT_SUCCESS;

}

