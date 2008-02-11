 /* 
  * CPTInfo - Corel PhotoPaint file information tool.
  * Copyright (c) 2006-2008 Jakub Argasiński (argasek@gmail.com).
  * 
  * CPTInfo CPT header and other helpful structures.
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

#ifndef _CPT_H_
#define _CPT_H_

#include <inttypes.h>

 /* 
  * Convention used (applies in the same way to lowercase names):
  *   - CPT9 refers to CPT9+ specific data.
  *   - CPT7 refers to CPT7 only specific data.
  *   - CPT refers to CPT7+ specific data.
  *   - CI means data helpful for CPTInfo application.
  * Letter case convention:
  *   - CPT_TheName is a type defined structure.
  *   - CPT_TheName_sz is a size of structure CPT_TheName.
  *   - CPT_THE_NAME means #define constant.
  *   - CPT_the_name is a new type.
  *   - cpt_the_name is a variable/named constant.
  */

#define CPT_VERSIONS_NUM        3               // number of known .cpt versions
#define CPT_DPI_MIN             10              // minimum allowed DPI 
#define CPT_DPI_MAX             10000           // maximum allowed DPI
#define CPT_WIDE_CHARSET        "UCS-2LE"       // encoding used for wide chars
#define CPT_NOTE_LEN_A           256             // max size of anote in bytes
#define CPT_NOTE_LEN_W           512             // max size of anote*2 in bytes
#define CPT_WIDE_COMMENT_MAGIC  0x0204          // wide comment block magic

// Helper macro to test if color model can have ICC information applied
#define CPT_ICC_ALLOWED(x)      ((x==CPT_RGB24)||(x==CPT_CMYK32)||(x==CPT_LAB24)||(x==CPT_RGB48))
#define CPT_ICC_MAGIC           0x5A            // ICC block magic
#define CPT_ICC_EMBEDDED        0xFFFFFFFF      // type val if ICC embedded as file
#define CPT_ICC_INTERNAL_NUM    8               // how many internal types of ICC

// Corel PHOTO-PAINT 9.0+ constants
#define CPT9_CHUNK_NUM          50              // CPT9 known chunk types number

#define CPT9_OINF_NAME_LEN_A     112             // Object info ansi name bytes
#define CPT9_OINF_NAME_LEN_W     128             // Object info wide name bytes

typedef uint16_t CPT_wchar;      // UCS-2 encoded character

// Available color models
enum {
    CPT_RGB24   = 0x01,
    CPT_CMYK32  = 0x03,
    CPT_GRAY8   = 0x05,
    CPT_BW1     = 0x06,
    CPT_RGB8    = 0x0A,
    CPT_LAB24   = 0x0B,
    CPT_RGB48   = 0x0C,
    CPT_GRAY16  = 0x0E
} CPT_ColorModels;

// Measure units
enum {
    CPT9_GRID_UNIT_INCH         = 0x01,
    CPT9_GRID_UNIT_MM           = 0x02,
    CPT9_GRID_UNIT_PICA_POINT   = 0x03,
    CPT9_GRID_UNIT_POINT        = 0x04,
    CPT9_GRID_UNIT_CM           = 0x05,
    CPT9_GRID_UNIT_PIXEL        = 0x06,
    CPT9_GRID_UNIT_CICERO_DIDOT = 0x0C,
    CPT9_GRID_UNIT_DIDOT        = 0x0D
} CPT9_GridUnit;

// CPT Application Versions
enum {
    CPT_AV_7                    = 0x01,
    CPT_AV_8                    = 0x8C,
    CPT_AV_9                    = 0x94
} CPT_AppVersions;

// Available FileHeader flags
enum {
    CPT_EMB_ICC_PROFILE     = 0x0100,       // file has embedded ICC profile
    CPT_EMB_WIDE_COMMENT    = 0x0200,       // file has embedded comment
    CPT_VERSION_7_01_MASK   = 0x00F0,       // is it 7.01 version?
    CPT_VERSION_7_01        = 0x0090        // it's 7.01 version
} CPT_FileHeaderFlags;

// Unknown flags helper
#define CPT_UNKNOWN_FILE_FLAGS 0xFFFF0000

// CPT single palette entry (BGR color triplet)
typedef struct _CPT_RGB {
    uint8_t     b;
    uint8_t     g;
    uint8_t     r;
} CPT_RGB;

// CPT palette structure
typedef struct _CPT_Palette {
    CPT_RGB    *color;
} CPT_Palette;

// CPT .cpt file header
typedef struct _CPT_FileHeader {
    uint8_t 	magic[8];	        // 0x000 [8] - OK!
    uint32_t 	color_model;	    // 0x008 [4] - OK!
    uint32_t 	palette_entries;    // 0x00C [4] - OK!
    uint32_t 	reserved00[2];	    // 0x010 [8], 0 always
    uint32_t 	xdpi;		        // 0x018 [4] - OK!
    uint32_t 	ydpi;		        // 0x01C [4] - OK!
    uint32_t 	reserved01[2];	    // 0x020 [8], 0 always
    uint32_t 	blocks_num;	        // 0x028 [4] - OK!
    uint32_t    unk00;              // 0x02C [4], always 0x00010000, bigger values = out of mem
    uint32_t    flags;              // 0x030 [4], 0x0000|94/01/8c|00-03
    uint32_t    blocks_table_offs;  // 0x034 [4] - OK!
    uint32_t    reserved02;        	// 0x038 [4], 0 always
    char        notes[CPT_NOTE_LEN_A];  // 0x03C [256] - OK!
} CPT_FileHeader;

// CPT .icc block
typedef struct _CPT_ICC {
    uint32_t    magic;
    uint32_t    type;
    uint32_t    len;
    uint32_t    unk[3];                 // probably internal profile data
    uint8_t     *data;
    // ... and here goes the data
} CPT_ICC;

// CPT wide comment block
typedef struct _CPT_WideComment {
    uint32_t    magic;
    char        notes[CPT_NOTE_LEN_W];
} CPT_WideComment;

// Entry in the blocks offset table.
// FIXME: maybe these are 64-bit offsets?
typedef struct _CPT_BlockTableEntry {
    uint32_t	offs;			// 0x000 [4] - OK!
    uint32_t	reserved;		// 0x004 [4], 0 always, reserved?
} CPT_BlockTableEntry;

typedef struct _CPT9_Block {
    uint32_t    width;          // 0x000 [4] width
    uint32_t    height;         // 0x004 [4] height
    uint32_t    tile_w;         // 0x008 [4] tile width ?
    uint32_t    tile_h;         // 0x00C [4] tile height ?
    uint32_t    bpp;            // 0x010 [4] bits per pixel
    uint32_t    unk00;          // 0x014 [4] always 0
    uint32_t    unk01;          // 0x018 [4] 1, 8 etc.
    uint32_t    unk02;          // 0x01C [4] 0, 1 or 2. 1 == object?
    uint32_t    size1;          // 0x020 [4] ? chunk area size
    uint32_t    pal_size;       // 0x024 [4] ? palette data size
    uint32_t    unk03[5];       // 0x028 [5] always 0
} CPT9_Block;

// ---- Chunks ----

typedef struct _CPT9_CGrid {
    uint32_t    unk00[2];
    double      xdensity;
    double      ydensity;
    uint32_t    xunit;
    uint32_t    yunit;
    uint32_t    unk01[8];
} CPT9_CGrid;

typedef struct _CPT9_CPath {
    uint32_t    unk00;
    uint32_t    unk01;
    uint32_t    unk02;
    uint32_t    unk03;
    uint32_t    unk04;
    char        *name;
} CPT9_CPath;

typedef struct _CPT9_CPthw {
    char        *name;
} CPT9_CPthw;

typedef struct _CPT9_COinf {
    uint32_t    unk00[6];
    uint32_t    unk01[6];
    uint32_t    unk02[7];
    char        name_a[CPT9_OINF_NAME_LEN_A];
    char        name_w[CPT9_OINF_NAME_LEN_W];
} CPT9_COinf;


// CPT version structure
typedef struct _CPT_Version {
    const char *magic;                  // buffer
    uint8_t     len;                    // length of magic
    uint32_t    version;                // assigned version
} CPT_Version;

// CPT9 CPTInfo chunk structure
typedef struct _CPT9_ChunkName {
    uint32_t    id;                     // 32-bit identifier
    uint8_t     flags;                  // CPT9_CHUNK_FLAGS
} CPT9_ChunkName;


// ---------- Constant values ----------

// used in DPI calculations [FIXME - which one is correct?]
//const double cpt_dpi_scale = 25.4/1000000;
const double cpt_dpi_scale = 25.399986284007403/1000000;

// Corel PhotoPaint version table
const CPT_Version cpt_version[CPT_VERSIONS_NUM] = {
    { "CPT7FILE", 8, 0x0700 },          // Corel PhotoPaint 7.0-8.0
    { "CPT8FILE", 8, 0x0800 },          // Corel PhotoPaint 8.0
    { "CPT9FILE", 8, 0x0900 }           // Colre PhotoPaint 9.0-X3
};

const char *cpt_internal_icc_type[] = {
    "sRGB",
    "Fraser (1998)",
    "SMPTE-240M",
    "NTSC (1953)",
    "PAL",
    "SECAM",
    "Barco - D50",
    "Barco - D65"
};


// See CPT9_GridUnit
const double cpt9_grid_table[] = {
    0,                              // -- empty
    1.0/(25.4*10000),               // OK inch
    1.0/(1.0*10000),                // OK mm
    1.0/((25.4*10000)/6),           // OK pica;point
    1.0/((25.4*10000)/72),          // OK point
    1.0/(10.0*10000),               // OK cm
    1.0/(25.4*10000),               // OK pixel (multiply this one by DPI!!!)
    0,0,0,0,0,                      // -- empty
    // In 1973 cicero was standarized to be 4.5 mm or 0.177 inch
    // 27.07 mm is a French Inch, Corel uses value of 27.0712199999999982 mm
    // 27,070 05
//    1.0/(((0.376065*12.0))*10000),
    1.0/(4.5118699999999997*10000), // ?? cicero;didot

    1.0/(0.37591999999999998*10000) // ?? didot
};

#define CPT9_CHUNK_GRID 0x67726964
#define CPT9_CHUNK_BNAM 0x626e616d
#define CPT9_CHUNK_BNWM 0x626e776d
#define CPT9_CHUNK_PATH 0x70617468
#define CPT9_CHUNK_PTHW 0x70746877
#define CPT9_CHUNK_OINF 0x6f696e66

// CPT9 CPTInfo known chunk list
const CPT9_ChunkName cpt9_chunk_name[CPT9_CHUNK_NUM] = {  
    { 0x61657874, 0x02 }, // "aext"
    { 0x616e6177, 0x02 }, // "anaw"
    { 0x616e616d, 0x02 }, // "anam"
    { 0x616f7672, 0x02 }, // "aovr"
    { CPT9_CHUNK_BNAM, 0x02 }, // "bnam"
    { CPT9_CHUNK_BNWM, 0x02 }, // "bnwm"
    { 0x636c7061, 0x02 }, // "clpa"
    { 0x646f6373, 0x02 }, // "docs"
    { 0x64756f74, 0x02 }, // "duot"
    { CPT9_CHUNK_GRID, 0x02 }, // "grid"
    { 0x67756964, 0x02 }, // "guid"
    { 0x69736772, 0x04 }, // "isgr"
    { 0x6c726573, 0x02 }, // "lres"
    { 0x6c74686d, 0x02 }, // "lthm"
    { 0x6e6d7061, 0x02 }, // "nmpa"
    { 0x6e6f7a7a, 0x02 }, // "nozz"
    { 0x6e757061, 0x02 }, // "nupa"
    { 0x6f626c6e, 0x02 }, // "obln"
    { 0x6f64756f, 0x02 }, // "oduo"
    { CPT9_CHUNK_OINF, 0x02 }, // "oinf"
    { 0x6f6c6578, 0x02 }, // "olex"
    { 0x6f6c6e73, 0x02 }, // "olns"
    { 0x6f736477, 0x02 }, // "osdw"
    { 0x6f743130, 0x02 }, // "ot10"
    { 0x6f743132, 0x02 }, // "ot12"
    { 0x6f74686d, 0x02 }, // "othm"
    { 0x6f747070, 0x02 }, // "otpp"
    { 0x6f747839, 0x02 }, // "otx9"
    { 0x6f747874, 0x02 }, // "otxt"
    { 0x726f6964, 0x02 }, // "roid"
    { CPT9_CHUNK_PATH, 0x02 }, // "path"
    { 0x70736470, 0x02 }, // "psdp"
    { CPT9_CHUNK_PTHW, 0x02 }, // "pthw"
    { 0x70746878, 0x02 }, // "pthx"
    { 0x74677061, 0x02 }, // "tgpa"
    { 0x7469746c, 0x02 }, // "titl"
    { 0x75726c61, 0x02 }, // "urla"
    { 0x75726c63, 0x02 }, // "urlc"
    { 0x75726c73, 0x02 }, // "urls"
    { 0x75726c74, 0x02 }, // "urlt"
    { 0x75727761, 0x02 }, // "urwa"
    { 0x75727763, 0x02 }, // "urwc"
    { 0x75727773, 0x02 }, // "urws"
    { 0x75727774, 0x02 }, // "urwt"
    { 0x76626169, 0x02 }, // "vbai"
    { 0x76626178, 0x02 }, // "vbax"
    { 0x76696163, 0x02 }, // "viac"
    { 0x76726873, 0x02 }, // "vrhs"
    { 0x76736574, 0x02 }, // "vset"
    { 0x776b7061, 0x02 }  // "wkpa"
};



// Structure/constans sizes
#define CPT_RGB_sz              sizeof(CPT_RGB)
#define CPT_BlockTableEntry_sz  sizeof(CPT_BlockTableEntry)
#define CPT9_Block_sz           sizeof(CPT9_Block)
#define CPT_FileHeader_sz       sizeof(CPT_FileHeader)
#define CPT_WideComment_sz      sizeof(CPT_WideComment)
#define CPT_ICC_sz              (sizeof(CPT_ICC)-sizeof(uint8_t *))

#endif
