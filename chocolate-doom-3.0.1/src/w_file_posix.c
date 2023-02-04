//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	WAD I/O functions.
//

#include "DoomPlayer.h"
#include "config.h"

#ifdef HAVE_MMAP

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "m_misc.h"
#include "w_file.h"
#include "z_zone.h"

typedef struct
{
    wad_file_t wad;
    int handle;
} posix_wad_file_t;

extern wad_file_class_t posix_wad_file;

#if 0
unsigned int GetFileLength(int handle)
{
    return lseek(handle, 0, SEEK_END);
}
#endif
   
static wad_file_t *W_POSIX_OpenFile(char *path)
{
    posix_wad_file_t *result;
    int handle;
    QSPI_DIRHEADER *dirhp = (QSPI_DIRHEADER *)QSPI_ADDR;
    FS_DIRENT *direp;
    int i;

    direp = dirhp->fs_direntry;
    handle = 0;

    if (dirhp->fs_magic != DOOMFS_MAGIC)
      return NULL;

    for (i = 0; i < dirhp->fs_fcount; i++)
    {
      if ((direp->fsize == 0xFFFFFFFF) || (direp->fsize == 0))
        break;
      debug_printf("%s, %d\n", direp->fname, direp->fsize);
      if (strcasecmp(direp->fname, path) == 0)
      {
        handle = 1;
        break;
      }
      direp++;
    }

    if (handle)
    {
      // Create a new posix_wad_file_t to hold the file handle.

      result = Z_Malloc(sizeof(posix_wad_file_t), PU_STATIC, 0);
      result->wad.file_class = &posix_wad_file;
      result->wad.length = direp->fsize;
      result->wad.path = M_StringDuplicate(path);
      result->handle = handle;

      // Try to map the file into memory with mmap:

      result->wad.mapped = (byte *)(QSPI_ADDR + direp->foffset);;

      return &result->wad;
    }
    return NULL;
}

static void W_POSIX_CloseFile(wad_file_t *wad)
{
    posix_wad_file_t *posix_wad;

    posix_wad = (posix_wad_file_t *) wad;

    // If mapped, unmap it.

    Z_Free(posix_wad);
}

// Read data from the specified position in the file into the 
// provided buffer.  Returns the number of bytes read.

size_t W_POSIX_Read(wad_file_t *wad, unsigned int offset,
                   void *buffer, size_t buffer_len)
{
    posix_wad_file_t *posix_wad;
    byte *byte_buffer;
    uint8_t *rp;

    posix_wad = (posix_wad_file_t *) wad;

    // Jump to the specified position in the file.

    rp = posix_wad->wad.mapped + offset;

    // Read into the buffer.

    byte_buffer = buffer;

    memcpy(byte_buffer, rp, buffer_len);

    return buffer_len;
}


wad_file_class_t posix_wad_file = 
{
    W_POSIX_OpenFile,
    W_POSIX_CloseFile,
    W_POSIX_Read,
};


#endif /* #ifdef HAVE_MMAP */

