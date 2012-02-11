
/******************************************************************************
 ******************************************************************************/

/** \file bufkey.c
 *  hash labels from buffers
 *
 * $Id: bufkey.c 16 2006-03-14 16:51:22Z shotgun $
 */

/******************************************************************************
 * L I C E N S E **************************************************************
 ******************************************************************************/

/*
 * Copyright (c) 2006 dev/IT - http://www.devit.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/******************************************************************************
 * I N C L U D E S ************************************************************
 ******************************************************************************/

#include <stdlib.h>
#include <string.h>

#include <hash.h>

/******************************************************************************
 * P R O T O T Y P E S ********************************************************
 ******************************************************************************/

static void* create(int *hint, va_list ap);
static int hashkey(int max, int *hint, va_list ap);
static int compare(int hint, void*, va_list ap);

/******************************************************************************
 * G L O B A L S **************************************************************
 ******************************************************************************/

/* hash type structure */
static hash_type_t type = {create, hashkey, compare};
hash_type_t *bufkey = &type;

/******************************************************************************
 * F U N C T I O N S **********************************************************
 ******************************************************************************/

/******************************************************************************/
/** create label
 * @return label pointer on success
 */

static void* create(int *hint, va_list ap)
{
  char *old, *new;


  old = va_arg(ap, char*);
  if(!(new = malloc(*hint = va_arg(ap, int))))
    return(NULL);
  return(memcpy(new, old, *hint));
}

/******************************************************************************/
/** generate hash key from string
 * @return hash key value
 */

static int hashkey(int max, int *hint, va_list ap)
{
  register char *buf, *end;
  register int key = 0, mix;


  buf = va_arg(ap, char*);
  end = buf + (*hint = va_arg(ap, int));
  for(; buf<end; buf++)
    {
    key = ((key << 4) + *buf);
    if((mix = (key & 0xF0000000)))
      key ^= (mix >> 24);
    key &= ~mix;
    }

  /* optimize for smaller keys */
  if(!(max & ~0xFFFF))
    {
    key ^= (key >> 16);
    if(!(max & ~0xFF))
      key ^= (key >> 8);
    }

  return(key & max);
}

/******************************************************************************/
/** compare string labels
 * @return result of comparison
 */

static int compare(int hint, void *label, va_list ap)
{
  return(memcmp((char*)label, va_arg(ap, char*), hint));
}

/******************************************************************************
 ******************************************************************************/

