
/******************************************************************************
 ******************************************************************************/

/** \file t_strkey.c
 *  test strkey hash routines
 *
 * $Id: t_strkey.c 11 2006-01-23 04:38:30Z shotgun $
 */

/******************************************************************************
 * I N C L U D E S ************************************************************
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <hash.h>
#include <strkey.h>

/******************************************************************************
 * M A C R O S ****************************************************************
 ******************************************************************************/

#define errkill(e,m...)		{fprintf(stderr, m); fputc('\n', stderr);\
				fflush(stderr); return(e);}

#define testinfo(m...)		{printf(m); printf("..."); fflush(stdout);}
#define passed()		printf("ok\n");

/******************************************************************************
 * M A I N ********************************************************************
 ******************************************************************************/

int main(int argc, char **argv)
{
  char buf[1024];
  char *string[1024];
  char *ptr, *chkstr;
  int loop, ctr = 0;
  hash_t *hash;


  testinfo("creating hash table");
  if(!(hash = hash_create(0, strkey, free)))
    errkill(-1, "failed (%s)", strerror(errno));
  passed();

  /* load strings */
  printf("\nloading strings from stdin:\n");
  while(fgets(buf, sizeof(buf), stdin))
    {
    printf("reading %i:", ctr);
    fflush(stdout);

    /* chomp newline */
    ptr = ((buf + strlen(buf)) - 1);
    if((ptr >= buf) && (*ptr == '\n'))
      *ptr = 0;

    if(!ctr && !(chkstr = strdup(buf)))
      errkill(-1, "error loading strings");
    if(!(string[ctr++] = strdup(buf)))
      errkill(-1, "error loading strings");
    printf("[%s]\n", string[ctr - 1]);
    if(ctr >= 1024)
      break;
    }

  /* store strings in hash */
  printf("\nstoring strings in hash:\n");
  for(loop=0; loop<ctr; loop++)
    {
    testinfo("storing [%s]...", string[loop]);
    if(hash_set(hash, string[loop], string[loop]))
      errkill(-1, "failed");
    passed();
    }

  /* test values */
  printf("\nlooking up stored values:\n");
  for(loop=0; loop<ctr; loop++)
    {
    testinfo("testing [%s]", string[loop]);
    if(!(ptr = hash_get(hash, string[loop])))
      errkill(-1, "failed");
    if(strcmp(ptr, string[loop]))
      errkill(-1, "error");
    passed();
    }

  testinfo("\ntesting hash reset");
  hash_reset(hash);
  if(hash_get(hash, chkstr))
    errkill(-1, "reset failed");
  free(chkstr);
  passed();

  testinfo("\nfreeing hash table");
  hash_free(hash);
  passed();

  printf("\n-- all tests passed --\n\n");
  return(0);
}

/******************************************************************************
 ******************************************************************************/

