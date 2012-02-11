
/******************************************************************************
 ******************************************************************************/

/** \file intkey.c
 *  hash labels from integer arrays
 *
 * $Id: intkey.c 16 2006-03-14 16:51:22Z shotgun $
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
hash_type_t *intkey = &type;

/******************************************************************************
 * F U N C T I O N S **********************************************************
 ******************************************************************************/

/******************************************************************************/
/** create label
 * @return label pointer on success
 */

static void* create(int *hint, va_list ap)
{
  int *ints;
  int loop;


  /* allocate space for ints */
  *hint = va_arg(ap, int);
  if(!(ints = malloc(*hint * sizeof(int))))
    return(NULL);

  /* setup ints */
  for(loop=0; loop<*hint; loop++)
    ints[loop] = va_arg(ap, int);
  return(ints);
}

/******************************************************************************/
/** generate hash key from array of integers
 * @return hash key value
 */

static int hashkey(int max, int *hint, va_list ap)
{
  register int key, mix, loop, ctr;


  key = ctr = va_arg(ap, int);
  for(loop=0; loop<ctr; loop++)
    {
    key = ((key << 4) + va_arg(ap, int));
    if((mix = (key & 0xF0000000)))
      key ^= (mix >> 24);
    key &= ~mix;
    }

  /* optimize for smaller keys */
  if(!(max & ~0xFFFF))
    {
    key ^= (key >> 16);
    if(~(max & ~0xFF))
      key ^= (key >> 8);
    }

  *hint = ctr;
  return(key & max);
}

/******************************************************************************/
/** compare integer array labels
 * @return result of comparison
 */

static int compare(int hint, void *label, va_list ap)
{
  register int loop, diff;


  /* check number of elements in array */
  if((diff = (hint - va_arg(ap, int))))
    return(diff);

  /* compare array elements */
  for(loop=0; loop<hint; loop++)
    diff += (((int*)label)[loop] - va_arg(ap, int));
  return(diff);
}

/******************************************************************************
 ******************************************************************************/

