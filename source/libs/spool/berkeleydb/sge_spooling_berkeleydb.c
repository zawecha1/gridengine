/*___INFO__MARK_BEGIN__*/
/*************************************************************************
 *
 *  The Contents of this file are made available subject to the terms of
 *  the Sun Industry Standards Source License Version 1.2
 *
 *  Sun Microsystems Inc., March, 2001
 *
 *
 *  Sun Industry Standards Source License Version 1.2
 *  =================================================
 *  The contents of this file are subject to the Sun Industry Standards
 *  Source License Version 1.2 (the "License"); You may not use this file
 *  except in compliance with the License. You may obtain a copy of the
 *  License at http://gridengine.sunsource.net/Gridengine_SISSL_license.html
 *
 *  Software provided under this License is provided on an "AS IS" basis,
 *  WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
 *  WITHOUT LIMITATION, WARRANTIES THAT THE SOFTWARE IS FREE OF DEFECTS,
 *  MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE, OR NON-INFRINGING.
 *  See the License for the specific provisions governing your rights and
 *  obligations concerning the Software.
 *
 *   The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 *
 *   Copyright: 2001 by Sun Microsystems, Inc.
 *
 *   All Rights Reserved.
 *
 ************************************************************************/
/*___INFO__MARK_END__*/                                   

#include <errno.h>
#include <string.h>
#include <time.h>

#include "sgermon.h"
#include "sge_log.h"

#include "sge_answer.h"
#include "sge_dstring.h"
#include "sge_profiling.h"
#include "sge_unistd.h"

#include "sge_object.h"

#include "sge_cqueue.h"
#include "sge_host.h"
#include "sge_job.h"
#include "sge_ja_task.h"
#include "sge_pe_task.h"
#include "sge_qinstance.h"

#include "spool/sge_spooling_utilities.h"
#include "spool/sge_spooling_database.h"

#include "msg_common.h"
#include "spool/msg_spoollib.h"
#include "spool/berkeleydb/msg_spoollib_berkeleydb.h"

#include "spool/berkeleydb/sge_bdb.h"
#include "spool/berkeleydb/sge_spooling_berkeleydb.h"

/* JG: TODO: the following defines should better be parameters to
 *           the berkeley db spooling
 */

/* how often will the transaction log be cleared */
#define BERKELEYDB_CLEAR_INTERVAL 300

/* how often will the database be checkpointed (cache written to disk) */
#define BERKELEYDB_CHECKPOINT_INTERVAL 60

static const char *spooling_method = "berkeleydb";

const char *get_spooling_method(void)
{
   return spooling_method;
}

/****** spool/berkeleydb/spool_berkeleydb_create_context() ********************
*  NAME
*     spool_berkeleydb_create_context() -- create a berkeleydb spooling context
*
*  SYNOPSIS
*     lListElem* 
*     spool_berkeleydb_create_context(lList **answer_list, const char *args)
*
*  FUNCTION
*     Create a spooling context for the berkeleydb spooling.
* 
*  INPUTS
*     lList **answer_list - to return error messages
*     int argc     - number of arguments in argv
*     char *argv[] - argument vector
*
*  RESULT
*     lListElem* - on success, the new spooling context, else NULL
*
*  SEE ALSO
*     spool/--Spooling
*     spool/berkeleydb/--BerkeleyDB-Spooling
*******************************************************************************/
lListElem *
spool_berkeleydb_create_context(lList **answer_list, const char *args)
{
   lListElem *context = NULL;

   DENTER(TOP_LAYER, "spool_berkeleydb_create_context");

   /* check input parameter (*/
   if (args == NULL) {
/*      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_ERROR, 
                              MSG_POSTGRES_INVALIDARGSTOCREATESPOOLINGCONTEXT); */
   } else {
      lListElem *rule, *type;
      struct bdb_info *info;
      
      /* create spooling context */
      context = spool_create_context(answer_list, "berkeleydb spooling");
      
      /* create rule and type for all objects spooled in the spool dir */
      rule = spool_context_create_rule(answer_list, context, 
                                       "default rule", 
                                       args,
                                       spool_berkeleydb_default_startup_func,
                                       spool_berkeleydb_default_shutdown_func,
                                       spool_berkeleydb_default_maintenance_func,
                                       spool_berkeleydb_trigger_func,
                                       spool_berkeleydb_transaction_func,
                                       spool_berkeleydb_default_list_func,
                                       spool_berkeleydb_default_read_func,
                                       spool_berkeleydb_default_write_func,
                                       spool_berkeleydb_default_delete_func,
                                       spool_default_validate_func,
                                       spool_default_validate_list_func);

      info = bdb_create(args);
      lSetRef(rule, SPR_clientdata, info);
      type = spool_context_create_type(answer_list, context, SGE_TYPE_ALL);
      spool_type_add_rule(answer_list, type, rule, true);
   }

   DEXIT;
   return context;
}

/****** spool/berkeleydb/spool_berkeleydb_default_startup_func() **************
*  NAME
*     spool_berkeleydb_default_startup_func() -- setup 
*
*  SYNOPSIS
*     bool 
*     spool_berkeleydb_default_startup_func(lList **answer_list, 
*                                         const char *args, bool check)
*
*  FUNCTION
*
*  INPUTS
*     lList **answer_list   - to return error messages
*     const lListElem *rule - the rule containing data necessary for
*                             the startup (e.g. path to the spool directory)
*     bool check            - check the spooling database
*
*  RESULT
*     bool - true, if the startup succeeded, else false
*
*  NOTES
*     This function should not be called directly, it is called by the
*     spooling framework.
*
*  SEE ALSO
*     spool/berkeleydb/--BerkeleyDB-Spooling
*     spool/spool_startup_context()
*******************************************************************************/
bool
spool_berkeleydb_default_startup_func(lList **answer_list, 
                                      const lListElem *rule, bool check)
{
   bool ret = true;
   const char *url;

   struct bdb_info *info;

   DENTER(TOP_LAYER, "spool_berkeleydb_default_startup_func");

   url = lGetString(rule, SPR_url);
   info = (struct bdb_info *)lGetRef(rule, SPR_clientdata);

   ret = spool_berkeleydb_check_version(answer_list);

   if (ret) {
      ret = spool_berkeleydb_create_environment(answer_list, info, url);
   }

   /* we only open database, if check = true */
   if (ret && check) {
      ret = spool_berkeleydb_open_database(answer_list, info, url, false);
   }

   DEXIT;
   return ret;
}

/****** spool/berkeleydb/spool_berkeleydb_default_shutdown_func() **************
*  NAME
*     spool_berkeleydb_default_shutdown_func() -- shutdown spooling context
*
*  SYNOPSIS
*     bool 
*     spool_berkeleydb_default_shutdown_func(lList **answer_list, 
*                                          lListElem *rule);
*
*  FUNCTION
*     Shuts down the context, e.g. the database connection.
*
*  INPUTS
*     lList **answer_list - to return error messages
*     const lListElem *rule - the rule containing data necessary for
*                             the shutdown (e.g. path to the spool directory)
*
*  RESULT
*     bool - true, if the shutdown succeeded, else false
*
*  NOTES
*     This function should not be called directly, it is called by the
*     spooling framework.
*
*  SEE ALSO
*     spool/berkeleydb/--Spooling-BerkeleyDB
*     spool/spool_shutdown_context()
*******************************************************************************/
bool
spool_berkeleydb_default_shutdown_func(lList **answer_list, 
                                    const lListElem *rule)
{
   bool ret = true;
   const char *url;

   struct bdb_info *info;

   DENTER(TOP_LAYER, "spool_berkeleydb_default_shutdown_func");

   url = lGetString(rule, SPR_url);
   info = (struct bdb_info *)lGetRef(rule, SPR_clientdata);

   if (info == NULL) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_ERROR, 
                              MSG_BERKELEY_NOCONNECTIONOPEN_S,
                              url);
      ret = false;
   } else {
      ret = spool_berkeleydb_close_database(answer_list, info, url);
   }

   DEXIT;
   return ret;
}

/****** spool/berkeleydb/spool_berkeleydb_default_maintenance_func() ************
*  NAME
*     spool_berkeleydb_default_maintenance_func() -- maintain database
*
*  SYNOPSIS
*     bool 
*     spool_berkeleydb_default_maintenance_func(lList **answer_list, 
*                                    lListElem *rule
*                                    const spooling_maintenance_command cmd,
*                                    const char *args);
*
*  FUNCTION
*     Maintains the database:
*        - initialization
*        - ...
*
*  INPUTS
*     lList **answer_list   - to return error messages
*     const lListElem *rule - the rule containing data necessary for
*                             the maintenance (e.g. path to the spool 
*                             directory)
*     const spooling_maintenance_command cmd - the command to execute
*     const char *args      - arguments to the maintenance command
*
*  RESULT
*     bool - true, if the maintenance succeeded, else false
*
*  NOTES
*     This function should not be called directly, it is called by the
*     spooling framework.
*
*  SEE ALSO
*     spool/berkeleydb/--Spooling-BerkeleyDB
*     spool/spool_maintain_context()
*******************************************************************************/
bool
spool_berkeleydb_default_maintenance_func(lList **answer_list, 
                                    const lListElem *rule, 
                                    const spooling_maintenance_command cmd,
                                    const char *args)
{
   bool ret = true;
   const char *url;

   struct bdb_info *info;

   DENTER(TOP_LAYER, "spool_berkeleydb_default_maintenance_func");

   url = lGetString(rule, SPR_url);
   info = (struct bdb_info *)lGetRef(rule, SPR_clientdata);

   switch (cmd) {
      case SPM_init:
         ret = spool_berkeleydb_open_database(answer_list, info, url, true);
         break;
      default:
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                 ANSWER_QUALITY_WARNING, 
                                 "unknown maintenance command %d\n", cmd);
         ret = false;
         break;
         
   }

   DEXIT;
   return ret;
}

bool
spool_berkeleydb_trigger_func(lList **answer_list, const lListElem *rule,
                              time_t trigger, time_t *next_trigger)
{
   bool ret = true;
   const char *url;
   struct bdb_info *info;

   DENTER(TOP_LAYER, "spool_berkeleydb_trigger_func");

   url = lGetString(rule, SPR_url);
   info = (struct bdb_info *)lGetRef(rule, SPR_clientdata);
   if (info == NULL) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_WARNING, 
                              MSG_BERKELEY_NOCONNECTIONOPEN_S,
                              lGetString(rule, SPR_url));
      ret = false;

      /* nothing can be done - but set new trigger!! */
      *next_trigger = trigger + MIN(BERKELEYDB_CLEAR_INTERVAL, 
                                    BERKELEYDB_CHECKPOINT_INTERVAL);
   } else {
      if (info->next_clear <= trigger) {
         ret = spool_berkeleydb_clear_log(answer_list, info, url);
         info->next_clear = trigger + BERKELEYDB_CLEAR_INTERVAL;
      }

      if (info->next_checkpoint <= trigger) {
         ret = spool_berkeleydb_checkpoint(answer_list, info);
         info->next_checkpoint = trigger + BERKELEYDB_CHECKPOINT_INTERVAL;
      }

      /* set time of next trigger */
      *next_trigger = MIN(info->next_clear, info->next_checkpoint); 
   }

   DEXIT;
   return ret;
}

bool
spool_berkeleydb_transaction_func(lList **answer_list, const lListElem *rule, 
                                  spooling_transaction_command cmd)
{
   bool ret = true;

   struct bdb_info *info;

   DENTER(TOP_LAYER, "spool_berkeleydb_default_transaction_func");

   info = (struct bdb_info *)lGetRef(rule, SPR_clientdata);
   if (info == NULL) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_ERROR, 
                              MSG_BERKELEY_NOCONNECTIONOPEN_S,
                              lGetString(rule, SPR_url));
      ret = false;
   } else {
      switch (cmd) {
         case STC_begin:
            ret = spool_berkeleydb_start_transaction(answer_list, info);
            break;
         case STC_commit:
            ret = spool_berkeleydb_end_transaction(answer_list, info, true);
            break;
         case STC_rollback:
            ret = spool_berkeleydb_end_transaction(answer_list, info, false);
            break;
         default:
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                    ANSWER_QUALITY_ERROR, 
                                    MSG_BERKELEY_TRANSACTIONEINVAL);
            ret = false;
            break;
      }
   }
   
   DEXIT;
   return ret;
}

/****** spool/berkeleydb/spool_berkeleydb_default_list_func() *****************
*  NAME
*     spool_berkeleydb_default_list_func() -- read lists through berkeleydb spooling
*
*  SYNOPSIS
*     bool 
*     spool_berkeleydb_default_list_func(
*                                      lList **answer_list, 
*                                      const lListElem *type, 
*                                      const lListElem *rule, lList **list, 
*                                      const sge_object_type object_type) 
*
*  FUNCTION
*
*  INPUTS
*     lList **answer_list - to return error messages
*     const lListElem *type           - object type description
*     const lListElem *rule           - rule to be used 
*     lList **list                    - target list
*     const sge_object_type object_type - object type
*
*  RESULT
*     bool - true, on success, else false
*
*  NOTES
*     This function should not be called directly, it is called by the
*     spooling framework.
*
*  SEE ALSO
*     spool/berkeleydb/--BerkeleyDB-Spooling
*     spool/spool_read_list()
*******************************************************************************/
bool
spool_berkeleydb_default_list_func(lList **answer_list, 
                                 const lListElem *type, 
                                 const lListElem *rule, lList **list, 
                                 const sge_object_type object_type)
{
   bool ret = true;
#if 0
   bool local_transaction = false; /* did we start a transaction? */
#endif

   const lDescr *descr;
   const char *table_name;

   struct bdb_info *info;

   DENTER(TOP_LAYER, "spool_berkeleydb_default_list_func");

   info = (struct bdb_info *)lGetRef(rule, SPR_clientdata);
   descr = object_type_get_descr(object_type);
   table_name = object_type_get_name(object_type);

   if (info == NULL) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_WARNING, 
                              MSG_BERKELEY_NOCONNECTIONOPEN_S,
                              lGetString(rule, SPR_url));
      ret = false;
   } else if (descr == NULL || list == NULL ||
              table_name == NULL) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_WARNING, 
                              MSG_SPOOL_SPOOLINGOFXNOTSUPPORTED_S, 
                              object_type_get_name(object_type));
      ret = false;
   } else {
      /* if no transaction was opened from outside, open a new one */
#if 0
   /* JG: TODO: why does reading within a transaction give me the error:
    *           Not enough space
    */
      if (db->txn == NULL) {
         ret = spool_berkeleydb_start_transaction(answer_list, info);
         if (ret) {
            local_transaction = true;
         }
      }
#endif
      dstring key_dstring;
      char key_buffer[MAX_STRING_SIZE];
      const char *key;

      sge_dstring_init(&key_dstring, key_buffer, sizeof(key_buffer));
      key = sge_dstring_sprintf(&key_dstring, "%s:", table_name);
                 
      if (ret) {
         switch (object_type) {
            case SGE_TYPE_QINSTANCE:
               break;
            case SGE_TYPE_CQUEUE:
               /* read all cluster queues */
               ret = spool_berkeleydb_read_list(answer_list, info, 
                                                list, descr,
                                                key);
               if (ret) {
                  lListElem *queue;
                  const char *qinstance_table;
                  /* for all cluster queues: read queue instances */
                  qinstance_table = object_type_get_name(SGE_TYPE_QINSTANCE);
                  for_each(queue, *list) {
                     lList *qinstance_list = NULL;
                     const char *qname = lGetString(queue, CQ_name);

                     key = sge_dstring_sprintf(&key_dstring, "%s:%s@",
                                               qinstance_table,
                                               qname);
                     ret = spool_berkeleydb_read_list(answer_list, info,
                                                      &qinstance_list, QU_Type,
                                                      key);
                     if (ret && qinstance_list != NULL) {
                        lSetList(queue, CQ_qinstances, qinstance_list);
                     }
                  }
               }
               break;
            case SGE_TYPE_JATASK:
            case SGE_TYPE_PETASK:
               break;
            case SGE_TYPE_JOB:
               /* read all jobs */
               ret = spool_berkeleydb_read_list(answer_list, info, 
                                                list, descr,
                                                key);
               if (ret) {
                  lListElem *job;
                  const char *ja_task_table;
                  /* for all jobs: read ja_tasks */
                  ja_task_table = object_type_get_name(SGE_TYPE_JATASK);
                  for_each(job, *list) {
                     lList *task_list = NULL;
                     u_long32 job_id = lGetUlong(job, JB_job_number);

                     key = sge_dstring_sprintf(&key_dstring, "%s:%8d.",
                                               ja_task_table,
                                               job_id);
                     ret = spool_berkeleydb_read_list(answer_list, info,
                                                      &task_list, JAT_Type,
                                                      key);
                     /* reading ja_tasks succeeded */
                     if (ret) {
                        /* we actually found ja_tasks for this job */
                        if (task_list != NULL) {
                           lListElem *ja_task;
                           const char *pe_task_table;

                           lSetList(job, JB_ja_tasks, task_list);
                           pe_task_table = object_type_get_name(SGE_TYPE_PETASK);

                           for_each(ja_task, task_list) {
                              lList *pe_task_list = NULL;
                              u_long32 ja_task_id = lGetUlong(ja_task, 
                                                              JAT_task_number);
                              key = sge_dstring_sprintf(&key_dstring, "%s:%8d.%8d.",
                                                        pe_task_table,
                                                        job_id, ja_task_id);
                              
                              ret = spool_berkeleydb_read_list(answer_list, info,
                                                      &pe_task_list, PET_Type,
                                                      key);
                              if (ret) {
                                 if (pe_task_list != NULL) {
                                    lSetList(ja_task, JAT_task_list, pe_task_list);
                                 }
                              } else {
                                 break;
                              }
                           }
                        }
                     }

                     if (!ret) {
                        break;
                     }
                  }
               }
               break;
            default:
               ret = spool_berkeleydb_read_list(answer_list, info, 
                                                list, descr,
                                                key);
               break;
         }
#if 0
         /* spooling is done, now end the transaction, if we have an own one */
         if (local_transaction) {
            ret = spool_berkeleydb_end_transaction(answer_list, info, ret);
         }
#endif
      }
   }

   if (ret) {
      lListElem *ep;
      spooling_validate_func validate = 
         (spooling_validate_func)lGetRef(rule, SPR_validate_func);
      spooling_validate_list_func validate_list = 
         (spooling_validate_list_func)lGetRef(rule, SPR_validate_list_func);

      /* validate each individual object */
      /* JG: TODO: is it valid to validate after reading all objects? */
      for_each(ep, *list) {
         ret = validate(answer_list, type, rule, ep, object_type);
         if (!ret) {
            /* error message has been created in the validate func */
            break;
         }
      }

      if (ret) {
         /* validate complete list */
         ret = validate_list(answer_list, type, rule, object_type);
      }
   }



   DEXIT;
   return ret;
}

/****** spool/berkeleydb/spool_berkeleydb_default_read_func() *****************
*  NAME
*     spool_berkeleydb_default_read_func() -- read objects through berkeleydb spooling
*
*  SYNOPSIS
*     lListElem* 
*     spool_berkeleydb_default_read_func(lList **answer_list, 
*                                      const lListElem *type, 
*                                      const lListElem *rule, const char *key, 
*                                      const sge_object_type object_type) 
*
*  FUNCTION
*
*  INPUTS
*     lList **answer_list - to return error messages
*     const lListElem *type           - object type description
*     const lListElem *rule           - rule to use
*     const char *key                 - unique key specifying the object
*     const sge_object_type object_type - object type
*
*  RESULT
*     lListElem* - the object, if it could be read, else NULL
*
*  NOTES
*     This function should not be called directly, it is called by the
*     spooling framework.
*
*  SEE ALSO
*     spool/berkeleydb/--BerkeleyDB-Spooling
*     spool/spool_read_object()
*******************************************************************************/
lListElem *
spool_berkeleydb_default_read_func(lList **answer_list, 
                                 const lListElem *type, 
                                 const lListElem *rule, const char *key, 
                                 const sge_object_type object_type)
{
   lListElem *ep = NULL;

   struct bdb_info *info;

   DENTER(TOP_LAYER, "spool_berkeleydb_default_read_func");

   info = (struct bdb_info *)lGetRef(rule, SPR_clientdata);

   switch (object_type) {
      default:
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                 ANSWER_QUALITY_WARNING, 
                                 MSG_SPOOL_SPOOLINGOFXNOTSUPPORTED_S, 
                                 object_type_get_name(object_type));
         break;
   }

   DEXIT;
   return ep;
}

/****** spool/berkeleydb/spool_berkeleydb_default_write_func() ****************
*  NAME
*     spool_berkeleydb_default_write_func() -- write objects through berkeleydb spooling
*
*  SYNOPSIS
*     bool
*     spool_berkeleydb_default_write_func(lList **answer_list, 
*                                       const lListElem *type, 
*                                       const lListElem *rule, 
*                                       const lListElem *object, 
*                                       const char *key, 
*                                       const sge_object_type object_type) 
*
*  FUNCTION
*     Writes an object through the appropriate berkeleydb spooling functions.
*
*  INPUTS
*     lList **answer_list - to return error messages
*     const lListElem *type           - object type description
*     const lListElem *rule           - rule to use
*     const lListElem *object         - object to spool
*     const char *key                 - unique key
*     const sge_object_type object_type - object type
*
*  RESULT
*     bool - true on success, else false
*
*  NOTES
*     This function should not be called directly, it is called by the
*     spooling framework.
*
*  SEE ALSO
*     spool/berkeleydb/--BerkeleyDB-Spooling
*     spool/spool_delete_object()
*******************************************************************************/
bool
spool_berkeleydb_default_write_func(lList **answer_list, 
                                  const lListElem *type, 
                                  const lListElem *rule, 
                                  const lListElem *object, 
                                  const char *key, 
                                  const sge_object_type object_type)
{
   bool ret = true;
   bool local_transaction = false; /* did we start a transaction? */
   struct bdb_info *info;

   DENTER(TOP_LAYER, "spool_berkeleydb_default_write_func");

   DPRINTF(("spool_berkeleydb_default_write_func called for %s with key %s\n",
            object_type_get_name(object_type), key != NULL ? key : "<null>"));

   info = (struct bdb_info *)lGetRef(rule, SPR_clientdata);
   if (info == NULL) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_WARNING, 
                              MSG_BERKELEY_NOCONNECTIONOPEN_S,
                              lGetString(rule, SPR_url));
      ret = false;
   } else if (key == NULL) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_WARNING, 
                              MSG_BERKELEY_NULLVALUEASKEY,
                              lGetString(rule, SPR_url));
      ret = false;
   } else {
      /* if no transaction was opened from outside, open a new one */
      DB_TXN *txn = bdb_get_txn(info);
      if (txn == NULL) {
         ret = spool_berkeleydb_start_transaction(answer_list, info);
         if (ret) {
            local_transaction = true;
         }
      }

      if (ret) {
         switch (object_type) {
            case SGE_TYPE_JOB:
            case SGE_TYPE_JATASK:
            case SGE_TYPE_PETASK:
               {
                  u_long32 job_id, ja_task_id;
                  char *pe_task_id;
                  char *dup = strdup(key);
                  bool only_job; 

                  job_parse_key(dup, &job_id, &ja_task_id, &pe_task_id, &only_job);

                  if (object_type == SGE_TYPE_PETASK) {
                     ret = spool_berkeleydb_write_pe_task(answer_list, info,
                                                          object,
                                                          job_id, ja_task_id,
                                                          pe_task_id);
                  } else if (object_type == SGE_TYPE_JATASK) {
                     ret = spool_berkeleydb_write_ja_task(answer_list, info,
                                                          object,
                                                          job_id, ja_task_id);
                  } else {
                     ret = spool_berkeleydb_write_job(answer_list, info,
                                                      object,
                                                      job_id, only_job);
                  }
               }
               break;
            case SGE_TYPE_CQUEUE:
               ret = spool_berkeleydb_write_cqueue(answer_list, info, 
                                                   object, key);
               break;
            default:
               {
                  dstring dbkey_dstring;
                  char dbkey_buffer[MAX_STRING_SIZE];
                  const char *dbkey;

                  sge_dstring_init(&dbkey_dstring, 
                                   dbkey_buffer, sizeof(dbkey_buffer));

                  dbkey = sge_dstring_sprintf(&dbkey_dstring, "%s:%s", 
                                              object_type_get_name(object_type),
                                              key);

                  ret = spool_berkeleydb_write_object(answer_list, info, 
                                                      object, dbkey);
               }
               break;
         }
      }

      /* spooling is done, now end the transaction, if we have an own one */
      if (local_transaction) {
         ret = spool_berkeleydb_end_transaction(answer_list, info, ret);
      }
   }

   DEXIT;
   return ret;
}

/****** spool/berkeleydb/spool_berkeleydb_default_delete_func() ***************
*  NAME
*     spool_berkeleydb_default_delete_func() -- delete object in berkeleydb spooling
*
*  SYNOPSIS
*     bool
*     spool_berkeleydb_default_delete_func(lList **answer_list, 
*                                        const lListElem *type, 
*                                        const lListElem *rule, 
*                                        const char *key, 
*                                        const sge_object_type object_type) 
*
*  FUNCTION
*     Deletes an object in the berkeleydb spooling.
*
*  INPUTS
*     lList **answer_list - to return error messages
*     const lListElem *type           - object type description
*     const lListElem *rule           - rule to use
*     const char *key                 - unique key 
*     const sge_object_type object_type - object type
*
*  RESULT
*     bool - true on success, else false
*
*  NOTES
*     This function should not be called directly, it is called by the
*     spooling framework.
*
*  SEE ALSO
*     spool/berkeleydb/--BerkeleyDB-Spooling
*     spool/spool_delete_object()
*******************************************************************************/
bool
spool_berkeleydb_default_delete_func(lList **answer_list, 
                                   const lListElem *type, 
                                   const lListElem *rule,
                                   const char *key, 
                                   const sge_object_type object_type)
{
   bool ret = true;
   bool local_transaction = false; /* did we start a transaction? */
   const char *table_name;
   struct bdb_info *info;

   dstring dbkey_dstring;
   char dbkey_buffer[MAX_STRING_SIZE];
   const char *dbkey;

   DENTER(TOP_LAYER, "spool_berkeleydb_default_delete_func");

   sge_dstring_init(&dbkey_dstring, dbkey_buffer, sizeof(dbkey_buffer));
   info = (struct bdb_info *)lGetRef(rule, SPR_clientdata);
   if (info == NULL) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_WARNING, 
                              MSG_BERKELEY_NOCONNECTIONOPEN_S,
                              lGetString(rule, SPR_url));
      ret = false;
   } else {
      DB_TXN *txn = bdb_get_txn(info);
      /* if no transaction was opened from outside, open a new one */
      if (txn == NULL) {
         ret = spool_berkeleydb_start_transaction(answer_list, info);
         if (ret) {
            local_transaction = true;
         }
      }

      if (ret) {
         switch (object_type) {
            case SGE_TYPE_CQUEUE:
               ret = spool_berkeleydb_delete_cqueue(answer_list, info, key);
               break;
            case SGE_TYPE_JOB:
            case SGE_TYPE_JATASK:
            case SGE_TYPE_PETASK:
               {
                  u_long32 job_id, ja_task_id;
                  char *pe_task_id;
                  char *dup = strdup(key);
                  bool only_job; 

                  job_parse_key(dup, &job_id, &ja_task_id, &pe_task_id, &only_job);

                  if (pe_task_id != NULL) {
                     dbkey = sge_dstring_sprintf(&dbkey_dstring, "%8d.%8d %s",
                                                 job_id, ja_task_id, pe_task_id);
                     ret = spool_berkeleydb_delete_pe_task(answer_list, info, 
                                                           dbkey, false);
                  } else if (ja_task_id != 0) {
                     dbkey = sge_dstring_sprintf(&dbkey_dstring, "%8d.%8d",
                                                 job_id, ja_task_id);
                     ret = spool_berkeleydb_delete_ja_task(answer_list, info, 
                                                           dbkey, false);
                  } else {
                     dbkey = sge_dstring_sprintf(&dbkey_dstring, "%8d",
                                                 job_id);
                     ret = spool_berkeleydb_delete_job(answer_list, info, 
                                                       dbkey, false);
                  }
               }
               break;
            default:
               table_name = object_type_get_name(object_type);
               dbkey = sge_dstring_sprintf(&dbkey_dstring, "%s:%s", 
                                           table_name,
                                           key);

               ret = spool_berkeleydb_delete_object(answer_list, info, 
                                                    dbkey, false);
               break;
         }

         /* spooling is done, now end the transaction, if we have an own one */
         if (local_transaction) {
            ret = spool_berkeleydb_end_transaction(answer_list, info, ret);
         }
      }   
   }

   DEXIT;
   return ret;
}
