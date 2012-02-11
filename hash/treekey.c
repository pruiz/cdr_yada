
/******************************************************************************
 ******************************************************************************/

/** \file treekey.c
 *  hash labels from namespace/string pairs
 *
 * $Id: treekey.c 16 2006-03-14 16:51:22Z shotgun $
 */

/******************************************************************************
 * L I C E N S E **************************************************************
 ******************************************************************************/

/*
 * Copyright (c) 2004-2006 dev/IT - http://www.devit.com
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/******************************************************************************
 * I N C L U D E S ************************************************************
 ******************************************************************************/

#include <stdlib.h>
#include <string.h>

#include <hash.h>

/******************************************************************************
 * T Y P E D E F S ************************************************************
 ******************************************************************************/

/* namespace / string pair */
typedef struct
{
  int ns;
  char str[1];
} label_t;

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
hash_type_t *treekey = &type;

/******************************************************************************
 * F U N C T I O N S **********************************************************
 ******************************************************************************/

/******************************************************************************/
/** create label
 * @return label pointer on success
 */

static void* create(int *hint, va_list ap)
{
  char *str;
  int ns;
  label_t *label;


  ns = va_arg(ap, int);
  if(!(str = va_arg(ap, char*)))
    str = "";

  /* allocate space for label */
  *hint = strlen(str);
  if(!(label = malloc(sizeof(label_t) + *hint)))
    return(NULL);

  /* setup label */
  label->ns = ns;
  memcpy(label->str, str, ++(*hint));
  return(label);
}

/******************************************************************************/
/** generate hash key from namespace/string pair
 * @return hash key value
 */

static int hashkey(int max, int *hint, va_list ap)
{
  char *str;
  register char *src;
  register int key, mix, ctr = 0;


  key = va_arg(ap, int);
  if(!(str = va_arg(ap, char*)))
    str = "";

  for(src=str; *src; ctr++)
    {
    key = ((key << 4) + *src++);
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

  *hint = (ctr + 1);
  return(key & max);
}

/******************************************************************************/
/** compare namespace/string labels
 * @return result of comparison
 */

static int compare(int hint, void *label, va_list ap)
{
  char *str;
  int ns;


  ns = va_arg(ap, int);
  if(!(str = va_arg(ap, char*)))
    str = "";

  if(ns -= ((label_t*)label)->ns)
    return(ns);
  return(memcmp(((label_t*)label)->str, str, hint));
}

/******************************************************************************
 ******************************************************************************/

