/*
 * $Id: as_meta.h,v 1.8 2004/10/23 09:23:50 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_META_H
#define __AS_META_H

/*****************************************************************************/

/* Note that there is no realm 'unknown'. Ares does not share files which
 * don't fall into the below categories. The file extension used by
 * Ares Lite 1.8.1.2944 are:
 *
 * Archive:  .zip .tar .gz .rar .cab .sit .msi .ace .hqx .iso .nrg .rmp .rv
 *           .cif .img .rip .cue .bin .fla .swf .dwg .dxf .wsz .nes .md5
 *
 * Audio:    .mp3 .vqf .wma .wav .voc .mod .ra .ram .mid .au .ogg .mp2 .mpc
 *           .flac .shn .ape
 *
 * Software: .exe .msi .vbs .pif .bat .com .scr
 *
 * Video:    .avi .mpeg .asf .mov .fli .flc .lsf .wm .qt .viv .vivo .mpg
 *           .mpe .mpa .rm .wmv .divx .m1v .mkv .ogm
 *
 * Document: .doc .rtf .pdf .ppt .wri .txt .hlp .lit .book .pps .ps
 *
 * Image:    .gif .jpg .jpeg .bmp .psd .psp .tga .tif .tiff .png
 *
 * Due to a bug in Ares files with truncated extensions of the above are also
 * categorized under the same realm given that the total extension length
 * inluding the leading dot is at least 3 chars. E.g. ".ex" will be
 * categorized as software an ".mp" will be categorized as audio sonce ".mp3"
 * is checked before ".mpeg".
 */

typedef enum
{
	REALM_ARCHIVE  = 0,
	REALM_AUDIO    = 1,
	REALM_SOFTWARE = 3,
	REALM_VIDEO    = 5,
	REALM_DOCUMENT = 6,
	REALM_IMAGE    = 7,

	REALM_UNKNOWN  = 0xFFFF /* not used on the ares network */
} ASRealm;

typedef enum
{
	TAG_TITLE  = 1,
	TAG_ARTIST = 2,
	TAG_ALBUM  = 3,
	TAG_XXX    = 4, /* depends on realm */
	TAG_UNKSTR = 5, /* some unknown string, maybe "comment" */
	TAG_YEAR   = 6,
	TAG_CODEC  = 7,
	TAG_KEYWORDS = 15, /* verify */
	TAG_FILENAME = 16
} ASTagType;

typedef struct
{
	char *name;					/* human readable tag name */
	char *value;				/* human readable tag value */

} ASMetaTag;

typedef struct
{
	List *tags; /* list of ASMetaTag */

} ASMeta;

typedef struct
{
	const char *name;
	ASTagType   type;
	as_bool     tokenize;
} ASTagMapping;

typedef as_bool (*ASMetaForeachFunc) (ASMetaTag *tag, void *udata);

/*****************************************************************************/

/* create meta data object */
ASMeta *as_meta_create ();

/* create copy of meta data object */
ASMeta *as_meta_copy (ASMeta *meta);

/* create meta data object from search result packet */
ASMeta *as_meta_parse_result (ASPacket *packet, ASRealm realm);

/* free meta data object */
void as_meta_free (ASMeta *meta);

/*****************************************************************************/

/* add gift style meta tag or update if already present */
as_bool as_meta_add_tag (ASMeta *meta, const char *name, const char *value);

/* get gift style meta tag data */
const char *as_meta_get_tag (ASMeta *meta, const char *name);

/* remove gift style meta tag */
as_bool as_meta_remove_tag (ASMeta *meta, const char *name);

/* Call func for each tag. Returns number of times func returned TRUE. */
int as_meta_foreach_tag (ASMeta *meta, ASMetaForeachFunc func, void *udata);

/* wrapper around as_meta_get_tag */
int as_meta_get_int (ASMeta *meta, const char *name);

const ASTagMapping *as_meta_tag_name (const char *name);
const ASTagMapping *as_meta_tag_type (ASTagType type);

/*****************************************************************************/

/* Returns realm based on file extension exactly as Ares does. */
ASRealm as_meta_realm_from_filename (const char *path);

/*****************************************************************************/

#endif /* __AS_META_H */
