
/******************************************************************************
 ******************************************************************************/

/** \file hash.h
 *  modular hash library
 *
 * $Id: hash.h 16 2006-03-14 16:51:22Z shotgun $
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

#ifndef __HASH_H__
#define __HASH_H__

/******************************************************************************
 * I N C L U D E S ************************************************************
 ******************************************************************************/

#include <stdarg.h>

/******************************************************************************
 * T Y P E D E F S ************************************************************
 ******************************************************************************/

/* hash table type */
typedef struct hash hash_t;

/* label function prototypes */
typedef void* hash_new_t(int*, va_list);
typedef int hash_key_t(int, int*, va_list);
typedef int hash_cmp_t(int, void*, va_list);

/* prototype to free value */
typedef void hash_free_t(void*);

/* type of hash table */
typedef struct
{
  hash_new_t *create;
  hash_key_t *hashkey;
  hash_cmp_t *compare;
} hash_type_t;

/******************************************************************************
 * P R O T O T Y P E S ********************************************************
 ******************************************************************************/

/* create and destroy hashes */
hash_t* hash_create(int siz, hash_type_t *type, hash_free_t *free);
void hash_free(hash_t *hash);

/* insert and retrieve values */
void* hash_get(hash_t *hash, ...);
int hash_set(hash_t *hash, void *value, ...);

/* remove hash values */
void hash_rm(hash_t *hash, ...);
void hash_reset(hash_t *hash);

/******************************************************************************/

#endif /* end __HASH_H__ */

/******************************************************************************
 ******************************************************************************/

