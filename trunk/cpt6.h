 /* 
  * CPTInfo - Corel PhotoPaint file information tool.
  * Copyright (c) 2006-2008 Jakub Argasiński (argasek@gmail.com).
  * 
  * CPTInfo CPTv6 header and other helpful structures.
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
#ifndef _CPT6_H_
#define _CPT6_H_

#include <inttypes.h>

#define CPT6_MAGIC_sz       4
#define CPT6_VERSION_sz     21

// Corel PhotoPaint 6.0 version magic and version inside TIFF
const char      cpt6_magic[CPT6_MAGIC_sz]       = "\x49\x49\x2a\x00";
const char      cpt6_version[CPT6_VERSION_sz]   = "Corel PHOTO-PAINT 6.0";
const uint32_t  cpt6_version_offs               = 0x000000F;

#endif
