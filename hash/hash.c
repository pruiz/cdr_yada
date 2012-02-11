
/******************************************************************************
 ******************************************************************************/

/** \file hash.c
 *  core hash routines
 *
 * $Id: hash.c 23 2006-09-10 15:33:09Z grizz $
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

#include "hash.h"

/******************************************************************************
 * D E F I N E S **************************************************************
 ******************************************************************************/

/* minimum hash size in bits */
#define HASH_MINBITS	8

/******************************************************************************
 * T Y P E D E F S ************************************************************
 ******************************************************************************/

/* single location of hash storage */
typedef struct hashent hashent_t;
struct hashent
{
  void *label;
  int hint;
  void *value;
  hashent_t *next;
};

/* hash table structure */
struct hash
{
  hashent_t **ent;
  int ents;
  int siz, max;
  hash_new_t *create;
  hash_key_t *hashkey;
  hash_cmp_t *compare;
  hash_free_t *free;
};

/******************************************************************************
 * F U N C T I O N S **********************************************************
 ******************************************************************************/

/******************************************************************************/
/** create hash structure
 * @return hash pointer on success
 */

hash_t* hash_create(int siz, hash_type_t *type, hash_free_t *free)
{
  int max;
  hash_t *hash;


  /* determine size in bits */
  if(siz < HASH_MINBITS)
    siz = HASH_MINBITS;
  else if(siz > (sizeof(int) << 3))
    siz = (sizeof(int) << 3);
  max = (~(unsigned)0 >> ((sizeof(int) << 3) - siz));

  /* allocate hash structure */
  if(!(hash = calloc(1, sizeof(hash_t))))
    return(NULL);
  if(!(hash->ent = calloc((max + 1), sizeof(hashent_t*))))
    {
    free(hash);
    return(NULL);
    }

  /* initialize hash structure */
  hash->siz = siz;
  hash->max = max;
  hash->create = type->create;
  hash->hashkey = type->hashkey;
  hash->compare = type->compare;
  hash->free = free;
  return(hash);
}

/******************************************************************************/
/** free hash created by hash_create
 */

void hash_free(hash_t *hash)
{
  if(!hash)
    return;

  hash_reset(hash);
  free(hash->ent);
  free(hash);
}

/******************************************************************************/
/** get value from hash
 * @return pointer to value on success
 */

void* hash_get(hash_t *hash, ...)
{
  int hint;
  hashent_t *ent;
  va_list ap;


  /* lookup entry */
  va_start(ap, hash);
  if(!(ent = hash->ent[hash->hashkey(hash->max, &hint, ap)]))
    return(NULL);
  va_end(ap);

  /* scan for match */
  for(; ent; ent=ent->next)
    {
    va_start(ap, hash);
    if((ent->hint == hint) && !hash->compare(hint, ent->label, ap))
      {
      va_end(ap);
      return(ent->value);
      }
    va_end(ap);
    }

  return(NULL);
}

/******************************************************************************/
/** insert value into hash
 * @return 0 on success
 */

int hash_set(hash_t *hash, void *value, ...)
{
  void *label;
  int key, hint;
  hashent_t *ent;
  va_list ap;


  /* check for existing entry */
  va_start(ap, value);
  ent = hash->ent[key = hash->hashkey(hash->max, &hint, ap)];
  va_end(ap);

  for(; ent; ent=ent->next)
    {
    va_start(ap, value);
    if((ent->hint == hint) && !hash->compare(hint, ent->label, ap))
      {
      /* replace existing value */
      if(hash->free)
        hash->free(ent->value);
      ent->value = value;
      va_end(ap);
      return(0);
      }
    va_end(ap);
    }

  /* create label and compute hint for new entry */
  va_start(ap, value);
  if(!(label = hash->create(&hint, ap)))
    {
    va_end(ap);
    return(-1);
    }
  va_end(ap);

  /* create new entry */
  if(!(ent = calloc(1, sizeof(hashent_t))))
    {
    free(label);
    return(-1);
    }

  /* populate new entry and link to front of list */
  ent->label = label;
  ent->hint = hint;
  ent->value = value;
  ent->next = hash->ent[key];

  /* insert into table */
  hash->ent[key] = ent;
  return(0);
}

/******************************************************************************/
/** remove hash entry
 */

void hash_rm(hash_t *hash, ...)
{
  int key, hint;
  hashent_t *ent, *prev = NULL;
  va_list ap;


  /* check for existing entry */
  va_start(ap, hash);
  ent = hash->ent[key = hash->hashkey(hash->max, &hint, ap)];
  va_end(ap);

  for(; ent; ent=ent->next)
    {
    va_start(ap, hash);
    if((ent->hint == hint) && !hash->compare(hint, ent->label, ap))
      {
      /* relink list */
      if(prev)
        prev->next = ent->next;
      else
        hash->ent[key] = ent->next;

      /* remove entry */
      free(ent->label);
      if(hash->free)
        hash->free(ent->value);
      free(ent);
      return;
      }
    va_end(ap);

    prev = ent;
    } /* foreach(list_entry) */
}

/******************************************************************************/
/** clear all hash entries
 */

void hash_reset(hash_t *hash)
{
  int loop;
  hashent_t *ent, *next;


  /* free each hash entry */
  for(loop=0; loop<hash->max; loop++)
    {
    /* free all entries in this list */
    for(ent=hash->ent[loop]; ent; ent=next)
      {
      next = ent->next;
      /* free associated data */
      free(ent->label);
      if(hash->free)
        hash->free(ent->value);
      free(ent);
      }

    /* mark the list as enpty */
    hash->ent[loop] = NULL;
    } /* foreach(hashent) */

  hash->ents = 0;
}

/******************************************************************************
 ******************************************************************************/

