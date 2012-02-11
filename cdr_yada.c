
/******************************************************************************
 ******************************************************************************/

/** \file cdr_yada.c
 * yada call detail recording module for asterisk
 *
 * current version at http://oss.devit.com/matt/#cdr_yada
 *
 * $Id: cdr_yada.c 1036 2008-01-15 21:07:35Z grizz $
 */

/******************************************************************************
 * L I C E N S E **************************************************************
 ******************************************************************************/

/*
 * Copyright (c) 2004,2005 Matt Griswold, dev/IT - http://www.devit.com/
 *
 * cdr_yada is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * cdr_yada is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with cdr_yada; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/******************************************************************************
 * I N C L U D E S ************************************************************
 ******************************************************************************/

#include "asterisk.h"

//ASTERISK_FILE_VERSION(__FILE__, "$Rev: 1036 $")

#include <sys/types.h>

#include <asterisk/config.h>
#include <asterisk/options.h>
#include <asterisk/channel.h>
#include <asterisk/cdr.h>
#include <asterisk/module.h>
#include <asterisk/logger.h>
#include <asterisk/cli.h>
#include "asterisk.h"

#include <stdio.h>

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <yada.h>

/******************************************************************************
 * D E F I N E S **************************************************************
 ******************************************************************************/

#define AST_MODULE "cdr_yada"

#define DATE_FORMAT "%Y-%m-%d %T"
#define ERROR_THRESHOLD 5
#define CHUNK_SZ 512
#define UFC_MAX 16

#define DEFAULT_QUERY \
  "insert into %s "\
    "(calldate, clid, src, dst, dcontext, channel, dstchannel, lastapp, " \
    "lastdata, duration, billsec, disposition, amaflags, accountcode, "\
    "uniqueid, userfield) " \
  "values " \
    "('?s', ?v, ?v, ?v, ?v, ?v, ?v, ?v, ?v, ?d, ?d, ?v, ?d, ?v, ?v, ?v)"

#define UFP_QUERY "insert into %s (%s) values (%s)"
#define UFP_CLIST \
 "calldate, clid, src, dst, dcontext, channel, dstchannel, lastapp, " \
 "lastdata, duration, billsec, disposition, amaflags, accountcode, uniqueid"
#define UFP_VLIST \
 "'?s', ?v, ?v, ?v, ?v, ?v, ?v, ?v, ?v, ?d, ?d, ?v, ?d, ?v, ?v"

#ifndef BUILDREV
#  error BUILDREV is not defined
#endif

/******************************************************************************
 * M A C R O S ****************************************************************
 ******************************************************************************/

#define TO_STRING_(s)   #s
#define TO_STRING(s)    TO_STRING_(s)

#define errret(e,f...) { ast_log(LOG_ERROR, f); return(e); }
#define errjmp(f...) { ast_log(LOG_ERROR, f); goto Err; }

/******************************************************************************
 * G L O B A L S **************************************************************
 ******************************************************************************/

/******************************************************************************/
/* module info */

//static char *desc = "yada CDR module";
static char *name = "yada cdr";;
static char *config = "cdr_yada.conf";
static char *rev = "build " TO_STRING(BUILDREV) ": $Date: 2008-01-15 15:07:35 -0600 (Tue, 15 Jan 2008) $";

/******************************************************************************/
/* options */

static char *dbstr = 0, *user = 0, *pass = 0, *table = 0;
static char *query = DEFAULT_QUERY;
static char **queue = 0, *queue_file = 0;
static int queue_size = 0, file_playback = 1;

static yada_t *yada;
static yada_rc_t *stmt_ins;
static int opt_nodb = 0, connected = 0;

static time_t act_time = 0;
static int error_cnt = 0, log_cnt = 0, queue_cnt = 0;

AST_MUTEX_DEFINE_STATIC(cdr_yada_lock);

/******************************************************************************/
/* userfield parser */

#include <hash.h>
#include <bufkey.h>
int ufv_sz;
char **ufv = 0, *ufvp;
hash_t *ufp_h = 0;

/******************************************************************************/
/* cli command help */

static char cdr_yada_connect_help[] =
  "Usage: cdr yada connect\n"
  "       Make cdr_yada connect to the database\n";

static char cdr_yada_disconnect_help[] =
  "Usage: cdr yada disconnect\n"
  "       Disconnect cdr_yada from database\n";

static char cdr_yada_status_help[] =
  "Usage: cdr yada status\n"
  "       Show current status for cdr_yada\n";

/******************************************************************************
 * F U N C T I O N S **********************************************************
 ******************************************************************************/

/******************************************************************************/

static inline void cdr_yada_queue_log(char *qstr)
{
  int i;
  FILE *qf;


  if(queue_cnt < queue_size)
    {
    queue[queue_cnt++] = qstr;
    return;
    }

  /* try to flush queue to file */
  if(!queue_file)
    return;

  if(!(qf = fopen(queue_file, "a")))
    {
    ast_log(LOG_ERROR, "Failed to open queue_file %s: %s\n",
                       queue_file, strerror(errno));
    return;
    }

  /* print all records to file */
  for(i = 0; i < queue_cnt; i++)
    {
    fprintf(qf, "%s\n", queue[i]);
    free(queue[i]);
    }

  queue_cnt = 0;
  fprintf(qf, "%s\n", qstr);
  fclose(qf);
  return;
}

/******************************************************************************/
/** log queued records
 */

static inline void cdr_yada_process_queue()
{
  int i, new_cnt;
  int line_sz = CHUNK_SZ;
  char *line, *tmp;
  FILE *qf;


  /* process any cached records */
  new_cnt = 0;
  for(i = 0; i < queue_cnt; i++)
    {
    if(yada->execute(yada, queue[i], 0) == -1)
      {
      if(i != new_cnt)
        queue[new_cnt] = queue[i];

      new_cnt++;
      continue;
      }
    free(queue[i]);
    }

  queue_cnt = new_cnt;

  /* check for queue file and process */
  if(!file_playback || !queue_file)
    return;

  if(!(qf = fopen(queue_file, "r")))
    return;

  if(!(line = malloc(line_sz)))
    {
    ast_log(LOG_ERROR, "Failed to alloc for playback: %s\n", strerror(errno));
    fclose(qf);
    return;
    }

  /* unlink file now so we can write and overflow back out to it */
  unlink(queue_file);

  /* playback file line by line */
  while(fgets(line, line_sz, qf))
    {
    /* check for whole line */
    while(line[(i = strlen(line) - 1)] != '\n')
      {
      /* set i back to end of line */
      i++;

      /* try to grow line */
      if(!(tmp = realloc(line, (line_sz += CHUNK_SZ))))
        {
        ast_log(LOG_ERROR, "Failed to alloc for playback: %s\n",
                           strerror(errno));
        line_sz -= CHUNK_SZ;
        continue;
        }
      line = tmp;

      /* get rest of line */
      if(!fgets(&line[i], line_sz - i, qf))
        {
        ast_log(LOG_ERROR, "Error reading file: %s\n", strerror(errno));
        free(line);
        fclose(qf);
        return;
        }
      }

    /* empty line */
    if(!i)
      continue;

    /* if insert fails, requeue line */
    if(yada->execute(yada, line, 0) == -1)
      cdr_yada_queue_log(line);
    }

  free(line);
  fclose(qf);
}

/******************************************************************************/
/** check for database connection, connecting if able
 */

static inline int db_ping()
{
  if(yada && connected)
    return(1);

  if(opt_nodb)
    return(0);

  if(!yada)
    {
    /* init and connect to yada */
    if(!(yada = yada_init(dbstr, 0)))
      {
      ast_log(LOG_ERROR, "Failed to initialize yada: %s\n", strerror(errno));
      return(0);
      }

    /* prepare insert */
    if(query == DEFAULT_QUERY)
      stmt_ins = yada->ypreparef(yada, query, table);
    else
      stmt_ins = yada->yprepare(yada, query, 0);

    if(!stmt_ins)
      {
      ast_log(LOG_ERROR, "Failed to prepare insert: %s\n", yada->errmsg);
      yada->destroy(yada);
      yada = 0;
      return(0);
      }
    }

  /* connect and check for queued logs */
  if(!yada->connect(yada, user, pass))
    {
    ast_log(LOG_ERROR, "Failed to connect: %s\n", yada->errmsg);
    return(0);
    }

  connected = 1;
  act_time = time(0);

  cdr_yada_process_queue();
  return(1);
}

/******************************************************************************/
/** disconnect from database
 */

static inline void db_disco()
{
  if(!yada || !connected)
    return;

  /* try to insert any queued records */
  cdr_yada_process_queue();

  yada->disconnect(yada);
  connected = 0;
  act_time = time(0);
}

/******************************************************************************/
/** disconnect from database and destroy yada
 */

static inline void db_destroy()
{
  if(!yada)
    return;

  yada->destroy(yada);
  yada = 0;
}

/******************************************************************************/
/** cli to connect to database
 */

static int cdr_yada_connect(int fd, int argc, char *argv[])
{
  opt_nodb = 0;


  if(yada && connected)
    {
    ast_cli(fd, "cdr_yada: database allready connected\n");
    return(RESULT_SUCCESS);
    }

  if(!db_ping())
    {
    ast_cli(fd, "cdr_yada: unable to connect to database: %s\n", yada->errmsg);
    return(RESULT_FAILURE);
    }

  ast_cli(fd, "cdr_yada: database connected\n");
  return(RESULT_SUCCESS);
}

/******************************************************************************/
/** cli to disconnect to database
 */

static int cdr_yada_disconnect(int fd, int argc, char *argv[])
{
  opt_nodb = 1;


  if(!yada || !connected)
    {
    ast_cli(fd, "cdr_yada: database is not connected\n");
    return(RESULT_SUCCESS);
    }

  db_disco();

  ast_cli(fd, "cdr_yada: database disconnected\n");
  return(RESULT_SUCCESS);
}

/******************************************************************************/
/** cli to get status
 */

static int cdr_yada_status(int fd, int argc, char *argv[])
{
  int ctime = time(0) - act_time;

  
  ast_cli(fd, "cdr_yada %s\n", rev);

  if(connected)
    {
    ast_cli(fd, "Connected to '%s'", dbstr);
    if(user)
      ast_cli(fd, " as '%s'", user);
    }
  else
    ast_cli(fd, "Not connected");

  if(ctime > 86400)
    ast_cli(fd, " for %dd%dh%dm%ds.\n", ctime / 86400, (ctime % 86400) / 3600,
                                        (ctime % 3600) / 60, ctime % 60);
  else if(ctime > 3600)
    ast_cli(fd, " for %dh%dm%ds.\n", ctime / 3600, (ctime % 3600) / 60,
                                     ctime % 60);
  else if(ctime > 60)
    ast_cli(fd, " for %dm%ds.\n", ctime / 60, ctime % 60);
  else
    ast_cli(fd, " for %ds.\n", ctime);

  ast_cli(fd, "%d of %d records queued, %d errors\n", queue_cnt, queue_size,
                                                      error_cnt);
  ast_cli(fd, "queue_file is %s\n", queue_file ? queue_file : "not set");
  ast_cli(fd, "\n");
  return(RESULT_SUCCESS);
}

/******************************************************************************/
/** check error threshold
 */

static inline void ck_err_thresh()
{
  if(++error_cnt < ERROR_THRESHOLD)
    return;

  ast_log(LOG_NOTICE, "Error threshold reached, bouncing db connection\n");
  error_cnt = 0;
  db_disco();
}

/******************************************************************************/
/** grow ufv array to allow specified size more
 */

static inline int grow_ufv(size_t sz)
{
  int i;
  off_t off;
  char **new;


  if(!(new = realloc(ufv, (sz += ufv_sz + CHUNK_SZ))))
    return(0);

  if(ufv != new)
    {
    off = (char *)ufv - (char *)new;
    /* loop through array updating offsets */
    for(i=0; i<UFC_MAX; i++)
      if(new[i])
        new[i] -= off;

    ufv = new;
    }

  ufv_sz = sz;
  return(1);
}

/******************************************************************************/
/** parse userfield then try to log to database
 */

static char** parse_uf(char *userfield)
{
  int idx = -1, len;
  char *ptr, *start;
  char *ufvp;


  /* zero out index space */
  memset(ufv, 0, sizeof(char*) * UFC_MAX);
  ufvp = ((char *)ufv) + sizeof(char*) * UFC_MAX;

  for(start = ptr = userfield; *ptr; ptr++)
    {
    switch(*ptr)
      {
    case('|'):
      if(idx == -1)
        {
        start = ptr + 1;
        break;
        }
        
      /* check for size */
      len = ptr - start;
      if(len + 1 > ufv_sz - (ufvp - (char *)ufv))
        if(!grow_ufv(len + 1))
          errret(NULL, "Failed to alloc for userfield array");

      /* copy value to array and set offset */
      memcpy(ufvp, start, len);
      ufvp[len] = 0;
      ufv[idx] = ufvp;
      ufvp += len + 1;

      start = ptr + 1;
      idx = -1;
      break;

    case('='):
      /* get index of variable */
      if((idx = (int)hash_get(ufp_h, start, ptr - start) - 1) == -1)
        {
        ast_log(LOG_WARNING, "Skipping unknown variable starting at %s\n",
         start);
        continue;
        }

      /* update ph to ptr+1 */
      start = ptr + 1;
      break;
      }
    }

  /* get last var */
  if(idx != -1)
    {
    /* check for size */
    len = ptr - start;
    if(len + 1 > ufv_sz - (ufvp - (char *)ufv))
      if(!grow_ufv(len + 1))
        errret(NULL, "Failed to alloc for userfield array");

    /* copy value to array and set offset */
    memcpy(ufvp, start, len);
    ufvp[len] = 0;
    ufv[idx] = ufvp;
    }

  return(ufv);
}

/******************************************************************************/
/** parse userfield then try to log to database
 */

static int cdr_yada_ufp_log(struct ast_cdr *cdr)
{
  struct tm tm;
  static char timestr[128];
  char *qstr;


  ast_mutex_lock(&cdr_yada_lock);

  ast_localtime(&cdr->start.tv_sec, &tm, NULL);
  strftime(timestr, 128, DATE_FORMAT, &tm);

  /* parse userfield into vars */
  if(!parse_uf(cdr->userfield))
    {
    ast_log(LOG_ERROR, "error parsing userfield, skipping userfield vars");
    /* zero out index space */
    memset(ufv, 0, sizeof(char*) * UFC_MAX);
    }

  /* try to write to db */
  if(db_ping())
    {

    if(yada->execute(yada, stmt_ins,
         timestr, cdr->clid, cdr->src, cdr->dst, cdr->dcontext,
         cdr->channel, cdr->dstchannel, cdr->lastapp, cdr->lastdata,
         cdr->duration, cdr->billsec, ast_cdr_disp2str(cdr->disposition),
         cdr->amaflags, cdr->accountcode, cdr->uniqueid, ufv[0], ufv[1], ufv[2],
         ufv[3], ufv[4], ufv[5], ufv[6], ufv[7], ufv[8], ufv[9], ufv[10],
         ufv[11], ufv[12], ufv[13], ufv[14], ufv[15])
         != -1)
      {
      log_cnt++;
      error_cnt = 0;
      ast_mutex_unlock(&cdr_yada_lock);
      return(RESULT_SUCCESS);
      }

    ast_log(LOG_ERROR, "Failed to insert row: %s\n", yada->errmsg);
    ck_err_thresh();
    }

  /* return if we can't queue */
  if(queue_cnt >= queue_size && !queue_file)
    {
    ast_log(LOG_ERROR, "Lost CDR of %ld billsecs for '%s <%s>'\n",
                       cdr->billsec, cdr->accountcode, cdr->src);
    ast_mutex_unlock(&cdr_yada_lock);
    return(RESULT_FAILURE);
    }

  /* dump sql for queuing */
  if(!(qstr = yada->dumpexec(yada, 0, stmt_ins,
         timestr, cdr->clid, cdr->src, cdr->dst, cdr->dcontext,
         cdr->channel, cdr->dstchannel, cdr->lastapp, cdr->lastdata,
         cdr->duration, cdr->billsec, ast_cdr_disp2str(cdr->disposition),
         cdr->amaflags, cdr->accountcode, cdr->uniqueid, ufv[0], ufv[1], ufv[2],
         ufv[3], ufv[4], ufv[5], ufv[6], ufv[7], ufv[8], ufv[9], ufv[10],
         ufv[11], ufv[12], ufv[13], ufv[14], ufv[15])))
    {
    ast_log(LOG_ERROR, "Failed to dump log row: %s\n", yada->errmsg);
    ast_log(LOG_ERROR, "Lost CDR of %ld billsecs for '%s <%s>'\n",
                       cdr->billsec, cdr->accountcode, cdr->src);
    ck_err_thresh();
    ast_mutex_unlock(&cdr_yada_lock);
    return(RESULT_FAILURE);
    }

  cdr_yada_queue_log(qstr);

  ast_mutex_unlock(&cdr_yada_lock);
  return(RESULT_FAILURE);
}

/******************************************************************************/
/** try to log cdr to database, or queue
 */

static int cdr_yada_log(struct ast_cdr *cdr)
{
  struct tm tm;
  static char timestr[128];
  char *qstr;


  ast_mutex_lock(&cdr_yada_lock);

  ast_localtime(&cdr->start.tv_sec, &tm, NULL);
  strftime(timestr, 128, DATE_FORMAT, &tm);

  /* try to write to db */
  if(db_ping())
    {

    if(yada->execute(yada, stmt_ins,
         timestr, cdr->clid, cdr->src, cdr->dst, cdr->dcontext,
         cdr->channel, cdr->dstchannel, cdr->lastapp, cdr->lastdata,
         cdr->duration, cdr->billsec, ast_cdr_disp2str(cdr->disposition),
         cdr->amaflags, cdr->accountcode, cdr->uniqueid, cdr->userfield)
         != -1)
      {
      log_cnt++;
      error_cnt = 0;
      ast_mutex_unlock(&cdr_yada_lock);
      return(RESULT_SUCCESS);
      }

    ast_log(LOG_ERROR, "Failed to insert row: %s\n", yada->errmsg);
    ck_err_thresh();
    }

  /* return if we can't queue */
  if(queue_cnt >= queue_size && !queue_file)
    {
    ast_log(LOG_ERROR, "Lost CDR of %ld billsecs for '%s <%s>'\n",
                       cdr->billsec, cdr->accountcode, cdr->src);
    ast_mutex_unlock(&cdr_yada_lock);
    return(RESULT_FAILURE);
    }

  /* dump sql for queuing */
  if(!(qstr = yada->dumpexec(yada, 0, stmt_ins,
         timestr, cdr->clid, cdr->src, cdr->dst, cdr->dcontext,
         cdr->channel, cdr->dstchannel, cdr->lastapp, cdr->lastdata,
         cdr->duration, cdr->billsec, ast_cdr_disp2str(cdr->disposition),
         cdr->amaflags, cdr->accountcode, cdr->uniqueid, cdr->userfield)))
    {
    ast_log(LOG_ERROR, "Failed to dump log row: %s\n", yada->errmsg);
    ast_log(LOG_ERROR, "Lost CDR of %ld billsecs for '%s <%s>'\n",
                       cdr->billsec, cdr->accountcode, cdr->src);
    ck_err_thresh();
    ast_mutex_unlock(&cdr_yada_lock);
    return(RESULT_FAILURE);
    }

  cdr_yada_queue_log(qstr);

  ast_mutex_unlock(&cdr_yada_lock);
  return(RESULT_FAILURE);
}

/******************************************************************************/
/* cli definitions */

static struct ast_cli_entry cdr_yada_cli_connect =
{
  { "cdr", "yada", "connect", NULL },
  cdr_yada_connect,
  "Connect database from cdr_yada",
  cdr_yada_connect_help,
  NULL
};

static struct ast_cli_entry cdr_yada_cli_disconnect =
{
  { "cdr", "yada", "disconnect", NULL },
  cdr_yada_disconnect,
  "Disconnect database from cdr_yada",
  cdr_yada_disconnect_help,
  NULL
};

static struct ast_cli_entry cdr_yada_cli_status =
{
  { "cdr", "yada", "status", NULL },
  cdr_yada_status,
  "Show current status of cdr_yada",
  cdr_yada_status_help,
  NULL
};

/******************************************************************************/
/** loads userfield parser config
 */

static inline int ufp_init(struct ast_config *cfg)
{
  int i, idx, csz, vsz;
  char *new, ufc[6];
  char *clist = 0, *vlist = 0, *clp, *vlp;
  const char *colname;


  /* init column hash table */
  if(!(ufp_h = hash_create(0, bufkey, NULL)))
    errret(0, "Failed to create userfield hash: %s", strerror(errno));

  csz = vsz = CHUNK_SZ;
  if(!(clist = malloc(csz)))
    errjmp("Failed to alloc for column list: %s", strerror(errno));
  if(!(vlist = malloc(vsz)))
    errjmp("Failed to alloc for value list: %s", strerror(errno));

  /* set up initial columns */
  memcpy(clist, UFP_CLIST, sizeof(UFP_CLIST) - 1);
  clp = clist + sizeof(UFP_CLIST) - 1;

  memcpy(vlist, UFP_VLIST, sizeof(UFP_VLIST) - 1);
  vlp = vlist + sizeof(UFP_VLIST) - 1;

  /* init variable array */
  ufv_sz = CHUNK_SZ;
  if(!(ufv = malloc(ufv_sz)))
    errjmp("Failed to alloc for value array: %s", strerror(errno));

  /* index starts at 1 for hash use */
  idx = 1;
  for(i=0; i<UFC_MAX; i++)
    {
    sprintf(ufc, "ufc%d", i);

    if(!(colname = ast_variable_retrieve(cfg, ufc, "name")))
      continue;

    /* check length */
    if((clp - clist) + strlen(colname) >= csz)
      {
      if(!(new = realloc(clist, (csz += strlen(colname) + CHUNK_SZ))))
        errjmp("Failed to grow userfield column list: %s\n", strerror(errno));
      if(clist != new)
        {
        clp -= (clist - new);
        clist = new;
        }
      }

    clp += sprintf(clp, ",%s", colname);
    vlp += sprintf(vlp, ",?v");

    if(hash_set(ufp_h, (void*)idx++, colname, strlen(colname)))
      errjmp("Failed userfield hash set");
    }

  /* null terminate lists */
  *clp = 0;
  *vlp = 0;

  /* put query together - query, table, columns, variables, NULL */
  if(!(query = malloc(sizeof(UFP_QUERY) + strlen(table) + (clp - clist) +
   (vlp - vlist) + 1)))
    errjmp("Failed to alloc for userfield query\n");

  sprintf(query, UFP_QUERY, table, clist, vlist);
  free(clist);
  free(vlist);
  return(1);

Err:
  hash_free(ufp_h);
  free(clist);
  free(vlist);
  free(ufv);
  return(0);
}

/******************************************************************************/
/** frees used memory
 */

static inline void cdr_yada_free()
{
  free(dbstr);
  dbstr = 0;
  free(user);
  user = 0;
  free(pass);
  pass = 0;
  free(table);
  table = 0;
  free(queue);
  queue = 0;
  free(queue_file);
  queue_file = 0;

  hash_free(ufp_h);
  ufp_h = 0;
  free(ufv);
  ufv = 0;

  if(query != DEFAULT_QUERY)
    {
    free(query);
    query = DEFAULT_QUERY;
    }
}

/******************************************************************************/
/** init and load
 */

static int cdr_yada_load(void)
{
  int ufp_enabled = 0;
  struct ast_config *cfg;
  struct ast_variable *var;
  const char *tmp;
  ast_cdrbe logger;


  if(!(cfg = ast_config_load(config)))
    {
    ast_log(LOG_WARNING, "Aborting: failed to load config: %s\n", config);
    return(AST_MODULE_LOAD_DECLINE);
    }
  
  /* nothing in config */
  if(!(var = ast_variable_browse(cfg, "global")))
    errjmp("no global section in config\n");

  /* get db string */
  if(!(tmp = ast_variable_retrieve(cfg, "global", "dbstr")))
    errjmp("Aborting: config directive 'dbstr' missing\n");

  if(!(dbstr = strdup(tmp)))
    errjmp("Failed dbstr strdup\n");

  /* get username */
  if((tmp = ast_variable_retrieve(cfg, "global", "user")))
    if(!(user = strdup(tmp)))
      errjmp("Failed user strdup\n");

  /* get password */
  if((tmp = ast_variable_retrieve(cfg, "global", "pass")))
    if(!(pass = strdup(tmp)))
      errjmp("Failed pass strdup\n");

  /* get table */
  if((tmp = ast_variable_retrieve(cfg, "global", "table")))
    if(!(table = strdup(tmp)))
      errjmp("Failed table strdup\n");

  /* get full query string */
  if((tmp = ast_variable_retrieve(cfg, "global", "query")))
    if(!(query = strdup(tmp)))
      errjmp("Failed query strdup\n");

  /* get max queue size */
  if((tmp = ast_variable_retrieve(cfg, "global", "queue_size")))
    queue_size = atoi(tmp);

  /* get queue file */
  if((tmp = ast_variable_retrieve(cfg, "global", "queue_file")))
    if(!(queue_file = strdup(tmp)))
      errjmp("Failed queue_file strdup\n");

  /* playback of logfiles */
  if((tmp = ast_variable_retrieve(cfg, "global", "file_playback")))
    if(strlen(tmp) == 2 && tolower(tmp[0]) == 'n' && tolower(tmp[1]) == 'o')
      file_playback = 0;

  /* max queue size */
  if(queue_size)
    {
    if(!(queue = malloc(sizeof(char *) * queue_size)))
      errjmp("Failed queue_file strdup\n");
    }
  else
    ast_log(LOG_WARNING,
      "queue_size not set: records may be lost if database goes away\n");

  /* check userfield parsing */
  if((tmp = ast_variable_retrieve(cfg, "userfield_parse", "enabled")))
    {
    if(strlen(tmp) != 2 || !(tolower(tmp[0]) == 'n' && tolower(tmp[1]) == 'o'))
      ufp_enabled = 1;
    }

  if(!queue_file)
    ast_log(LOG_WARNING,
      "queue_file not set: records may be LOST if database goes away\n");

  if(ufp_enabled)
    {
    if(!table)
      errjmp("Aborting: table must be defined for userfield parsing\n");
    if(query != DEFAULT_QUERY)
      {
      ast_log(LOG_WARNING,
       "query directive ignored when userfield parsing is enabled\n");
      free(query);
      query = 0;
      }

    /* initialize userfield parser */
    if(!ufp_init(cfg))
      errjmp("Failed to init userfield parsing, aborting");

    logger = cdr_yada_ufp_log;
    }
  else
    {
    if(!table && query == DEFAULT_QUERY)
      errjmp("Aborting: neither table or query are defined\n");

    if(table && query != DEFAULT_QUERY)
      ast_log(LOG_WARNING, "query directive overrides table directive\n");

    logger = cdr_yada_log;
    }

  /* init and connect to yada */
  if(!db_ping() && !queue_file)
    errjmp("Failed to connect to database and no queue file specified\n");

  /* register module */
  if(ast_cdr_register(name, ast_module_info->description, logger))
    errjmp("Failed to register cdr_yada\n");

  /* register cli */
  if(ast_cli_register(&cdr_yada_cli_connect) ||
     ast_cli_register(&cdr_yada_cli_disconnect) ||
     ast_cli_register(&cdr_yada_cli_status))
    errjmp("Failed to register cli 'connect'\n");

  ast_config_destroy(cfg);
  return(RESULT_SUCCESS);

Err:
  ast_config_destroy(cfg);
  cdr_yada_free();
  db_destroy();
  return(RESULT_FAILURE);
}

/******************************************************************************/
/** free and unload
 */

int cdr_yada_unload(void)
{
  cdr_yada_free();

  /* destroy yada */
  db_destroy();

  ast_cli_unregister(&cdr_yada_cli_connect);
  ast_cli_unregister(&cdr_yada_cli_disconnect);
  ast_cli_unregister(&cdr_yada_cli_status);

  ast_cdr_unregister(name);
  return(RESULT_SUCCESS);
}

/******************************************************************************/
/** load module
 */

static int load_module(void)
{
  return(cdr_yada_load());
}

/******************************************************************************/
/** unload module
 */

static int unload_module(void)
{
  return(cdr_yada_unload());
}

/******************************************************************************/
/** reload module
 */

static int reload(void)
{
  cdr_yada_unload();
  return(cdr_yada_load());
}

/******************************************************************************/

AST_MODULE_INFO_STANDARD(ASTERISK_GPL_KEY, "yada CDR backend");

/******************************************************************************
 ******************************************************************************/

