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

#define SGE_INCLUDE_QUEUED_JOBS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <float.h>
#include <math.h>

#include "sge_profiling.h"
#include "sge_log.h"
#include "sge.h"
#include "sge_all_listsL.h"
#include "commlib.h"
#include "sge_time.h"
#include "sge_prog.h"
#include "sgermon.h"
#include "scheduler.h"
#include "sge_orders.h"
#include "sge_job_schedd.h"
#include "sge_urgency.h"

#include "sgeee.h"
#include "sge_support.h"
#include "sge_schedd_conf.h"
#include "sge_ja_task.h"
#include "sge_usageL.h"
#include "sge_eejobL.h"

#include "sort_hosts.h"
#include "msg_schedd.h"
#include "sge_language.h"
#include "sge_string.h"
#include "sge_conf.h"
#include "sge_job.h"
#include "sge_range.h"
#include "sge_pe.h"
#include "sge_qinstance.h"
#include "sge_host.h"
#include "sge_userprj.h"

/* 
 * Following fields are necessary for tasks which are not
 * enrolled in the JB_ja_tasks-list. For these jobs we have no
 * reference to the 'ja_task'-CULL-element. 
 */ 
typedef struct {
   u_long32 job_number;       /* job number */
   u_long32 ja_task_number;   /* ja task id */
   double ja_task_fticket;    /* ftickets for task 'ja_task_id' */ 
   double ja_task_sticket;    /* stickets for task 'ja_task_id' */ 
   double ja_task_oticket;    /* otickets for task 'ja_task_id' */ 
   double ja_task_ticket;     /* tickets for task 'ja_task_id' */ 
   double ja_task_share;      /* share for task 'ja_task_id' */
   u_long32 ja_task_fshare;   /* fshare for task 'ja_task_id' */
} sge_task_ref_t;

/*
 * sge_ref_t - this structure is used in the SGEEE
 * scheduler to keep references to all the objects
 * associated with a job.  An array is built containing
 * one of these structures for each job/array we
 * are scheduling.
 */

typedef struct {
   lListElem *job;		      /* job reference */
   lListElem *ja_task;        /* task reference */
   lListElem *user;		      /* user reference */
   lListElem *project;		   /* project reference */
   lListElem *dept;		      /* department reference */
   lListElem *jobclass;	      /* job class reference */
   lListElem *node;		      /* node reference */
   int queued;                /* =1 if job is a queued job */
   int num_task_jobclasses;   /* number of task job classes */
   lListElem **task_jobclass; /* task job class reference array */
   u_long32  share_tree_type; /* share tree type */
   double total_jobclass_ftickets;
   double total_jobclass_otickets;
   double user_fshare;        /* job's share of user functional shares */
   double dept_fshare;        /* job's share of department functional shares */
   double project_fshare;     /* job's share of project functional shares */
   double jobclass_fshare;    /* job's share of jobclass functional shares */
   double job_fshare;         /* job's share of job functional shares */
   double tickets;            /* job's pending tickets from hierarchical policies */
   sge_task_ref_t *tref;
} sge_ref_t;

/*
 * this allows to build a list from
 * the the ref structure. This is need
 * to build the functional categories
 */
typedef struct sge_ref_list_t{
   sge_ref_t *ref;            /* ref reference */
   struct sge_ref_list_t *next;        /* next list item */
   struct sge_ref_list_t *prev;        /* previous list itme */
} sge_ref_list_t;
/* profiling info*/
extern int do_profiling;

static sge_Sdescr_t *all_lists;
static u_long32 sge_scheduling_run;


static void tix_range_set(double min_tix, double max_tix);
static void tix_range_get(double *min_tix, double *max_tix);

/* EB: debug 
#define DEBUG_TASK_REF 
*/

#ifdef DEBUG_TASK_REF 
static void task_ref_print_table(void);
static void task_ref_print_table_entry(sge_task_ref_t *tref);
#endif

static void sge_clear_job( lListElem *job);
static void sge_clear_ja_task ( lListElem *ja_task );

static sge_task_ref_t *task_ref_get_first(u_long32 job_number, u_long32 ja_task_number);
static sge_task_ref_t *task_ref_get_first_job_entry(void);
static sge_task_ref_t *task_ref_get_next_job_entry(void);
static void task_ref_copy_to_ja_task(sge_task_ref_t *tref, lListElem *ja_task);

static void sge_do_sgeee_priority(lList *job_list, double min_tix, double max_tix);
static void sgeee_priority(lListElem *task, u_long32 jobid, double nsu, double min_tix, double max_tix);
static void recompute_prio(sge_task_ref_t *tref, lListElem *task, double uc);

static void get_max_ptix(double *min, double *max, lList *job_list);

static lList *sge_build_sgeee_orders ( sge_Sdescr_t *lists,
                              lList *running_jobs,
                              lList *queued_jobs,
                              lList *finished_jobs,
                              lList *order_list,
                              bool update_usage_and_configuration,
                              int seqno, 
                              bool max_queued_ticket_orders,
                              bool update_execd);
static double calc_job_tickets(sge_ref_t *ref);
static int sge_calc_tickets (sge_Sdescr_t *lists,
                      lList *running_jobs,
                      lList *finished_jobs,
	          	       lList *queued_jobs,
                      int do_usage );

static lListElem *get_mod_share_tree(lListElem *node, lEnumeration *what, int seqno);
static lList *sge_sort_pending_job_nodes(lListElem *root, lListElem *node,
                           double total_share_tree_tickets);
static int sge_calc_node_targets(lListElem *root, lListElem *node, sge_Sdescr_t *lists);
static int sge_calc_sharetree_targets(lListElem *root, sge_Sdescr_t *lists,
                           lList *decay_list, u_long curr_time,
                           u_long seqno);
static int sge_init_share_tree_node_fields( lListElem *node, void *ptr );
static int sge_init_share_tree_nodes( lListElem *root );

static void free_fcategories(lList **fcategories);
static void build_functional_categories(sge_ref_t *job_ref, int num_jobs, lList **root, int dependent); 
static void copy_ftickets(sge_ref_list_t *source, sge_ref_list_t *dest);
static void destribute_ftickets(lList *root, int dependent);

static void calc_intern_pending_job_functional_tickets(
                                    lListElem *current,
                                    double sum_of_user_functional_shares,
                                    double sum_of_project_functional_shares,
                                    double sum_of_department_functional_shares,
                                    double sum_of_jobclass_functional_shares,
                                    double sum_of_job_functional_shares,
                                    double total_functional_tickets,
                                    double weight[],
                                    bool share_functional_shares);


static void task_ref_initialize_table(u_long32 number_of_tasks);
static void task_ref_destroy_table(void);
static sge_task_ref_t *task_ref_get_entry(u_long32 index);
static sge_task_ref_t *task_ref_table = NULL;
static u_long32 task_ref_entries = 0;
static u_long32 task_ref_job_pos = 0;
static double Master_min_tix = 0.0;
static double Master_max_tix = 0.0;

/****** sgeee/tix_range_set() **************************************************
*  NAME
*     tix_range_set() -- Store ticket range.
*
*  SYNOPSIS
*     static void tix_range_set(double min_tix, double max_tix) 
*
*  FUNCTION
*     Stores ticket range in the global variables.
*
*  INPUTS
*     double min_tix - Minimum ticket value.
*     double max_tix - Maximum ticket value.
*
*  NOTES
*     MT-NOTES: tix_range_set() is not MT safe
*******************************************************************************/
static void tix_range_set(double min_tix, double max_tix)
{
   Master_min_tix = min_tix;
   Master_max_tix = max_tix;
}

/****** sgeee/tix_range_get() **************************************************
*  NAME
*     tix_range_get() -- Get stored ticket range.
*
*  SYNOPSIS
*     static void tix_range_get(double *min_tix, double *max_tix) 
*
*  FUNCTION
*     Get stored ticket range from global variables.
*
*  INPUTS
*     double *min_tix - Target for minimum value.
*     double *max_tix - Target for maximum value.
*
*  NOTES
*     MT-NOTES: tix_range_get() is not MT safe
*******************************************************************************/
static void tix_range_get(double *min_tix, double *max_tix)
{
   if (min_tix) 
      *min_tix = Master_min_tix;
   if (max_tix) 
      *max_tix = Master_max_tix;
}

static void task_ref_initialize_table(u_long32 number_of_tasks) 
{
   const size_t size = number_of_tasks * sizeof(sge_task_ref_t);

   if (task_ref_entries != number_of_tasks && number_of_tasks > 0) {
      task_ref_destroy_table();
      task_ref_table = (sge_task_ref_t *) malloc(size);
      task_ref_entries = number_of_tasks;
   }
   if (task_ref_table != NULL) {
      memset(task_ref_table, 0, size);
   }
}

static void task_ref_destroy_table(void) 
{
   if (task_ref_table != NULL) {
      free(task_ref_table);
      task_ref_entries = 0;
   }
}

static sge_task_ref_t *task_ref_get_entry(u_long32 index) 
{
   sge_task_ref_t *ret = NULL;

   DENTER(BASIS_LAYER, "task_ref_get_entry");
   if (index >= 0 && index < task_ref_entries) {
      ret = &task_ref_table[index];
   }
   DEXIT;
   return ret;
}

#ifdef DEBUG_TASK_REF
static void task_ref_print_table(void)
{
   u_long32 i;

   DENTER(BASIS_LAYER, "task_ref_print_table");
   for (i = 0; i < task_ref_entries; i++) {
      sge_task_ref_t *tref = task_ref_get_entry(i);

      if (tref == NULL) {
         break;
      }

      task_ref_print_table_entry(tref);
   }
   DEXIT;
}

static void task_ref_print_table_entry(sge_task_ref_t *tref) 
{
   if (tref != NULL) {
      fprintf(stderr, "@@@ "
         "job: "u32" "
         "ja_task: "u32" "
         "t: %f "
         "st: %f "
         "s: %f "
         "\n",
         tref->job_number,
         tref->ja_task_number,
         tref->ja_task_ticket,
         tref->ja_task_sticket,
         tref->ja_task_share
      );       
   }
}
#endif

static sge_task_ref_t *task_ref_get_first(u_long32 job_number, 
                                   u_long32 ja_task_number)
{
   sge_task_ref_t *ret = NULL;
   u_long32 i;

   DENTER(BASIS_LAYER, "task_ref_get_first");
   for (i = 0; i < task_ref_entries; i++) {
      sge_task_ref_t *tref = task_ref_get_entry(i);

      if (tref != NULL && tref->job_number == job_number &&  
          tref->ja_task_number == ja_task_number) {
         ret = tref;
         break;
      }
   }
   DEXIT;
   return ret;
}

static sge_task_ref_t *task_ref_get_first_job_entry(void)
{
   sge_task_ref_t *ret = NULL;

   DENTER(BASIS_LAYER, "task_ref_get_first_job_entry");
   task_ref_job_pos = 0;
   if (task_ref_job_pos < task_ref_entries) {
      ret = task_ref_get_entry(task_ref_job_pos);
   }
   DEXIT;
   return ret;
}

static sge_task_ref_t *task_ref_get_next_job_entry(void)
{
   sge_task_ref_t *current_entry = task_ref_get_entry(task_ref_job_pos);
   sge_task_ref_t *ret = NULL;
   u_long32 pos = task_ref_job_pos;

   DENTER(BASIS_LAYER, "task_ref_get_next_job_entry"); 
   if (current_entry != NULL) {
      u_long32 current_job_number = current_entry->job_number;
   
      while (++pos < task_ref_entries) {
         sge_task_ref_t *next_entry = task_ref_get_entry(pos);
         
         if (next_entry != NULL && 
             current_job_number != next_entry->job_number) {
            task_ref_job_pos = pos;
            ret = next_entry;
            break; 
         } 
      } 
   }
   DEXIT;
   return ret;        
}

static void task_ref_copy_to_ja_task(sge_task_ref_t *tref, lListElem *ja_task) 
{

   DENTER(BASIS_LAYER, "task_ref_copy_to_ja_task");

   if (ja_task != NULL && tref != NULL) {
      lSetUlong(ja_task, JAT_task_number, tref->ja_task_number);

      lSetDouble(ja_task, JAT_tix,                    tref->ja_task_ticket); 
      lSetDouble(ja_task, JAT_fticket,                   tref->ja_task_fticket); 
      lSetDouble(ja_task, JAT_sticket,                   tref->ja_task_sticket); 
      lSetDouble(ja_task, JAT_oticket,                   tref->ja_task_oticket); 
      lSetDouble(ja_task, JAT_share,                     tref->ja_task_share); 
      lSetUlong(ja_task, JAT_fshare,                     tref->ja_task_fshare); 

   }
   DEXIT;
}


#define SGE_MIN_USAGE 1.0

#define __REF_SET_TYPE(ref, cull_attr, ref_attr, value, function) \
{ \
   if ((ref)->ja_task) { \
      (function)((ref)->ja_task, cull_attr, (value)); \
   } else { \
      (ref_attr) = (value); \
   } \
}
 
#define __REF_GET_TYPE(ref, cull_attr, ref_attr, function) \
   ((ref)->ja_task ? (function)((ref)->ja_task, cull_attr) : (ref_attr))
 
#define __REF_SET_ULONG(ref, cull_attr, ref_attr, value) \
   __REF_SET_TYPE(ref, cull_attr, ref_attr, value, lSetUlong)
 
#define __REF_SET_DOUBLE(ref, cull_attr, ref_attr, value) \
   __REF_SET_TYPE(ref, cull_attr, ref_attr, value, lSetDouble)
 
#define __REF_GET_ULONG(ref, cull_attr, ref_attr) \
   __REF_GET_TYPE(ref, cull_attr, ref_attr, lGetUlong)
 
#define __REF_GET_DOUBLE(ref, cull_attr, ref_attr) \
   __REF_GET_TYPE(ref, cull_attr, ref_attr, lGetDouble)
 
/* u_long32 REF_GET_JA_TASK_NUMBER(sge_ref_t *ref) */
 
#define REF_GET_JA_TASK_NUMBER(ref) \
   __REF_GET_ULONG((ref), JAT_task_number, (ref)->tref->ja_task_number)
 
/* void REF_SET_JA_TASK_NUMBER(sge_ref_t *ref, u_long32 ja_task_number) */
#define REF_SET_JA_TASK_NUMBER(ref, ja_task_id) \
   __REF_SET_ULONG((ref), JAT_task_number, (ref)->tref->ja_task_number, \
                   (ja_task_id))
 
#define REF_GET_FTICKET(ref) \
   __REF_GET_DOUBLE((ref), JAT_fticket, (ref)->tref->ja_task_fticket)
 
#define REF_GET_STICKET(ref) \
   __REF_GET_DOUBLE((ref), JAT_sticket, (ref)->tref->ja_task_sticket)
 
#define REF_GET_OTICKET(ref) \
   __REF_GET_DOUBLE((ref), JAT_oticket, (ref)->tref->ja_task_oticket)

#define REF_GET_TICKET(ref) \
   __REF_GET_DOUBLE((ref), JAT_tix, (ref)->tref->ja_task_ticket)
 
#define REF_GET_SHARE(ref) \
   __REF_GET_DOUBLE((ref), JAT_share, (ref)->tref->ja_task_share)
 
#define REF_GET_FSHARE(ref) \
   __REF_GET_ULONG((ref), JAT_fshare, (ref)->tref->ja_task_fshare)

 
#define REF_SET_FTICKET(ref, ticket) \
   __REF_SET_DOUBLE((ref), JAT_fticket, (ref)->tref->ja_task_fticket, (ticket))
 
#define REF_SET_STICKET(ref, ticket) \
   __REF_SET_DOUBLE((ref), JAT_sticket, (ref)->tref->ja_task_sticket, (ticket))
 
#define REF_SET_OTICKET(ref, ticket) \
   __REF_SET_DOUBLE((ref), JAT_oticket, (ref)->tref->ja_task_oticket, (ticket))

#define REF_SET_TICKET(ref, ticket) \
   __REF_SET_DOUBLE((ref), JAT_tix, (ref)->tref->ja_task_ticket, (ticket))
 
#define REF_SET_SHARE(ref, share) \
   __REF_SET_DOUBLE((ref), JAT_share, (ref)->tref->ja_task_share, (share))

#define REF_SET_FSHARE(ref, fshare) \
   __REF_SET_ULONG((ref), JAT_fshare, (ref)->tref->ja_task_fshare, (fshare))



/****** sgeee/sgeee_resort_pending_jobs() **************************************
*  NAME
*     sgeee_resort_pending_jobs() -- Resort pending jobs after assignment
*
*  SYNOPSIS
*     void sgeee_resort_pending_jobs(lList **job_list, lList *orderlist) 
*
*  FUNCTION
*     Update pending jobs order upon assignement and change ticket amounts
*     in orders previously created. 
*     If we dispatch a job sub-task and the job has more sub-tasks, then 
*     the job is still first in the job list.
*     We need to remove and reinsert the job back into the sorted job
*     list in case another job is higher priority (i.e. has more tickets)
*     Additionally it is neccessary to update the number of pending tickets
*     for the following pending array task. (The next task will get less
*     tickets than the current one)
*
*  INPUTS
*     lList **job_list - The pending job list. The first job in the list was 
*                        assigned right before.
*     lList *orderlist - Needed for updating ORT_ptickets orders
*
*  NOTES
*
*******************************************************************************/
void sgeee_resort_pending_jobs(lList **job_list, lList *orderlist)
{
   lListElem *next_job = lFirst(*job_list);

   DENTER(TOP_LAYER, "sgeee_resort_pending_jobs");

   if (next_job) {
      u_long32 job_id = lGetUlong(next_job, JB_job_number);
      lListElem *tmp_task = lFirst(lGetList(next_job, JB_ja_tasks));
      lListElem *jep = NULL;
      lListElem *insert_jep = NULL;
      double prio;

      if (tmp_task == NULL) {
         double nurg;
         lList *range_list = lGetList(next_job, JB_ja_n_h_ids);
         u_long32 ja_task_id = range_list_get_first_id(range_list, NULL);
         sge_task_ref_t *tref = task_ref_get_first(job_id, ja_task_id);
         lListElem *ja_task_template =
                                   lFirst(lGetList(next_job, JB_ja_template));
         lListElem *order = NULL;

         tmp_task = ja_task_template;

         /*
          * jobs normalized urgency did not change though 
          * but it's needed for updating prio
          */
         nurg = lGetDouble(next_job, JB_nurg);

         /*
          * Update pending tickets in template element
          */
         DPRINTF(("task_ref_copy_to_ja_task(tref = "u32", template_task = "u32")\n",
            tref->ja_task_number, lGetUlong(ja_task_template, JAT_task_number)));
         task_ref_copy_to_ja_task(tref, ja_task_template);
         recompute_prio(tref, ja_task_template, nurg);

         /* 
          * Update pending tickets in ORT_ptickets-order which was
          * created previously
          */
         for_each(order, orderlist) {
            if (lGetUlong(order, OR_type) == ORT_ptickets &&
                lGetUlong(order, OR_job_number) == job_id) {
               lListElem *order_job = lFirst(lGetList(order, OR_joker));
               lListElem *order_task = lFirst(lGetList(order_job, JB_ja_tasks));
               DPRINTF(("task_ref_copy_to_ja_task(tref = "u32", task = "u32")\n",
                  tref->ja_task_number, lGetUlong(order_task, JAT_task_number)));
               task_ref_copy_to_ja_task(tref, order_task);
               recompute_prio(tref, order_task, nurg);
               break;
            }
         }
      }

      /*
       * Re-Insert job at the correct possition
       */
      lDechainElem(*job_list, next_job);
      prio = lGetDouble(tmp_task, JAT_prio);
      for_each(jep, *job_list) {
         u_long32 job_id2 = lGetUlong(jep, JB_job_number);
         lListElem *tmp_task2 = lFirst(lGetList(jep, JB_ja_tasks));
         double prio2;

         if (tmp_task2 == NULL) {
            tmp_task2 = lFirst(lGetList(jep, JB_ja_template));
         }
         prio2 = lGetDouble(tmp_task2, JAT_prio);
         if (prio > prio2 || (prio == prio2 && job_id < job_id2)) {
            break;
         }
         insert_jep = jep;
      }

      lInsertElem(*job_list, insert_jep, next_job);
   }
   DEXIT;
}


/****** sgeee/recompute_prio() *************************************************
*  NAME
*     recompute_prio() -- Recompute JAT prio based on changed ticket amount
*
*  SYNOPSIS
*     static void recompute_prio(sge_task_ref_t *tref, lListElem *task, double 
*     nurg) 
*
*  FUNCTION
*     Each time when the ticket amount for in a JAT_Type element is changed 
*     the JAT_prio needs to be updated. The new ticket value is normalized
*     and the priorty value is computed.
*
*  INPUTS
*     sge_task_ref_t *tref - The tref element that is related to the ticket change
*     lListElem *task      - The JAT_Type task element.
*     double nurg          - The normalized urgency assumed for the job.
*
*  NOTES
*******************************************************************************/
static void recompute_prio(sge_task_ref_t *tref, lListElem *task, double nurg)
{
   double min_tix, max_tix, prio;
   double ntix; 
   double weight_ticket = sconf_get_weight_ticket();
   double weight_urgency = sconf_get_weight_urgency();

   DENTER(TOP_LAYER, "recompute_prio");

   /* need to know min/max tix values to normalize new ticket value */
   tix_range_get(&min_tix, &max_tix);
   ntix = sge_normalize_value(tref->ja_task_ticket, min_tix, max_tix);
   lSetDouble(task, JAT_ntix, ntix); 

   /* prio changes, but only due to ntix update */
   prio = weight_urgency * nurg + weight_ticket * ntix;
   lSetDouble(task, JAT_prio, prio); 

   DPRINTF(("%f tickets for task "u32": ntix = %f (min/max %f/%f), "
         "prio = %f\n",
         tref->ja_task_ticket, tref->ja_task_number,
         ntix, min_tix, max_tix, 
         prio));

   DEXIT;
   return;
}

/*--------------------------------------------------------------------
 * job_is_active - returns true if job is active
 *--------------------------------------------------------------------*/

static int
job_is_active( lListElem *job,
               lListElem *ja_task )
{
   u_long job_status = lGetUlong(ja_task, JAT_status);
#ifdef SGE_INCLUDE_QUEUED_JOBS
   return (job_status == JIDLE ||
       job_status & (JRUNNING | JMIGRATING | JQUEUED | JTRANSFERING)) &&
       !(lGetUlong(ja_task, JAT_state) 
         & (JSUSPENDED|JSUSPENDED_ON_THRESHOLD));
#else
   return (job_status & (JRUNNING | JMIGRATING | JTRANSFERING)) &&
       !(lGetUlong(ja_task, JAT_state) & 
         (JSUSPENDED|JSUSPENDED_ON_THRESHOLD));
#endif
}

/*--------------------------------------------------------------------
 * locate_department - locate the department object by name
 *--------------------------------------------------------------------*/

static lListElem *
locate_department( lList *dept_list,
                   const char *name )
{
   if (!name)
      return NULL;

   /*-------------------------------------------------------------
    * Look up the department object by name
    *-------------------------------------------------------------*/

   return lGetElemStr(dept_list, US_name, name);
}


#if 0  /* SVD040130 - commented out until we support job classes */

/*--------------------------------------------------------------------
 * locate_jobclass - locate the job class object by name
 *--------------------------------------------------------------------*/

static lListElem *
locate_jobclass( lList *job_class_list,
                 const char *name )
{
   if (!name)
      return NULL;

   /*-------------------------------------------------------------
    * Look up the job class object by name
    *-------------------------------------------------------------*/
   return qinstance_list_locate2(job_class_list, name);
}

#endif

#if 0
/*--------------------------------------------------------------------
 * locate_jobclass - locate the job class object for a pending job.
 * This returns the first matching queue.
 *--------------------------------------------------------------------*/

static lListElem *
locate_jobclass_for_pending_job( lListElem *jep,
                                 lList *queue_list,
                                 lList *exec_host_list,
                                 lList *centry_list )
{
   lListElem *qep;
   lList *ce;

   /*
   ** if the job has no requests� any queue fits
   ** so return the first queue
   */
   if (!lGetList(jep, JB_hard_resource_list)) {
      return lFirst(queue_list);
   }

   /*
   ** return first queue which fulfills the request_list of the job
   */
   for_each(qep, queue_list) {
      lListElem *ep;

      ce = NULL;
      queue_complexes2scheduler(&ce, qep, exec_host_list, centry_list, 0);
      if (sge_select_queue(ce, lGetList(jep, JB_hard_resource_list), 1,
                                       NULL, 0, -1)) {
         ce = lFreeList(ce);
         return qep;
      }
      ce = lFreeList(ce);
   }

   return NULL;
}

#endif


/*--------------------------------------------------------------------
 * sge_set_job_refs - set object references in the job entry
 *--------------------------------------------------------------------*/

static void
sge_set_job_refs( lListElem *job,
                  lListElem *ja_task,
                  sge_ref_t *ref,
                  sge_task_ref_t *tref,
                  sge_Sdescr_t *lists,
                  int queued)
{
   lListElem *root;
   int is_enrolled_ja_task = (ja_task != NULL && tref == NULL) ? 1 : 0;

   if (ref->task_jobclass) {
      free(ref->task_jobclass);
   }

   memset(ref, 0, sizeof(sge_ref_t));

   if (tref != NULL) {
      memset(tref, 0, sizeof(sge_task_ref_t));

      tref->job_number = lGetUlong(job, JB_job_number);
      tref->ja_task_number = lGetUlong(ja_task, JAT_task_number);
      ref->tref = tref;
   }

   /*-------------------------------------------------------------
    * save job reference
    *-------------------------------------------------------------*/

   ref->job = job;

   /*-------------------------------------------------------------
    * save task reference 
    *-------------------------------------------------------------*/

   ref->ja_task = is_enrolled_ja_task ? ja_task : NULL;

   /*-------------------------------------------------------------
    * save job state
    *-------------------------------------------------------------*/

   ref->queued = queued;

   /*-------------------------------------------------------------
    * locate user object and save off reference
    *-------------------------------------------------------------*/

   ref->user = userprj_list_locate(lists->user_list,
                                   lGetString(job, JB_owner));
   if (ref->user) {
      lSetUlong(ref->user, UP_job_cnt, 0);
      lSetUlong(ref->user, UP_pending_job_cnt, 0);
   }

   /*-------------------------------------------------------------
    * locate project object and save off reference
    *-------------------------------------------------------------*/

   ref->project = userprj_list_locate(lists->project_list,
                                      lGetString(job, JB_project));
   if (ref->project) {
      lSetUlong(ref->project, UP_job_cnt, 0);
      lSetUlong(ref->project, UP_pending_job_cnt, 0);
   }


#if 0 /* SVD040130 - commented out until we support job classes */

   /*-------------------------------------------------------------
    * locate job class object and save off reference
    *-------------------------------------------------------------*/

   if (ref->queued) {
#if 0
      ref->jobclass = locate_jobclass_for_pending_job(job,
                                 lists->all_queue_list,
                                 lists->host_list,
                                 lists->centry_list);
      if (ref->jobclass) {
         lSetUlong(ref->jobclass, QU_job_cnt, 0);
         lSetUlong(ref->jobclass, QU_pending_job_cnt, 0);
      }
#endif
   } else if (is_enrolled_ja_task) {
      ref->jobclass = locate_jobclass(lists->all_queue_list,
                                      lGetString(ja_task, JAT_master_queue));
      if (ref->jobclass) {
         lSetUlong(ref->jobclass, QU_job_cnt, 0);
         lSetUlong(ref->jobclass, QU_pending_job_cnt, 0);
      }
   }

   /*-------------------------------------------------------------
    * for task-controlled PE jobs
    *    for each task
    *       locate job class object and save off reference
    *-------------------------------------------------------------*/

   if (is_enrolled_ja_task && (pe_str=lGetString(ja_task, JAT_granted_pe)) && 
       (pe=pe_list_locate(all_lists->pe_list, pe_str)) &&
       lGetBool(pe, PE_control_slaves) &&
       (granted=lGetList(ja_task, JAT_granted_destin_identifier_list))) {

      ref->num_task_jobclasses = lGetNumberOfElem(granted);

      if (ref->num_task_jobclasses > 0) {
         const size_t size = ref->num_task_jobclasses * sizeof(lListElem *);
         int i=0;

         ref->task_jobclass = (lListElem **)malloc(size); 
         memset((void *)ref->task_jobclass, 0, size); 

         for_each(granted_el, granted) {
            if (active_subtasks(job, lGetString(granted_el, JG_qname))) {
               ref->task_jobclass[i] = locate_jobclass(lists->all_queue_list,
                                       lGetString(granted_el, JG_qname));
               if (ref->task_jobclass[i]) {
                  lSetUlong(ref->task_jobclass[i], QU_job_cnt, 0);
                  lSetUlong(ref->task_jobclass[i], QU_pending_job_cnt, 0);
               }
            }
            i++;
         }
      }
   }

#endif

   /*-------------------------------------------------------------
    * locate share tree node and save off reference
    *-------------------------------------------------------------*/

   if ((root = lFirst(lists->share_tree))) {
      lListElem *pnode = NULL;
       
      ref->share_tree_type = lGetUlong(root, STN_type);

#ifdef notdef
      if (ref->share_tree_type == STT_PROJECT)
         userprj = ref->project;
      else
         userprj = ref->user;
#endif

      if (ref->user || ref->project) {
         ref->node = search_userprj_node(root,
               ref->user ? lGetString(ref->user, UP_name) : NULL,
               ref->project ? lGetString(ref->project, UP_name) : NULL,
               &pnode);

         /*
          * if the node was not found and there is a user but no project,
          * then look for the default user node.
          */

         if (!ref->node && ref->user && !ref->project)
            ref->node = search_userprj_node(root, "default", NULL, &pnode);

         /*
          * if the found node is the "default" node, then create a
          * temporary sibling node using the "default" node as a
          * template
          */

         if (ref->user && ref->node && pnode &&
             strcmp(lGetString(ref->node, STN_name), "default") == 0) {
            ref->node = lCopyElem(ref->node);
            lSetString(ref->node, STN_name, lGetString(ref->user, UP_name));
            lSetUlong(ref->node, STN_temp, 1);
            lAppendElem(lGetList(pnode, STN_children), ref->node);
         }
      } else
         ref->node = NULL;
   }

   /*-------------------------------------------------------------
    * locate department object and save off reference in job entry
    *-------------------------------------------------------------*/

   ref->dept = locate_department(lists->dept_list,
                                 lGetString(job, JB_department));
   if (ref->dept) {
      lSetUlong(ref->dept, US_job_cnt, 0);
      lSetUlong(ref->dept, US_pending_job_cnt, 0);
   }
}


/*--------------------------------------------------------------------
 * sge_set_job_cnts - set job counts in the various object entries
 *--------------------------------------------------------------------*/

static void
sge_set_job_cnts( sge_ref_t *ref,
                  int queued )
{
   int i;
   u_long up_job_cnt = queued ? UP_pending_job_cnt : UP_job_cnt;
   u_long us_job_cnt = queued ? US_pending_job_cnt : US_job_cnt;
   u_long qu_job_cnt = queued ? QU_pending_job_cnt : QU_job_cnt;
   if (ref->user)
      lSetUlong(ref->user, up_job_cnt, lGetUlong(ref->user, up_job_cnt)+1);
   if (ref->project)
      lSetUlong(ref->project, up_job_cnt, lGetUlong(ref->project,up_job_cnt)+1);
   if (ref->dept)
      lSetUlong(ref->dept, us_job_cnt, lGetUlong(ref->dept, us_job_cnt)+1);
   if (ref->task_jobclass)
      for(i=0; i<ref->num_task_jobclasses; i++) {
         if (ref->task_jobclass[i])
            lSetUlong(ref->task_jobclass[i], qu_job_cnt,
                      lGetUlong(ref->task_jobclass[i], qu_job_cnt)+1);
      }
   else
      if (ref->jobclass)
         lSetUlong(ref->jobclass, qu_job_cnt, lGetUlong(ref->jobclass,
               qu_job_cnt)+1);
   return;
}


/*--------------------------------------------------------------------
 * sge_unset_job_cnts - set job counts in the various object entries
 *--------------------------------------------------------------------*/

static void
sge_unset_job_cnts( sge_ref_t *ref,
                    int queued )
{
   int i;
   u_long up_job_cnt = queued ? UP_pending_job_cnt : UP_job_cnt;
   u_long us_job_cnt = queued ? US_pending_job_cnt : US_job_cnt;
   u_long qu_job_cnt = queued ? QU_pending_job_cnt : QU_job_cnt;
   if (ref->user)
      lSetUlong(ref->user, up_job_cnt, lGetUlong(ref->user, up_job_cnt)-1);
   if (ref->project)
      lSetUlong(ref->project, up_job_cnt, lGetUlong(ref->project,up_job_cnt)-1);
   if (ref->dept)
      lSetUlong(ref->dept, us_job_cnt, lGetUlong(ref->dept, us_job_cnt)-1);
   if (ref->task_jobclass)
      for(i=0; i<ref->num_task_jobclasses; i++) {
         if (ref->task_jobclass[i])
            lSetUlong(ref->task_jobclass[i], qu_job_cnt,
                      lGetUlong(ref->task_jobclass[i], qu_job_cnt)-1);
      }
   else
      if (ref->jobclass)
         lSetUlong(ref->jobclass, qu_job_cnt, lGetUlong(ref->jobclass,
               qu_job_cnt)-1);
   return;
}


/*--------------------------------------------------------------------
 * calculate_m_shares - calculate m_share for share tree node
 *      descendants
 *
 * Calling calculate_m_shares(root_node) will calculate shares for
 * every active node in the share tree.
 *
 * Rather than having to recalculate m_shares on every scheduling
 * interval, we calculate it whenever the scheduler comes up or
 * whenever the share tree itself changes and make adjustments
 * every time a job becomes active or inactive (in adjust_m_shares).
 *--------------------------------------------------------------------*/

static void
calculate_m_shares( lListElem *parent_node )
{
   u_long sum_of_child_shares = 0;
   lListElem *child_node;
   lList *children;
   double parent_m_share;

   DENTER(TOP_LAYER, "calculate_m_shares");

   children = lGetList(parent_node, STN_children);
   if (!children) {
      DEXIT;
      return;
   }

   /*-------------------------------------------------------------
    * Sum child shares
    *-------------------------------------------------------------*/

   for_each(child_node, children) {

      if (lGetUlong(child_node, STN_job_ref_count) > 0)
         sum_of_child_shares += lGetUlong(child_node, STN_shares);
   }

   /*-------------------------------------------------------------
    * Calculate m_shares for each child and descendants
    *-------------------------------------------------------------*/

   parent_m_share = lGetDouble(parent_node, STN_m_share);

   for_each(child_node, children) {
      if (lGetUlong(child_node, STN_job_ref_count) > 0)
         lSetDouble(child_node, STN_m_share,
                    parent_m_share *
                    (sum_of_child_shares ?
                    ((double)(lGetUlong(child_node, STN_shares)) /
                    (double)sum_of_child_shares) : 0));
      else
         lSetDouble(child_node, STN_m_share, 0);

      calculate_m_shares(child_node);
   }

   DEXIT;
   return;
}


#ifdef notdef

/*--------------------------------------------------------------------
 * adjust_m_shares - adjust m_share for all ancestor nodes with
 * reference count less than or equal to count parameter.
 *--------------------------------------------------------------------*/

static void
adjust_m_shares( lListElem *root,
                 lListElem *job,
                 u_long count )
{
   lListElem *node;
   char *name;
   ancestors_t ancestors;
   int depth;

   if (!root)
      return;

   /*-------------------------------------------------------------
    * Look up share tree node
    *-------------------------------------------------------------*/

   name = lGetString(job, lGetUlong(root, STN_type) == STT_PROJECT ?
                          JB_project : JB_owner);

   if (!name)
      return;

   memset(&ancestors, 0, sizeof(ancestors_t));
   node = search_ancestor_list(root, name, &ancestors);
   depth = ancestors.depth;

   /*-------------------------------------------------------------
    * Search ancestor nodes until a previously active node is found
    *-------------------------------------------------------------*/

   while (depth-- && node && node != root &&
          lGetUlong(node, STN_job_ref_count)<=count) {
      node = ancestors.nodes[depth-1];
   }

   if (ancestors.nodes) free(ancestors.nodes);

   /*-------------------------------------------------------------
    * Calculate m_shares for every descendant of the active node
    *-------------------------------------------------------------*/

   if (node) calculate_m_shares(node);

}

/*

   To know if a node is active, we maintain a reference count in each
   node and do the following to keep it up-to-date.

   o call setup_share_tree when scheduler is initialized or
     if the share tree structure or shares are modified
   o call increment_job_ref_count when a new job is added (i.e. submitted)
   o call decrement_job_ref_count when a job is deleted (i.e. ended, aborted)

*/

/*--------------------------------------------------------------------
 * adjust_job_ref_count - adjusts all of the corresponding share
 * tree nodes' job reference counts
 *--------------------------------------------------------------------*/

static void
adjust_job_ref_count( lListElem *root,
                      lListElem *job,
                      long adjustment )
{
   lListElem *node=NULL;
   char *name;
   ancestors_t ancestors;
   int depth;

   /*-------------------------------------------------------------
    * Adjust job reference count for share tree node and
    *     ancestor nodes
    *-------------------------------------------------------------*/

   memset(&ancestors, 0, sizeof(ancestors_t));

   name = lGetString(job, lGetUlong(root, STN_type) == STT_PROJECT ?
                          JB_project : JB_owner);

   if (!name)
      return;

   memset(&ancestors, 0, sizeof(ancestors_t));
   node = search_ancestor_list(root, name, &ancestors);
   depth = ancestors.depth;

   while (depth--) {
      node = ancestors.nodes[depth];
      lSetUlong(node, STN_job_ref_count,
         lGetUlong(node, STN_job_ref_count)+adjustment);
   }

   if (ancestors.nodes) free(ancestors.nodes);

   return;
}


/*--------------------------------------------------------------------
 * increment_job_ref_count - increments all of the corresponding share
 * tree nodes' job reference counts and the job's share tree node
 * reference
 *--------------------------------------------------------------------*/

void
increment_job_ref_count( lListElem *root,
                         lListElem *job )
{
   adjust_job_ref_count(root, job, 1);
}


/*--------------------------------------------------------------------
 * decrement_job_ref_count - decrements all of the corresponding share
 * tree nodes' job reference counts and the job's share tree node
 * reference
 *--------------------------------------------------------------------*/

static void
decrement_job_ref_count( lListElem *root,
                         lListElem *job )
{
   adjust_job_ref_count(root, job, -1);
}


#endif /* notdef */


/*--------------------------------------------------------------------
 * update_job_ref_count - update job_ref_count for node and descendants
 *--------------------------------------------------------------------*/

u_long
update_job_ref_count( lListElem *node )
{
   int job_count=0;
   lList *children;
   lListElem *child;

   children = lGetList(node, STN_children);
   if (children) {
      for_each(child, children) {
         job_count += update_job_ref_count(child);
      }
      lSetUlong(node, STN_job_ref_count, job_count);
   }

   return lGetUlong(node, STN_job_ref_count);
}

/*--------------------------------------------------------------------
 * update_active_job_ref_count - update active_job_ref_count for node and descendants
 *--------------------------------------------------------------------*/

static u_long
update_active_job_ref_count( lListElem *node )
{
   int active_job_count=0;
   lList *children;
   lListElem *child;

   children = lGetList(node, STN_children);
   if (children) {
      for_each(child, children) {
         active_job_count += update_active_job_ref_count(child);
      }
      lSetUlong(node, STN_active_job_ref_count, active_job_count);
   }

   return lGetUlong(node, STN_active_job_ref_count);
}


/*--------------------------------------------------------------------
 * sge_init_share_tree_node_fields - zero out the share tree node
 * fields that will be set and used during sge_calc_tickets
 *--------------------------------------------------------------------*/

static int
sge_init_share_tree_node_fields( lListElem *node,
                                 void *ptr )
{
   static int sn_m_share_pos = -1;
   static int sn_adjusted_current_proportion_pos,
              sn_last_actual_proportion_pos, sn_sum_priority_pos,
              sn_job_ref_count_pos, sn_active_job_ref_count_pos,
              sn_usage_list_pos, sn_stt_pos, sn_ostt_pos,
              sn_ltt_pos, sn_oltt_pos, sn_shr_pos, sn_ref_pos,
              sn_proportion_pos, sn_adjusted_proportion_pos, sn_target_proportion_pos,
              sn_current_proportion_pos, sn_adjusted_usage_pos, sn_combined_usage_pos,
              sn_actual_proportion_pos;


   if (sn_m_share_pos == -1) {
      sn_m_share_pos = lGetPosViaElem(node, STN_m_share);
      sn_adjusted_current_proportion_pos =
            lGetPosViaElem(node, STN_adjusted_current_proportion);
      sn_last_actual_proportion_pos =
            lGetPosViaElem(node, STN_last_actual_proportion);
      sn_job_ref_count_pos = lGetPosViaElem(node, STN_job_ref_count);
      sn_active_job_ref_count_pos = lGetPosViaElem(node, STN_active_job_ref_count);
      sn_usage_list_pos = lGetPosViaElem(node, STN_usage_list);
      sn_sum_priority_pos = lGetPosViaElem(node, STN_sum_priority);
      /* sn_temp_pos = lGetPosViaElem(node, STN_temp); */
      sn_stt_pos = lGetPosViaElem(node, STN_stt);
      sn_ostt_pos = lGetPosViaElem(node, STN_ostt);
      sn_ltt_pos = lGetPosViaElem(node, STN_ltt);
      sn_oltt_pos = lGetPosViaElem(node, STN_oltt);
      sn_shr_pos = lGetPosViaElem(node, STN_shr);
      sn_ref_pos = lGetPosViaElem(node, STN_ref);
      sn_proportion_pos = lGetPosViaElem(node, STN_proportion);
      sn_adjusted_proportion_pos = lGetPosViaElem(node, STN_adjusted_proportion);
      sn_target_proportion_pos = lGetPosViaElem(node, STN_target_proportion);
      sn_current_proportion_pos = lGetPosViaElem(node, STN_current_proportion);
      sn_adjusted_usage_pos = lGetPosViaElem(node, STN_adjusted_usage);
      sn_combined_usage_pos = lGetPosViaElem(node, STN_combined_usage);
      sn_actual_proportion_pos = lGetPosViaElem(node, STN_actual_proportion);
   }

   lSetPosDouble(node, sn_m_share_pos, 0);
   lSetPosDouble(node, sn_last_actual_proportion_pos, 0);
   lSetPosDouble(node, sn_adjusted_current_proportion_pos, 0);
   lSetPosDouble(node, sn_proportion_pos, 0);
   lSetPosDouble(node, sn_adjusted_proportion_pos, 0);
   lSetPosDouble(node, sn_target_proportion_pos, 0);
   lSetPosDouble(node, sn_current_proportion_pos, 0);
   lSetPosDouble(node, sn_adjusted_usage_pos, 0);
   lSetPosDouble(node, sn_combined_usage_pos, 0);
   lSetPosDouble(node, sn_actual_proportion_pos, 0);
   lSetPosUlong(node, sn_job_ref_count_pos, 0);
   lSetPosUlong(node, sn_active_job_ref_count_pos, 0);
   lSetPosList(node, sn_usage_list_pos, NULL);
   lSetPosUlong(node, sn_sum_priority_pos, 0);
   /* lSetPosUlong(node, sn_temp_pos, 0); */
   lSetPosDouble(node, sn_stt_pos, 0);
   lSetPosDouble(node, sn_ostt_pos, 0);
   lSetPosDouble(node, sn_ltt_pos, 0);
   lSetPosDouble(node, sn_oltt_pos, 0);
   lSetPosDouble(node, sn_shr_pos, 0);
   lSetPosUlong(node, sn_ref_pos, 0);
   return 0;
}

/*--------------------------------------------------------------------
 * sge_init_share_tree_nodes - zero out the share tree node fields 
 * that will be set and used during sge_calc_tickets
 *--------------------------------------------------------------------*/

static int
sge_init_share_tree_nodes( lListElem *root )
{
   return sge_for_each_share_tree_node(root,
         sge_init_share_tree_node_fields, NULL);
}


/*--------------------------------------------------------------------
 * set_share_tree_project_flags - set the share tree project flag for
 *       node and descendants
 *--------------------------------------------------------------------*/

static void
set_share_tree_project_flags( lList *project_list,
                              lListElem *node )
{
   lList *children;
   lListElem *child;

   if (!project_list || !node)
      return;

   if (userprj_list_locate(project_list, lGetString(node, STN_name)))
      lSetUlong(node, STN_project, 1);
   else
      lSetUlong(node, STN_project, 0);

   children = lGetList(node, STN_children);
   if (children) {
      for_each(child, children) {
         set_share_tree_project_flags(project_list, child);
      }
   }
   return;
}


/*--------------------------------------------------------------------
 * get_usage - return usage entry based on name
 *--------------------------------------------------------------------*/

static lListElem *
get_usage( lList *usage_list,
           const char *name )
{
   return lGetElemStr(usage_list, UA_name, name);
}


/*--------------------------------------------------------------------
 * create_usage_elem - create a new usage element
 *--------------------------------------------------------------------*/

static lListElem *
create_usage_elem( const char *name )
{
   lListElem *usage;
    
   usage = lCreateElem(UA_Type);
   lSetString(usage, UA_name, name);
   lSetDouble(usage, UA_value, 0);

   return usage;
}


/*--------------------------------------------------------------------
 * build_usage_list - create a new usage list from an existing list
 *--------------------------------------------------------------------*/

lList *
build_usage_list( char *name,
                  lList *old_usage_list )
{
   lList *usage_list = NULL;
   lListElem *usage;

   if (old_usage_list) {

      /*-------------------------------------------------------------
       * Copy the old list and zero out the usage values
       *-------------------------------------------------------------*/

      usage_list = lCopyList(name, old_usage_list);
      for_each(usage, usage_list)
         lSetDouble(usage, UA_value, 0);

   } else {

      /*
       * the UA_value fields are implicitly set to 0 at creation
       * time of a new element with lCreateElem or lAddElemStr
       */
        
      lAddElemStr(&usage_list, UA_name, USAGE_ATTR_CPU, UA_Type);
      lAddElemStr(&usage_list, UA_name, USAGE_ATTR_MEM, UA_Type);
      lAddElemStr(&usage_list, UA_name, USAGE_ATTR_IO, UA_Type);
   }

   return usage_list;
}




/*--------------------------------------------------------------------
 * delete_debited_job_usage - deleted debitted job usage for job
 *--------------------------------------------------------------------*/

static void
delete_debited_job_usage( sge_ref_t *ref,
                          u_long seqno )
{
   lListElem *job=ref->job,
             *user=ref->user,
             *project=ref->project;
   lList *upu_list;
   lListElem *upu;
   
   DENTER(TOP_LAYER, "delete_debited_job_usage");

   DPRINTF(("DDJU (1) "u32"\n", lGetUlong(job, JB_job_number)));

   if (user) {
      upu_list = lGetList(user, UP_debited_job_usage);
      DPRINTF(("DDJU (2) "u32"\n", lGetUlong(job, JB_job_number)));
      if (upu_list) {
         
         /* Note: In order to cause the qmaster to delete the
            usage for this job, we zero out UPU_old_usage_list
            for this job in the UP_debited_job_usage list */
         DPRINTF(("DDJU (3) "u32"\n", lGetUlong(job, JB_job_number))); 

         if ((upu = lGetElemUlong(upu_list, UPU_job_number,
                             lGetUlong(job, JB_job_number)))) {
            DPRINTF(("DDJU (4) "u32"\n", lGetUlong(job, JB_job_number)));
            lSetList(upu, UPU_old_usage_list, NULL);
            lSetUlong(user, UP_usage_seqno, seqno);
         }
      }
   }

   if (project) {
      upu_list = lGetList(project, UP_debited_job_usage);
      if (upu_list) {
         
         /* Note: In order to cause the qmaster to delete the
            usage for this job, we zero out UPU_old_usage_list
            for this job in the UP_debited_job_usage list */

         if ((upu = lGetElemUlong(upu_list, UPU_job_number,
                             lGetUlong(job, JB_job_number)))) {
            lSetList(upu, UPU_old_usage_list, NULL);
            lSetUlong(project, UP_usage_seqno, seqno);
         }
      }
   }

   DEXIT;
   return;
}


/*--------------------------------------------------------------------
 * combine_usage - combines a node's associated user/project usage 
 * into a single value and stores it in the node.
 *--------------------------------------------------------------------*/

static void
combine_usage( sge_ref_t *ref )
{
   double usage_value = 0;

   /*-------------------------------------------------------------
    * Get usage from associated user/project object
    *-------------------------------------------------------------*/

#if 0
   userprj = ref->share_tree_type == STT_PROJECT ? ref->project : ref->user;
#endif

   if (ref->node) {
      const lList *usage_weight_list = NULL;
      lList *usage_list=NULL;
      lListElem *usage_weight, *usage_elem;
      double sum_of_usage_weights = 0;
      const char *usage_name;

      /*-------------------------------------------------------------
       * Sum usage weighting factors
       *-------------------------------------------------------------*/

      usage_weight_list = sconf_get_usage_weight_list();
      if (usage_weight_list) {
          for_each(usage_weight, usage_weight_list)
             sum_of_usage_weights += lGetDouble(usage_weight, UA_value);
      }

      /*-------------------------------------------------------------
       * Combine user/project usage based on usage weighting factors
       *-------------------------------------------------------------*/

      if (usage_weight_list) {

         if (ref->user) {
            if (ref->project) {
               lList *upp_list = lGetList(ref->user, UP_project);
               lListElem *upp;
               if (upp_list &&
                   ((upp = lGetElemStr(upp_list, UPP_name,
                                       lGetString(ref->project, UP_name)))))
                  usage_list = lGetList(upp, UPP_usage);
            } else
               usage_list = lGetList(ref->user, UP_usage);
         } else if (ref->project) /* not sure about this, use it when? */
            usage_list = lGetList(ref->project, UP_usage);

         for_each(usage_elem, usage_list) {
            usage_name = lGetString(usage_elem, UA_name);
            usage_weight = lGetElemStr(usage_weight_list, UA_name,
                                       usage_name);
            if (usage_weight && sum_of_usage_weights>0) {
               usage_value += lGetDouble(usage_elem, UA_value) *
                  (lGetDouble(usage_weight, UA_value) /
                   sum_of_usage_weights);
            }
         }
      }

      /*-------------------------------------------------------------
       * Set combined usage in the node
       *-------------------------------------------------------------*/

      lSetDouble(ref->node, STN_combined_usage, usage_value);

   }

   return;
}


/*--------------------------------------------------------------------
 * decay_and_sum_usage - accumulates and decays usage in the correct
 * user and project objects for the specified job
 *--------------------------------------------------------------------*/

static void
decay_and_sum_usage( sge_ref_t *ref,
                     lList *decay_list,
                     u_long seqno,
                     u_long curr_time )
{
   lList *job_usage_list=NULL,
         *old_usage_list=NULL,
         *user_usage_list=NULL,
         *project_usage_list=NULL,
         *user_long_term_usage_list=NULL,
         *project_long_term_usage_list=NULL;
   lListElem *job=ref->job,
             *ja_task=ref->ja_task,
             *node=ref->node,
             *user=ref->user,
             *project=ref->project,
             *userprj = NULL,
             *petask;

   if (!node && !user && !project)
      return;

#if 0
   if (ref->share_tree_type == STT_PROJECT)
      userprj = ref->project;
   else
      userprj = ref->user;
#endif

   if (ref->user)
      userprj = ref->user;
   else if (ref->project)
      userprj = ref->project;

   /*-------------------------------------------------------------
    * Decay the usage for the associated user and project
    *-------------------------------------------------------------*/
    
   if (user)
      decay_userprj_usage(user, decay_list, seqno, curr_time);

   if (project)
      decay_userprj_usage(project, decay_list, seqno, curr_time);

   /*-------------------------------------------------------------
    * Note: Since CODINE will update job.usage directly, we 
    * maintain the job usage the last time we collected it from
    * the job.  The difference between the new usage and the old
    * usage is what needs to be added to the user or project node.
    * This old usage is maintained in the user or project node
    * depending on the type of share tree.
    *-------------------------------------------------------------*/

   if (ja_task != NULL) {
      job_usage_list = lCopyList(NULL, lGetList(ja_task, JAT_scaled_usage_list));

      /* sum sub-task usage into job_usage_list */
      if (job_usage_list) {
         for_each(petask, lGetList(ja_task, JAT_task_list)) {
            lListElem *dst, *src;
            for_each(src, lGetList(petask, PET_scaled_usage)) {
               if ((dst=lGetElemStr(job_usage_list, UA_name, lGetString(src, UA_name))))
                  lSetDouble(dst, UA_value, lGetDouble(dst, UA_value) + lGetDouble(src, UA_value));
               else
                  lAppendElem(job_usage_list, lCopyElem(src));
            }
         }
      }
   }

   if (userprj) {
      lListElem *upu;
      lList *upu_list = lGetList(userprj, UP_debited_job_usage);
      if (upu_list) {
         if ((upu = lGetElemUlong(upu_list, UPU_job_number,
                                  lGetUlong(job, JB_job_number)))) {
            if ((old_usage_list = lGetList(upu, UPU_old_usage_list))) {
               old_usage_list = lCopyList("", old_usage_list);
            }
         }
      }
   }

   if (!old_usage_list)
      old_usage_list = build_usage_list("old_usage_list", NULL);

   if (user) {

      /* if there is a user & project, usage is kept in the project sub-list */

      if (project) {
         lList *upp_list = lGetList(user, UP_project);
         lListElem *upp;
         const char *project_name = lGetString(project, UP_name);

         if (!upp_list) {
            upp_list = lCreateList("", UPP_Type);
            lSetList(user, UP_project, upp_list);
         }
         if (!((upp = lGetElemStr(upp_list, UPP_name, project_name))))
            upp = lAddElemStr(&upp_list, UPP_name, project_name, UPP_Type);
         user_long_term_usage_list = lGetList(upp, UPP_long_term_usage);
         if (!user_long_term_usage_list) {
            user_long_term_usage_list = 
                  build_usage_list("upp_long_term_usage_list", NULL);
            lSetList(upp, UPP_long_term_usage, user_long_term_usage_list);
         }
         user_usage_list = lGetList(upp, UPP_usage);
         if (!user_usage_list) {
            user_usage_list = build_usage_list("upp_usage_list", NULL);
            lSetList(upp, UPP_usage, user_usage_list);
         }

      } else {
         user_long_term_usage_list = lGetList(user, UP_long_term_usage);
         if (!user_long_term_usage_list) {
            user_long_term_usage_list = 
                  build_usage_list("user_long_term_usage_list", NULL);
            lSetList(user, UP_long_term_usage, user_long_term_usage_list);
         }
         user_usage_list = lGetList(user, UP_usage);
         if (!user_usage_list) {
            user_usage_list = build_usage_list("user_usage_list", NULL);
            lSetList(user, UP_usage, user_usage_list);
         }
      }
   }

   if (project) {
      project_long_term_usage_list = lGetList(project, UP_long_term_usage);
      if (!project_long_term_usage_list) {
         project_long_term_usage_list =
              build_usage_list("project_long_term_usage_list", NULL);
         lSetList(project, UP_long_term_usage, project_long_term_usage_list);
      }
      project_usage_list = lGetList(project, UP_usage);
      if (!project_usage_list) {
         project_usage_list = build_usage_list("project_usage_list", 
                                                NULL);
         lSetList(project, UP_usage, project_usage_list);
      }
   }

   if (job_usage_list) {

      lListElem *job_usage;

      /*-------------------------------------------------------------
       * Add to node usage for each usage type
       *-------------------------------------------------------------*/

      for_each(job_usage, job_usage_list) {

         lListElem *old_usage=NULL, 
                   *user_usage=NULL, *project_usage=NULL,
                   *user_long_term_usage=NULL,
                   *project_long_term_usage=NULL;
         const char *usage_name = lGetString(job_usage, UA_name);

         /* only copy CPU, memory, and I/O usage */
         /* or usage explicitly in decay list */
         if (strcmp(usage_name, USAGE_ATTR_CPU) != 0 &&
             strcmp(usage_name, USAGE_ATTR_MEM) != 0 &&
             strcmp(usage_name, USAGE_ATTR_IO) != 0 &&
             !lGetElemStr(decay_list, UA_name, usage_name))
             continue;

         /*---------------------------------------------------------
          * Locate the corresponding usage element for the job
          * usage type in the node usage, old job usage, user usage,
          * and project usage.  If it does not exist, create a new
          * corresponding usage element.
          *---------------------------------------------------------*/

         if (old_usage_list) {
            old_usage = get_usage(old_usage_list, usage_name);
            if (!old_usage) {
               old_usage = create_usage_elem(usage_name);
               lAppendElem(old_usage_list, old_usage);
            }
         }

         if (user_usage_list) {
            user_usage = get_usage(user_usage_list, usage_name);
            if (!user_usage) {
               user_usage = create_usage_elem(usage_name);
               lAppendElem(user_usage_list, user_usage);
            }
         }

         if (user_long_term_usage_list) {
            user_long_term_usage = get_usage(user_long_term_usage_list,
                                                       usage_name);
            if (!user_long_term_usage) {
               user_long_term_usage = create_usage_elem(usage_name);
               lAppendElem(user_long_term_usage_list, user_long_term_usage);
            }
         }

         if (project_usage_list) {
            project_usage = get_usage(project_usage_list, usage_name);
            if (!project_usage) {
               project_usage = create_usage_elem(usage_name);
               lAppendElem(project_usage_list, project_usage);
            }
         }

         if (project_long_term_usage_list) {
            project_long_term_usage =
                  get_usage(project_long_term_usage_list, usage_name);
            if (!project_long_term_usage) {
               project_long_term_usage = create_usage_elem(usage_name);
               lAppendElem(project_long_term_usage_list,
			   project_long_term_usage);
            }
         }

         if (job_usage && old_usage) {

            double usage_value;

            usage_value = MAX(lGetDouble(job_usage, UA_value) -
                              lGetDouble(old_usage, UA_value), 0);
                                     
            /*---------------------------------------------------
             * Add usage to decayed user usage
             *---------------------------------------------------*/

            if (user_usage)
                lSetDouble(user_usage, UA_value,
                      lGetDouble(user_usage, UA_value) +
                      usage_value);

            /*---------------------------------------------------
             * Add usage to long term user usage
             *---------------------------------------------------*/

            if (user_long_term_usage)
                lSetDouble(user_long_term_usage, UA_value,
                      lGetDouble(user_long_term_usage, UA_value) +
                      usage_value);

            /*---------------------------------------------------
             * Add usage to decayed project usage
             *---------------------------------------------------*/

            if (project_usage)
                lSetDouble(project_usage, UA_value,
                      lGetDouble(project_usage, UA_value) +
                      usage_value);

            /*---------------------------------------------------
             * Add usage to long term project usage
             *---------------------------------------------------*/

            if (project_long_term_usage)
                lSetDouble(project_long_term_usage, UA_value,
                      lGetDouble(project_long_term_usage, UA_value) +
                      usage_value);


         }

      }

   }

   /*-------------------------------------------------------------
    * save off current job usage in debitted job usage list
    *-------------------------------------------------------------*/

   if (old_usage_list)
      lFreeList(old_usage_list);

   if (job_usage_list) {
      if (userprj) {
         lListElem *upu;
         u_long jobnum = lGetUlong(job, JB_job_number);
         lList *upu_list = lGetList(userprj, UP_debited_job_usage);
         if (!upu_list) {
            upu_list = lCreateList("", UPU_Type);
            lSetList(userprj, UP_debited_job_usage, upu_list);
         }
         if ((upu = lGetElemUlong(upu_list, UPU_job_number, jobnum))) {
            lSetList(upu, UPU_old_usage_list,
                     lCopyList(lGetListName(job_usage_list), job_usage_list));
         } else {
            upu = lCreateElem(UPU_Type);
            lSetUlong(upu, UPU_job_number, jobnum);
            lSetList(upu, UPU_old_usage_list,
                     lCopyList(lGetListName(job_usage_list), job_usage_list));
            lAppendElem(upu_list, upu);
         }
      }
      lFreeList(job_usage_list);
   }

}



/*--------------------------------------------------------------------
 * calc_job_share_tree_tickets_pass1 - performs pass 1 of calculating
 *      the job share tree tickets for the specified job
 *--------------------------------------------------------------------*/

static void
calc_job_share_tree_tickets_pass1( sge_ref_t *ref,
                                   double sum_m_share,
                                   double sum_proportion,
                                   double *sum_adjusted_proportion,
                                   u_long seqno )
{
   lListElem *job = ref->job;
   lListElem *node = ref->node;

   if (!node) {
      return;
   }

   /*-------------------------------------------------------
    * sum job shares for each job for use in pass 2
    *-------------------------------------------------------*/

   lSetUlong(node, STN_sum_priority,
             lGetUlong(node, STN_sum_priority) +
             lGetUlong(job, JB_jobshare));
}


/*--------------------------------------------------------------------
 * calc_job_share_tree_tickets_pass2 - performs pass 2 of calculating
 *      the job share tree tickets for the specified job
 *--------------------------------------------------------------------*/

static void
calc_job_share_tree_tickets_pass2( sge_ref_t *ref,
                                   double sum_adjusted_proportion,
                                   double total_share_tree_tickets,
                                   u_long seqno )
{
   double share_tree_tickets;
   lListElem *job = ref->job;
   lListElem *node = ref->node;

   if (!node) {
      return;
   }

   /*-------------------------------------------------------
    * calculate the number of share tree tickets for this job
    *-------------------------------------------------------*/

   share_tree_tickets = lGetDouble(node, STN_adjusted_current_proportion) *
                        total_share_tree_tickets;

   if (lGetUlong(node, STN_sum_priority)) {
      REF_SET_STICKET(ref, 
                (u_long)((double)lGetUlong(job, JB_jobshare) *
                share_tree_tickets /
                lGetUlong(node, STN_sum_priority)));
   } else {
      REF_SET_STICKET(ref,
                share_tree_tickets / lGetUlong(node, STN_job_ref_count));
   }
}

/****** sgeee/copy_ftickets() **************************************************
*  NAME
*     copy_ftickets() -- copy the ftix from one job to an other one 
*
*  SYNOPSIS
*     void copy_ftickets(sge_ref_list_t *source, sge_ref_list_t *dest) 
*
*  FUNCTION
*     Copy the functional tickets and ref fields used for ftix calculation
*     from one job to an other job. 
*
*  INPUTS
*     sge_ref_list_t *source - source job  
*     sge_ref_list_t *dest   - dest job 
*
*  BUGS
*     ??? 
*******************************************************************************/
static void copy_ftickets(sge_ref_list_t *source, sge_ref_list_t *dest){
   if(!source || !dest)
      return;
   { 
      sge_ref_t *dest_r = dest->ref;
      sge_ref_t *source_r = source->ref;
 
      dest_r->user_fshare = source_r->user_fshare;
      dest_r->project_fshare = source_r->project_fshare;
      dest_r->dept_fshare = source_r->dept_fshare;
      dest_r->jobclass_fshare = source_r->jobclass_fshare;
      dest_r->job_fshare = source_r->job_fshare;

      REF_SET_FSHARE(dest_r, lGetUlong(source_r->job, JB_jobshare));
      dest_r->total_jobclass_ftickets = source_r->total_jobclass_ftickets;
      REF_SET_FTICKET(dest_r, REF_GET_FTICKET(source_r));
   }
}

/****** sgeee/destribute_ftickets() ********************************************
*  NAME
*     destribute_ftickets() -- ensures, that all jobs have ftix asoziated with them. 
*
*  SYNOPSIS
*     void destribute_ftickets(sge_fcategory_t *root, int dependent) 
*
*  FUNCTION
*     After the functional tickets are calculated, only the first job in the fcategory
*     job list has ftix. This function copies the result from the first job to all
*     other jobs in the same list and sums the job ticket count with the ftix. 
*
*  INPUTS
*     sge_fcategory_t *root - fcategory list 
*     int dependent         - does the final ticket count depend on ftix? 
*
*
*
*  NOTES
*     - This function is only needed, because not all functional tickets are calculated
*       and to give a best guess result, all jobs in one category with no ftix get the
*       same amount of ftix. 
*
*******************************************************************************/
static void destribute_ftickets(lList *root, int dependent){   
   lListElem *elem;
   sge_ref_list_t *current = NULL;
   sge_ref_list_t *first = NULL;

   for_each(elem, root) {
      first = lGetRef(elem, FCAT_jobrelated_ticket_first);
/*      if(!first)
         continue;*/
      current = first->next;
      
      while(current != NULL) {
         copy_ftickets(first, current); 
         if(dependent)
            current->ref->tickets += REF_GET_FTICKET(current->ref);        
         current = current->next; 
      }
   }
}

/*
 * job classes are ignored.
 */
/****** sgeee/build_functional_categories() ************************************
*  NAME
*     build_functional_categories() --  sorts the pending jobs into functional categories
*
*  SYNOPSIS
*     void build_functional_categories(sge_ref_t *job_ref, int num_jobs, 
*     sge_fcategory_t **root, int dependent) 
*
*  FUNCTION
*     Generates a list of functional categories. Each category contains a list of jobs
*     which belongs to this category. A functional category is assembled of:
*     - job shares
*     - user shares
*     - department shares
*     - project shares 
*     Alljobs with the same job, user,... shares are put in the same fcategory.
*
*  INPUTS
*     sge_ref_t *job_ref     - array of pointers to the job reference structure 
*     int num_jobs           - amount of elements in the job_ref array 
*     sge_fcategory_t **root - root pointer to the functional category list 
*     int dependent          - does the functional tickets depend on prior computed tickets? 
*
*
*  NOTES
*     - job classes are ignored.
*
*  IMPROVEMENTS:
*     - the stored values in the functional category structure can be used to speed up the
*       ticket calculation. This wil avoid unnessisary CULL accesses in the function
*       calc_job_functional_tickets_pass1
*     - A further improvement can be done by:
*        - limiting the job list length in each category to the max nr of jobs calculated
*        - Sorting the jobs in each functional category by its job category. Each resulting
*          job list can be of max size of open slots. This will result in a correct ftix result
*          for all jobs, which might be scheduled.
*
*  BUGS
*     ??? 
*
*******************************************************************************/
static void build_functional_categories(sge_ref_t *job_ref, int num_jobs, lList **fcategories, int dependent) {
   lListElem *current;
   u_long32 job_ndx; 
   DENTER(TOP_LAYER,"build_functional_categories");

   for(job_ndx=0; job_ndx<num_jobs; job_ndx++) {
      sge_ref_t *jref = &job_ref[job_ndx];
      if (jref->queued) {
         u_long32 job_shares = 0;  
         u_long32 user_shares = 0;
         u_long32 dept_shares = 0;
         u_long32 project_shares = 0;
         u_long32 jobclass_shares = 0;  

         if(jref->user){
            user_shares = lGetUlong(jref->user, UP_fshare); 
         }
         if(jref->project){
            project_shares = lGetUlong(jref->project, UP_fshare); 
         }
         if(jref->dept){
            dept_shares = lGetUlong(jref->dept, US_fshare); 
         }
         job_shares =  lGetUlong(jref->job, JB_jobshare);

         /* locate the right category */
         for_each (current, *fcategories) {
            if ((lGetUlong(current, FCAT_job_share) == job_shares) &&
                (lGetRef(current, FCAT_user) == jref->user) &&
                (lGetRef(current, FCAT_dept) == jref->dept) &&
                (lGetRef(current, FCAT_project) == jref->project) &&
                (lGetUlong(current, FCAT_jobclass_share) == jobclass_shares)) {
               break;
            }
         }

         if (current == NULL) {   /* create new category */
            if (!(current = lAddElemUlong(fcategories, FCAT_job_share, job_shares, FCAT_Type))) {
               free_fcategories(fcategories);
               /* maybe an error code */
               DEXIT;
               return;
            }
            lSetUlong(current, FCAT_user_share, user_shares);
            lSetRef(current, FCAT_user, jref->user);
            lSetUlong(current, FCAT_dept_share, dept_shares);
            lSetRef(current, FCAT_dept, jref->dept);
            lSetUlong(current, FCAT_project_share, project_shares);
            lSetRef(current, FCAT_project, jref->project);
            lSetUlong(current, FCAT_jobclass_share, jobclass_shares);
         }

         {  /* create new job entry */ 
            sge_ref_list_t  *cur_ref_entry = NULL;
            sge_ref_list_t *new_elem = malloc(sizeof(sge_ref_list_t));

            if(new_elem == NULL){
               free_fcategories(fcategories);
               DEXIT;
               return;
            } 

            memset(new_elem, 0, sizeof(sge_ref_list_t));
            new_elem->ref = jref;

            /* find the location for the new element */
            cur_ref_entry = lGetRef(current, FCAT_jobrelated_ticket_last);

            if(dependent)

            while (cur_ref_entry != NULL) {
               sge_ref_t *ref = cur_ref_entry->ref;
               if (dependent){
                  if (ref->tickets > jref->tickets)
                     break;
                  else if (ref->tickets == jref->tickets){
                     if (lGetUlong(ref->job, JB_job_number) < lGetUlong(jref->job, JB_job_number))
                        break;
                     else if ((lGetUlong(ref->job, JB_job_number) == lGetUlong(jref->job, JB_job_number)) 
                           && (REF_GET_JA_TASK_NUMBER(ref) < REF_GET_JA_TASK_NUMBER(jref)))
                        break;
                  }
               }
/*               else{
                  if (lGetUlong(ref->job, JB_job_number) < lGetUlong(jref->job, JB_job_number))
                     break;
                  else if ((lGetUlong(ref->job, JB_job_number) == lGetUlong(jref->job, JB_job_number)) 
                        && (REF_GET_JA_TASK_NUMBER(ref) < REF_GET_JA_TASK_NUMBER(jref)))
                     break;

               }*/
               cur_ref_entry = cur_ref_entry->prev;
            }


            /* insert element into the list */ 
            if (cur_ref_entry == NULL) {
               if (lGetRef(current, FCAT_jobrelated_ticket_first) == NULL) {
                  /* first element, no elements in the list */
                  lSetRef(current, FCAT_jobrelated_ticket_first, new_elem);
                  lSetRef(current, FCAT_jobrelated_ticket_last, new_elem);
               } 
               else {
                  /* insert element at the beginning */
                  cur_ref_entry = lGetRef(current, FCAT_jobrelated_ticket_first);
                  cur_ref_entry->prev = new_elem;
                  new_elem->next = cur_ref_entry;
                  lSetRef(current, FCAT_jobrelated_ticket_first, new_elem); 
                  
               }
            }
            else {
               /* insert element somewhere in the list */
               new_elem->prev = cur_ref_entry;
               new_elem->next = cur_ref_entry->next;
               if(cur_ref_entry->next != NULL){
                  cur_ref_entry->next->prev = new_elem;
               }
               else{
                  /* element is the last in the list */
                  lSetRef(current, FCAT_jobrelated_ticket_last, new_elem);
               }
               cur_ref_entry->next = new_elem; 
            }
         }
      }

   }
   DEXIT;
}

/****** sgeee/free_fcategories() ***********************************************
*  NAME
*     free_fcategories() -- frees all fcategories and their job lists. 
*
*  SYNOPSIS
*     void free_fcategories(sge_fcategory_t **fcategories) 
*
*  FUNCTION
*     frees all fcategories and their job lists. 
*
*  INPUTS
*     sge_fcategory_t **fcategories /- pointer to a pointer of the first fcategory 
*
*  NOTES
*     - it does not delete the sge_ref_t structures, which are stored in
*       in the job lists. 
*
*******************************************************************************/
static void free_fcategories(lList **fcategories) {

   lListElem *fcategory;
   sge_ref_list_t *current=NULL;
   sge_ref_list_t *tmp = NULL;

   for_each(fcategory, *fcategories) {
      current = lGetRef(fcategory, FCAT_jobrelated_ticket_last);

      while(current != NULL){
         tmp = current;
         current = current->prev;
         free(tmp);
      }
      lSetRef(fcategory, FCAT_jobrelated_ticket_last, NULL);
      lSetRef(fcategory, FCAT_jobrelated_ticket_first, NULL);      
      lSetRef(fcategory, FCAT_user, NULL);
      lSetRef(fcategory, FCAT_dept, NULL);
      lSetRef(fcategory, FCAT_project, NULL);
   }
   *fcategories= lFreeList(*fcategories);


}

/*--------------------------------------------------------------------
 * calc_functional_tickets_pass1 - performs pass 1 of calculating
 *      the functional tickets for the specified job
 *--------------------------------------------------------------------*/

static void calc_job_functional_tickets_pass1( sge_ref_t *ref,
                                   double *sum_of_user_functional_shares,
                                   double *sum_of_project_functional_shares,
                                   double *sum_of_department_functional_shares,
                                   double *sum_of_jobclass_functional_shares,
                                   double *sum_of_job_functional_shares,
                                   int shared,
                                   int sum_shares)
{
   double job_cnt;

   /*-------------------------------------------------------------
    * Sum user functional shares
    *-------------------------------------------------------------*/

   if (ref->user) {
      job_cnt = lGetUlong(ref->user, UP_job_cnt);
      ref->user_fshare = shared ?
            (double)lGetUlong(ref->user, UP_fshare) / job_cnt :
            lGetUlong(ref->user, UP_fshare);
      if(sum_shares || job_cnt<=1)
         *sum_of_user_functional_shares += ref->user_fshare;

   }

   /*-------------------------------------------------------------
    * Sum project functional shares
    *-------------------------------------------------------------*/

   if (ref->project) {
      job_cnt = lGetUlong(ref->project, UP_job_cnt);
      ref->project_fshare = shared ?
            (double)lGetUlong(ref->project, UP_fshare) / job_cnt :
            lGetUlong(ref->project, UP_fshare);
      if(sum_shares || job_cnt<=1)
         *sum_of_project_functional_shares += ref->project_fshare;
   }

   /*-------------------------------------------------------------
    * Sum department functional shares
    *-------------------------------------------------------------*/

   if (ref->dept) {
      job_cnt = lGetUlong(ref->dept, US_job_cnt);
      ref->dept_fshare = shared ?
            (double)lGetUlong(ref->dept, US_fshare) / job_cnt :
            lGetUlong(ref->dept, US_fshare);
      if(sum_shares || job_cnt<=1)
         *sum_of_department_functional_shares += ref->dept_fshare;
   }

   /*-------------------------------------------------------------
    * Sum job class functional shares
    *-------------------------------------------------------------*/


#if 0  /* SVD040130 - commented out until we support job classes */

   if (ref->task_jobclass) {
      int i;
      for(i=0; i<ref->num_task_jobclasses; i++)
         if (ref->task_jobclass[i]) {
            job_cnt = lGetUlong(ref->task_jobclass[i], QU_job_cnt);
            if (sum_shares || job_cnt<=1) {
               if (shared)
                  *sum_of_jobclass_functional_shares +=
                     (double)lGetUlong(ref->task_jobclass[i], QU_fshare) / job_cnt;
               else
                  *sum_of_jobclass_functional_shares +=
                     lGetUlong(ref->task_jobclass[i], QU_fshare);
            }
         }
   } else if (ref->jobclass) {
      job_cnt = lGetUlong(ref->jobclass, QU_job_cnt);
      ref->jobclass_fshare = shared ?
            (double)lGetUlong(ref->jobclass, QU_fshare) / job_cnt :
            lGetUlong(ref->jobclass, QU_fshare);
      if(sum_shares || job_cnt<=1)
         *sum_of_jobclass_functional_shares += ref->jobclass_fshare;
   }

#endif

   /*-------------------------------------------------------------
    * Sum job functional shares
    *-------------------------------------------------------------*/

   REF_SET_FSHARE(ref, lGetUlong(ref->job, JB_jobshare));

   *sum_of_job_functional_shares += REF_GET_FSHARE(ref);
}

enum {
   k_user=0,
   k_department,
   k_project,
   k_jobclass,
   k_job,
   k_last
};


/*--------------------------------------------------------------------
 * get_functional_weighting_parameters - get the weighting parameters for
 *      the functional policy categories from the configuration
 *--------------------------------------------------------------------*/


static void
get_functional_weighting_parameters( double sum_of_user_functional_shares,
                                     double sum_of_project_functional_shares,
                                     double sum_of_department_functional_shares,
                                     double sum_of_jobclass_functional_shares,
                                     double sum_of_job_functional_shares,
                                     double weight[] )
{
   double k_sum;
   memset(weight, 0, sizeof(double)*k_last);

   if (sconf_is()) {
      if (sum_of_user_functional_shares > 0)
         weight[k_user] = sconf_get_weight_user();
      if (sum_of_department_functional_shares > 0)
         weight[k_department] = sconf_get_weight_department();
      if (sum_of_project_functional_shares > 0)
         weight[k_project] = sconf_get_weight_project();
      if (sum_of_jobclass_functional_shares > 0)
         weight[k_jobclass] = sconf_get_weight_jobclass();
      if (sum_of_job_functional_shares > 0)
         weight[k_job] = sconf_get_weight_job();
      k_sum = weight[k_user] + weight[k_department] + weight[k_project] +
              weight[k_jobclass] + weight[k_job];
   } else {
      weight[k_user] = 1;
      weight[k_department] = 1;
      weight[k_project] = 1;
      weight[k_jobclass] = 1;
      weight[k_job] = 1;
      k_sum = weight[k_user] + weight[k_department] + weight[k_project] +
              weight[k_jobclass] + weight[k_job];
   }

   if (k_sum>0) {
      weight[k_user] /= k_sum;
      weight[k_department] /= k_sum;
      weight[k_project] /= k_sum;
      weight[k_jobclass] /= k_sum;
      weight[k_job] /= k_sum;
   }

   return;
}


/*--------------------------------------------------------------------
 * calc_functional_tickets_pass2 - performs pass 2 of calculating
 *      the functional tickets for the specified job
 *--------------------------------------------------------------------*/

static double
calc_job_functional_tickets_pass2( sge_ref_t *ref,
                                   double sum_of_user_functional_shares,
                                   double sum_of_project_functional_shares,
                                   double sum_of_department_functional_shares,
                                   double sum_of_jobclass_functional_shares,
                                   double sum_of_job_functional_shares,
                                   double total_functional_tickets,
                                   double weight[],
                                   int shared)
{
   double user_functional_tickets=0,
          project_functional_tickets=0,
          department_functional_tickets=0,
          jobclass_functional_tickets=0,
          job_functional_tickets=0,
          total_job_functional_tickets;

   /*-------------------------------------------------------
    * calculate user functional tickets for this job
    *-------------------------------------------------------*/

   if (ref->user && sum_of_user_functional_shares)
      user_functional_tickets = (ref->user_fshare *
                                total_functional_tickets /
                                sum_of_user_functional_shares);

   /*-------------------------------------------------------
    * calculate project functional tickets for this job
    *-------------------------------------------------------*/

   if (ref->project && sum_of_project_functional_shares)
      project_functional_tickets = (ref->project_fshare *
                                 total_functional_tickets /
                                 sum_of_project_functional_shares);

   /*-------------------------------------------------------
    * calculate department functional tickets for this job
    *-------------------------------------------------------*/

   if (ref->dept && sum_of_department_functional_shares)
      department_functional_tickets = (ref->dept_fshare *
                                 total_functional_tickets /
                                 sum_of_department_functional_shares);


#if 0  /* SVD040130 - commented out until we support job classes */

   /*-------------------------------------------------------
    * calculate job class functional tickets for this job
    *-------------------------------------------------------*/

   if (ref->task_jobclass && sum_of_jobclass_functional_shares) {
      int i;
      double qshares=0, job_cnt;
      for(i=0; i<ref->num_task_jobclasses; i++)
         if (ref->task_jobclass[i]) {
            job_cnt = lGetUlong(ref->task_jobclass[i], QU_job_cnt);
            qshares += shared ?
                  (double)lGetUlong(ref->task_jobclass[i], QU_fshare) / job_cnt :
                  lGetUlong(ref->task_jobclass[i], QU_fshare);
         }

      /* NOTE: Should we divide qshares by the number of task job classes
               to get an average across the associated queues? */

      jobclass_functional_tickets = (qshares *
                                 total_functional_tickets /
                                 sum_of_jobclass_functional_shares);

   } else if (ref->jobclass && sum_of_jobclass_functional_shares)
      jobclass_functional_tickets = (ref->jobclass_fshare *
                                 total_functional_tickets /
                                 sum_of_jobclass_functional_shares);

#endif

   /*-------------------------------------------------------
    * calculate job functional tickets for this job
    *-------------------------------------------------------*/

   if (sum_of_job_functional_shares)
      job_functional_tickets = ((double)lGetUlong(ref->job, JB_jobshare) *
                                 (double)total_functional_tickets /
                                  sum_of_job_functional_shares);


#if 0  /* SVD040130 - commented out until we support job classes */

   /*-------------------------------------------------------
    * calculate functional tickets for PE tasks
    *-------------------------------------------------------*/

   if (ref->ja_task && ref->task_jobclass) {
      lListElem *granted_pe;
      int i=0;
      double task_jobclass_functional_tickets;

      for_each(granted_pe, lGetList(ref->ja_task, JAT_granted_destin_identifier_list)) {
         if (ref->task_jobclass[i]) {
            if (sum_of_jobclass_functional_shares) {
               double task_jobclass_fshare, job_cnt;
               job_cnt = lGetUlong(ref->task_jobclass[i], QU_job_cnt);
               task_jobclass_fshare = shared ?
                  (double)lGetUlong(ref->task_jobclass[i], QU_fshare) / job_cnt :
                  lGetUlong(ref->task_jobclass[i], QU_fshare);
               task_jobclass_functional_tickets = (task_jobclass_fshare *
                                        total_functional_tickets /
                                        (double)sum_of_jobclass_functional_shares);
            } else
               task_jobclass_functional_tickets = 0;

            lSetDouble(granted_pe, JG_jcfticket, weight[k_jobclass] * task_jobclass_functional_tickets);
         } else
            lSetDouble(granted_pe, JG_jcfticket, 0);
         i++;
      }
   }

#endif

   /*-------------------------------------------------------
    * calculate functional tickets for this job
    *-------------------------------------------------------*/

   total_job_functional_tickets = weight[k_user] * user_functional_tickets +
             weight[k_department] * department_functional_tickets +
             weight[k_project] * project_functional_tickets +
             weight[k_jobclass] * jobclass_functional_tickets +
             weight[k_job] * job_functional_tickets;

   ref->total_jobclass_ftickets = weight[k_jobclass] * jobclass_functional_tickets;
   REF_SET_FTICKET(ref, total_job_functional_tickets);

   return job_functional_tickets;
}


/*--------------------------------------------------------------------
 * calc_override_tickets - calculates the number of override tickets for the
 * specified job
 *--------------------------------------------------------------------*/

static double
calc_job_override_tickets( sge_ref_t *ref,
                           int shared)
{
   double job_override_tickets = 0;
   double otickets, job_cnt;

   DENTER(TOP_LAYER, "calc_job_override_tickets");

   /*-------------------------------------------------------
    * job.override_tickets = user.override_tickets +
    *                        project.override_tickets +
    *                        department.override_tickets +
    *                        jobclass.override_tickets +
    *                        job.override_tickets;
    *-------------------------------------------------------*/

   if (ref->user) {
      job_cnt = lGetUlong(ref->user, UP_job_cnt);
      if (((otickets = lGetUlong(ref->user, UP_oticket)) &&
          ((job_cnt = shared ? job_cnt : 1))))
         job_override_tickets += (otickets / job_cnt);
   }

   if (ref->project) {
      job_cnt = lGetUlong(ref->project, UP_job_cnt);
      if (((otickets = lGetUlong(ref->project, UP_oticket)) &&
          ((job_cnt = shared ? job_cnt : 1))))
         job_override_tickets += (otickets / job_cnt);
   }

   if (ref->dept) {
      job_cnt = lGetUlong(ref->dept, US_job_cnt);
      if (((otickets = lGetUlong(ref->dept, US_oticket)) &&
          ((job_cnt = shared ? job_cnt : 1))))
         job_override_tickets += (otickets / job_cnt);
   }

   job_override_tickets += lGetUlong(ref->job, JB_override_tickets);


#if 0  /* SVD040130 - commented out until we support job classes */

   if (ref->ja_task && ref->task_jobclass) {
      lListElem *granted_pe;
      double jobclass_otickets;
      int i=0;
      ref->total_jobclass_otickets=0;
      for_each(granted_pe, lGetList(ref->ja_task, JAT_granted_destin_identifier_list)) {
         jobclass_otickets = 0;
         if (ref->task_jobclass[i]) {
            job_cnt = lGetUlong(ref->task_jobclass[i], QU_job_cnt);
            if (((otickets = lGetUlong(ref->task_jobclass[i], QU_oticket)) &&
                ((job_cnt = shared ? job_cnt : 1))))
               jobclass_otickets = (otickets / (double)job_cnt);

            ref->total_jobclass_otickets += jobclass_otickets;
         }

         lSetDouble(granted_pe, JG_jcoticket, jobclass_otickets);
         i++;
      }
      job_override_tickets += ref->total_jobclass_otickets;
   } else if (ref->jobclass) {
      job_cnt = lGetUlong(ref->jobclass, QU_job_cnt);
      if (((otickets = lGetUlong(ref->jobclass, QU_oticket)) &&
          ((job_cnt = shared ? job_cnt : 1))))
         job_override_tickets += (otickets / job_cnt);
   }

#endif

   REF_SET_OTICKET(ref, job_override_tickets);

   DEXIT;
   return job_override_tickets;
}


/*--------------------------------------------------------------------
 * calc_job_tickets - calculates the total number of tickets for
 * the specified job
 *--------------------------------------------------------------------*/

static double
calc_job_tickets ( sge_ref_t *ref )
{
   lList *granted;
   lListElem *job = ref->job,
             *ja_task = ref->ja_task;
   double job_tickets;
   lListElem *pe, *granted_el;
   const char *pe_str;

   /*-------------------------------------------------------------
    * Sum up all tickets for job
    *-------------------------------------------------------------*/

   job_tickets = REF_GET_STICKET(ref) + REF_GET_FTICKET(ref) +
                 REF_GET_OTICKET(ref);

   REF_SET_TICKET(ref, job_tickets);

   /* for PE slave-controlled jobs, set tickets for each granted queue */

   if (ja_task && (pe_str=lGetString(ja_task, JAT_granted_pe))
         && (pe=pe_list_locate(all_lists->pe_list, pe_str))
         && lGetBool(pe, PE_control_slaves)
         && (granted=lGetList(ja_task, JAT_granted_destin_identifier_list))) {

      double job_tickets_per_slot, job_otickets_per_slot,
             job_ftickets_per_slot, job_stickets_per_slot, nslots, active_nslots;

      /* Here we get the total number of slots granted on all queues (nslots) and the
         total number of slots granted on all queues with active subtasks or
         on which no subtasks have started yet (active_nslots). */

      nslots = nslots_granted(granted, NULL);
      active_nslots = active_nslots_granted(job, granted, NULL);

      /* If there are some active_nslots, then calculate the number of tickets
         per slot based on the active_nslots */

      if (active_nslots > 0)
         nslots = active_nslots;

      if (nslots > 0) {
         job_stickets_per_slot = REF_GET_STICKET(ref)/nslots;
      } else {
         job_stickets_per_slot = 0;
      }

      for_each(granted_el, granted) {
         double slots;

         /* Only give tickets to queues with active sub-tasks or sub-tasks
            which have not started yet. */
            
         if (active_nslots > 0 && !active_subtasks(job, lGetString(granted_el, JG_qname)))
            slots = 0;
         else
            slots = lGetUlong(granted_el, JG_slots);
         
         if (nslots > 0) {
            job_ftickets_per_slot = (double)(REF_GET_FTICKET(ref) - ref->total_jobclass_ftickets)/nslots;
            job_otickets_per_slot = (double)(REF_GET_OTICKET(ref) - ref->total_jobclass_otickets)/nslots;
            job_tickets_per_slot = job_stickets_per_slot + job_ftickets_per_slot + job_otickets_per_slot;
         } else {
            job_ftickets_per_slot = 0;
            job_otickets_per_slot = 0;
            job_tickets_per_slot = 0;
         }

         lSetDouble(granted_el, JG_fticket, job_ftickets_per_slot*slots + lGetDouble(granted_el, JG_jcfticket));
         lSetDouble(granted_el, JG_oticket, job_otickets_per_slot*slots + lGetDouble(granted_el, JG_jcoticket));
         lSetDouble(granted_el, JG_sticket, job_stickets_per_slot*slots);
         lSetDouble(granted_el, JG_ticket, job_tickets_per_slot*slots +
                   lGetDouble(granted_el, JG_jcoticket) + lGetDouble(granted_el, JG_jcfticket));

      }
   }

   return job_tickets;
}


/*--------------------------------------------------------------------
 * sge_clear_job - clear tickets for job
 *--------------------------------------------------------------------*/

static void
sge_clear_job( lListElem *job )
{
   lListElem *ja_task;

   for_each(ja_task, lGetList(job, JB_ja_tasks))
      sge_clear_ja_task(ja_task);
}

/*--------------------------------------------------------------------
 * sge_clear_ja_task - clear tickets for job task
 *--------------------------------------------------------------------*/

static void
sge_clear_ja_task( lListElem *ja_task )
{
   lListElem *granted_el;

   lSetDouble(ja_task, JAT_prio, 0);
   lSetDouble(ja_task, JAT_ntix, 0);
   lSetDouble(ja_task, JAT_tix, 0);
   lSetDouble(ja_task, JAT_oticket, 0);
   lSetDouble(ja_task, JAT_fticket, 0);
   lSetDouble(ja_task, JAT_sticket, 0);
   lSetDouble(ja_task, JAT_share, 0);
   for_each(granted_el, lGetList(ja_task, JAT_granted_destin_identifier_list)) {
      lSetDouble(granted_el, JG_ticket, 0);
      lSetDouble(granted_el, JG_oticket, 0);
      lSetDouble(granted_el, JG_fticket, 0);
      lSetDouble(granted_el, JG_sticket, 0);
      lSetDouble(granted_el, JG_jcoticket, 0);
      lSetDouble(granted_el, JG_jcfticket, 0);
   }
}

/****** sgeee/calc_intern_pending_job_functional_tickets() *********************
*  NAME
*     calc_intern_pending_job_functional_tickets() -- calc ftix for pending jobs 
*
*  SYNOPSIS
*     void calc_intern_pending_job_functional_tickets(sge_fcategory_t *current, 
*     double sum_of_user_functional_shares, double 
*     sum_of_project_functional_shares, double 
*     sum_of_department_functional_shares, double 
*     sum_of_jobclass_functional_shares, double sum_of_job_functional_shares, 
*     double total_functional_tickets, double weight[]) 
*
*  FUNCTION
*     This is an optimized and incomplete version of calc_pending_job_functional_tickets.
*     It is good enough to get the order right within the inner loop of the ftix
*     calculation. 
*
*  INPUTS
*     sge_fcategory_t *current                   - current fcategory 
*     double sum_of_user_functional_shares       
*     double sum_of_project_functional_shares     
*     double sum_of_department_functional_shares   
*     double sum_of_jobclass_functional_shares     
*     double sum_of_job_functional_shares          
*     double total_functional_tickets              
*     double weight[]                            - destribution of the shares to each other  
*
*
*  NOTES
*     be carefull using it 
*
*  BUGS
*     ??? 
*
*******************************************************************************/
static void calc_intern_pending_job_functional_tickets(
                                    lListElem *current,
                                    double sum_of_user_functional_shares,
                                    double sum_of_project_functional_shares,
                                    double sum_of_department_functional_shares,
                                    double sum_of_jobclass_functional_shares,
                                    double sum_of_job_functional_shares,
                                    double total_functional_tickets,
                                    double weight[],
                                    bool share_functional_shares)
{
   sge_ref_list_t *tmp = (lGetRef(current, FCAT_jobrelated_ticket_first)); 
   sge_ref_t *ref = tmp->ref; 

   /*-------------------------------------------------------------
    * Sum user functional shares
    *-------------------------------------------------------------*/

   if (ref->user) {
      ref->user_fshare = lGetUlong(current, FCAT_user_share);
      if(share_functional_shares)
         ref->user_fshare /= lGetUlong(ref->user, UP_job_cnt) +1 ; 
      else 
         sum_of_user_functional_shares += ref->user_fshare;
   }

   /*-------------------------------------------------------------
    * Sum project functional shares
    *-------------------------------------------------------------*/

   if (ref->project) { 
      ref->project_fshare = lGetUlong(current, FCAT_project_share);
      if(share_functional_shares) 
         ref->project_fshare /= (lGetUlong(ref->project, UP_job_cnt) +1);

      else      
         sum_of_project_functional_shares += ref->project_fshare;
   }


   /*-------------------------------------------------------------
    * Sum department functional shares
    *-------------------------------------------------------------*/

   if (ref->dept) { 
      ref->dept_fshare = lGetUlong(current, FCAT_dept_share);
      if(share_functional_shares)
            ref->dept_fshare /= lGetUlong(ref->dept, US_job_cnt) +1;
      else 
         sum_of_department_functional_shares += ref->dept_fshare;
   }

   /*-------------------------------------------------------------
    * Sum job functional shares
    *-------------------------------------------------------------*/

   REF_SET_FSHARE(ref, lGetUlong(current, FCAT_job_share));
   sum_of_job_functional_shares += REF_GET_FSHARE(ref);

   calc_job_functional_tickets_pass2(ref,
                                     sum_of_user_functional_shares,
                                     sum_of_project_functional_shares,
                                     sum_of_department_functional_shares,
                                     sum_of_jobclass_functional_shares,
                                     sum_of_job_functional_shares,
                                     total_functional_tickets,
                                     weight,
                                     share_functional_shares);

   return;
}

static void calc_pending_job_functional_tickets(sge_ref_t *ref,
                                    double *sum_of_user_functional_shares,
                                    double *sum_of_project_functional_shares,
                                    double *sum_of_department_functional_shares,
                                    double *sum_of_jobclass_functional_shares,
                                    double *sum_of_job_functional_shares,
                                    double total_functional_tickets,
                                    double weight[],
                                    int shared)
{
   /*
    * If the functional shares of a given object are shared between jobs
    * (schedd_param SHARE_FUNCTIONAL_SHARES=true), then we need to reduce
    * the total number of shares for each category to include the object
    * shares only one time. If the object reference count is greater than
    * one, the object's shares were already included in the total before
    * calc_job_functional_tickets_pass1 was called, so we subtract them
    * out here.
   */
   calc_job_functional_tickets_pass1(ref,
                                     sum_of_user_functional_shares,
                                     sum_of_project_functional_shares,
                                     sum_of_department_functional_shares,
                                     sum_of_jobclass_functional_shares,
                                     sum_of_job_functional_shares,
                                     shared,
                                     !shared);

   calc_job_functional_tickets_pass2(ref,
                                     *sum_of_user_functional_shares,
                                     *sum_of_project_functional_shares,
                                     *sum_of_department_functional_shares,
                                     *sum_of_jobclass_functional_shares,
                                     *sum_of_job_functional_shares,
                                     total_functional_tickets,
                                     weight,
                                     shared);
   return;
}

/*--------------------------------------------------------------------
 * sge_calc_tickets - calculate proportional shares in terms of tickets
 * for all active jobs.
 *
 * queued_jobs should contain all pending jobs to be scheduled
 * running_jobs should contain all currently executing jobs
 * finished_jobs should contain all completed jobs
 *--------------------------------------------------------------------*/

static int
sge_calc_tickets( sge_Sdescr_t *lists,
                  lList *running_jobs,
                  lList *finished_jobs,
                  lList *queued_jobs,
                  int do_usage )
{
   double sum_of_user_functional_shares = 0,
          sum_of_project_functional_shares = 0,
          sum_of_department_functional_shares = 0,
          sum_of_jobclass_functional_shares = 0,
          sum_of_job_functional_shares = 0,
          sum_of_active_tickets = 0,
          sum_of_pending_tickets = 0,
          sum_of_active_override_tickets = 0;

   double sum_of_proportions = 0,
          sum_of_m_shares = 0,
          sum_of_adjusted_proportions = 0;

   u_long curr_time, num_jobs, num_queued_jobs, job_ndx;

   u_long32 num_unenrolled_tasks = 0;

   sge_ref_t *job_ref = NULL;

   lListElem *job, *qep, *root = NULL;

   double prof_init=0, prof_pass0=0, prof_pass1=0, prof_pass2=0, prof_calc=0; 

   double total_share_tree_tickets = sconf_get_weight_tickets_share();
   double total_functional_tickets = sconf_get_weight_tickets_functional();
   
   bool share_functional_shares = sconf_get_share_functional_shares();
   u_long32 max_pending_tasks_per_job = sconf_get_max_pending_tasks_per_job();
   static int halflife = 0;

   lList *decay_list = NULL;

   u_long32 free_qslots = 0;

   DENTER(TOP_LAYER, "sge_calc_tickets");
  
   PROF_START_MEASUREMENT(SGE_PROF_SCHEDLIB4);

   all_lists = lists;

   curr_time = sge_get_gmt();

   sge_scheduling_run++;
   { 
      const lList *halflife_decay_list = sconf_get_halflife_decay_list();
      if (halflife_decay_list) {
         lListElem *ep, *u;
         double decay_rate, decay_constant;
         for_each(ep, halflife_decay_list) {
            calculate_decay_constant(lGetDouble(ep, UA_value),
                                    &decay_rate, &decay_constant);
            u = lAddElemStr(&decay_list, UA_name, lGetString(ep, UA_name),
                           UA_Type); 
            lSetDouble(u, UA_value, decay_constant);
         }
      } else {
         lListElem *u;
         double decay_rate, decay_constant;
         calculate_decay_constant(-1, &decay_rate, &decay_constant);
         u = lAddElemStr(&decay_list, UA_name, "finished_jobs", UA_Type); 
         lSetDouble(u, UA_value, decay_constant);
      }
   }

   /*-------------------------------------------------------------
    * Decay usage for all users and projects if halflife changes
    *-------------------------------------------------------------*/

   if (do_usage && sconf_is() && halflife != sconf_get_halftime()) {
      lListElem *userprj;
      int oldhalflife = halflife;
      halflife = sconf_get_halftime(); 
      /* decay up till now based on old half life (unless it's zero),
         all future decay will be based on new halflife */
      if (oldhalflife == 0)
         calculate_default_decay_constant(halflife);
      else
         calculate_default_decay_constant(oldhalflife);
      for_each(userprj, lists->user_list)
         decay_userprj_usage(userprj, decay_list, sge_scheduling_run, curr_time);
      for_each(userprj, lists->project_list)
         decay_userprj_usage(userprj, decay_list, sge_scheduling_run, curr_time);
   } else
      calculate_default_decay_constant(sconf_get_halftime());

   /*-------------------------------------------------------------
    * Init job_ref_count in each share tree node to zero
    *-------------------------------------------------------------*/

   if ((lists->share_tree))
      if ((root = lFirst(lists->share_tree))) {
         sge_init_share_tree_nodes(root);
         set_share_tree_project_flags(lists->project_list, root);
      }

   for_each(qep, lists->queue_list)
      free_qslots += MAX(0, lGetUlong(qep, QU_job_slots) - qinstance_slots_used(qep));

   /*-----------------------------------------------------------------
    * Create and build job reference array.  The job reference array
    * contains all the references of the jobs to the various objects
    * needed for scheduling. It exists so that the ticket calculation
    * function only has to look up the various objects once.
    *-----------------------------------------------------------------*/
   
   num_jobs = num_queued_jobs = 0;
   if (queued_jobs) {
      for_each(job, queued_jobs) {
         u_long32 max = MIN(max_pending_tasks_per_job, free_qslots+1);
         u_long32 num_unenrolled_tasks_for_job = 0;
         lListElem *ja_task;
         int task_cnt = 0;

         for_each(ja_task, lGetList(job, JB_ja_tasks)) {
            if (++task_cnt > max) {
               break;
            }
            if (job_is_active(job, ja_task)) {
               num_queued_jobs++;
               num_jobs++;
            }
         }
         if (!(task_cnt > max)) {
            int space = max - task_cnt;
            int tasks = job_get_not_enrolled_ja_tasks(job);

            num_unenrolled_tasks_for_job = MIN(space, tasks);
            num_unenrolled_tasks += num_unenrolled_tasks_for_job;
            num_queued_jobs += num_unenrolled_tasks_for_job;
            num_jobs += num_unenrolled_tasks_for_job;
         }        
      }
   }
   if (running_jobs) {
      for_each(job, running_jobs) {
         lListElem *ja_task;
         for_each(ja_task, lGetList(job, JB_ja_tasks))
            if (job_is_active(job, ja_task))
               num_jobs++;
      }
   }
   if (num_jobs > 0) {
      job_ref = (sge_ref_t *)malloc(num_jobs * sizeof(sge_ref_t));
      memset(job_ref, 0, num_jobs * sizeof(sge_ref_t));
   }
   if (num_unenrolled_tasks > 0) {
      task_ref_initialize_table(num_unenrolled_tasks);
   }

   /*-----------------------------------------------------------------
    * Note: use of the the job_ref_t array assumes that no jobs
    * will be removed from the job lists and the job list order will
    * be maintained during the execution of sge_calc_tickets.
    *-----------------------------------------------------------------*/

   job_ndx = 0;

   if (running_jobs) {
      for_each(job, running_jobs) {
         lListElem *ja_task;

         for_each(ja_task, lGetList(job, JB_ja_tasks)) {
            if (!job_is_active(job, ja_task)) {
               sge_clear_ja_task(ja_task);
            } else {
               sge_set_job_refs(job, ja_task, &job_ref[job_ndx], NULL, lists, 0);
               if (job_ref[job_ndx].node) {
                  lSetUlong(job_ref[job_ndx].node, STN_job_ref_count,
                            lGetUlong(job_ref[job_ndx].node, STN_job_ref_count)+1);
                  lSetUlong(job_ref[job_ndx].node, STN_active_job_ref_count,
                            lGetUlong(job_ref[job_ndx].node, STN_active_job_ref_count)+1);
               }
               job_ndx++;
            }
         }
      }
   }

   if (queued_jobs) {
      u_long32 ja_task_ndx = 0;

      for_each(job, queued_jobs) {
         lListElem *ja_task = NULL;
         lListElem *range = NULL;
         lList *range_list = NULL;  
         u_long32 id;
         int task_cnt = 0;

         for_each(ja_task, lGetList(job, JB_ja_tasks)) {
            sge_ref_t *jref = &job_ref[job_ndx];

            if (++task_cnt > MIN(max_pending_tasks_per_job, free_qslots+1) ||
                !job_is_active(job, ja_task)) {
               sge_clear_ja_task(ja_task);
            } else {
               sge_set_job_refs(job, ja_task, jref, NULL, lists, 1);
               if (jref->node)
                  lSetUlong(jref->node, STN_job_ref_count,
                            lGetUlong(jref->node, STN_job_ref_count)+1);
               job_ndx++;
            }
         }

         range_list = lGetList(job, JB_ja_n_h_ids); 
         for_each(range, range_list) {
            for(id = lGetUlong(range, RN_min);
                id <= lGetUlong(range, RN_max);
                id += lGetUlong(range, RN_step)) {  
               sge_ref_t *jref = &job_ref[job_ndx];
               sge_task_ref_t *tref = task_ref_get_entry(ja_task_ndx);

               if (++task_cnt > MIN(max_pending_tasks_per_job, 
                                    free_qslots+1)) {
                  break;
               } 
               ja_task = job_get_ja_task_template_pending(job, id);
               sge_set_job_refs(job, ja_task, jref, tref, lists, 1); 
               if (jref->node) {
                  lSetUlong(jref->node, STN_job_ref_count,
                            lGetUlong(jref->node, STN_job_ref_count)+1);
               }
               ja_task_ndx++;
               job_ndx++;
            }
         }
      }
   }

   num_jobs = job_ndx;

   /*-------------------------------------------------------------
    * Calculate the m_shares in the share tree.  The m_share
    * is a hierarchichal share value used in the "classic
    * scheduling" mode. It is calculated by subdividing the shares
    * at each active level in the share tree.
    *-------------------------------------------------------------*/
     
   if (root) {
      update_job_ref_count(root);
      update_active_job_ref_count(root);
      lSetDouble(root, STN_m_share, 1.0);
      calculate_m_shares(root);
   }

   /*-----------------------------------------------------------------
    * Handle finished jobs. We add the finished job usage to the
    * user and project objects and then we delete the debited job
    * usage from the associated user/project object.  The debited
    * job usage is a usage list which is maintained in the user/project
    * object to indicate how much of the job's usage has been added
    * to the user/project usage so far.
    * We also add a usage value called finished_jobs to the job usage
    * so it will be summed and decayed in the user and project usage.
    * If the finished_jobs usage value already exists, then we know
    * we already handled this finished job, but the qmaster hasn't
    * been updated yet.
    *-----------------------------------------------------------------*/

   if (do_usage && finished_jobs) {
      for_each(job, finished_jobs) {
         sge_ref_t jref;
         lListElem *ja_task;

         for_each(ja_task, lGetList(job, JB_ja_tasks)) {
            memset(&jref, 0, sizeof(jref));
            sge_set_job_refs(job, ja_task, &jref, NULL, lists, 0);
            if (lGetElemStr(lGetList(jref.ja_task, JAT_scaled_usage_list),
                            UA_name, "finished_jobs") == NULL) {
               lListElem *u;
               if ((u=lAddSubStr(jref.ja_task, UA_name, "finished_jobs",
                                 JAT_scaled_usage_list, UA_Type)))
                  lSetDouble(u, UA_value, 1.0);
               decay_and_sum_usage(&jref, decay_list, sge_scheduling_run, curr_time);
               DPRINTF(("DDJU (0) "u32"."u32"\n",
                  lGetUlong(job, JB_job_number),
                  lGetUlong(ja_task, JAT_task_number)));
               delete_debited_job_usage(&jref, sge_scheduling_run);
            }
         }
      }
   } else {
      DPRINTF(("\n"));
      DPRINTF(("no DDJU: do_usage: %d finished_jobs %d\n",
         do_usage, (finished_jobs!=NULL)));
      DPRINTF(("\n"));
   }     
   
   PROF_STOP_MEASUREMENT(SGE_PROF_SCHEDLIB4);
   prof_init = prof_get_measurement_utime(SGE_PROF_SCHEDLIB4,false, NULL);
   PROF_START_MEASUREMENT(SGE_PROF_SCHEDLIB4); 

   /*-----------------------------------------------------------------
    * PASS 0
    *-----------------------------------------------------------------*/

   DPRINTF(("=====================[SGEEE Pass 0]======================\n"));

   for(job_ndx=0; job_ndx<num_jobs; job_ndx++) {

      sge_set_job_cnts(&job_ref[job_ndx], job_ref[job_ndx].queued);

      /*-------------------------------------------------------------
       * Handle usage
       *-------------------------------------------------------------*/

      if (do_usage)
         decay_and_sum_usage(&job_ref[job_ndx], decay_list, sge_scheduling_run, curr_time);

      combine_usage(&job_ref[job_ndx]);
   }

   if (root)
      sge_calc_sharetree_targets(root, lists, decay_list,
                                 curr_time, sge_scheduling_run);

   PROF_STOP_MEASUREMENT(SGE_PROF_SCHEDLIB4);
   prof_pass0 = prof_get_measurement_utime(SGE_PROF_SCHEDLIB4,false, NULL);
   PROF_START_MEASUREMENT(SGE_PROF_SCHEDLIB4); 

   /*-----------------------------------------------------------------
    * PASS 1
    *-----------------------------------------------------------------*/

   DPRINTF(("=====================[SGEEE Pass 1]======================\n"));

   for(job_ndx=0; job_ndx<num_jobs; job_ndx++) {
      u_long job_deadline_time;

      if (job_ref[job_ndx].queued)
         break;

      job_deadline_time = lGetUlong(job_ref[job_ndx].job, JB_deadline);

      if (total_share_tree_tickets > 0)
         calc_job_share_tree_tickets_pass1(&job_ref[job_ndx],
                                           sum_of_m_shares,
                                           sum_of_proportions,
                                           &sum_of_adjusted_proportions,
                                           sge_scheduling_run);

      if (total_functional_tickets > 0)
         calc_job_functional_tickets_pass1(&job_ref[job_ndx],
                                          &sum_of_user_functional_shares,
                                          &sum_of_project_functional_shares,
                                          &sum_of_department_functional_shares,
                                          &sum_of_jobclass_functional_shares,
                                          &sum_of_job_functional_shares,
                                          share_functional_shares, 1);
   }

   PROF_STOP_MEASUREMENT(SGE_PROF_SCHEDLIB4);
   prof_pass1= prof_get_measurement_utime(SGE_PROF_SCHEDLIB4,false, NULL);
   PROF_START_MEASUREMENT(SGE_PROF_SCHEDLIB4); 

   /*-----------------------------------------------------------------
    * PASS 2
    *-----------------------------------------------------------------*/

   DPRINTF(("=====================[SGEEE Pass 2]======================\n"));
   { 
      double weight[k_last];
      bool share_override_tickets = sconf_get_share_override_tickets();
      if(total_functional_tickets > 0)
         get_functional_weighting_parameters(sum_of_user_functional_shares,
                                       sum_of_project_functional_shares,
                                       sum_of_department_functional_shares,
                                       sum_of_jobclass_functional_shares,
                                       sum_of_job_functional_shares,
                                       weight);

      for(job_ndx=0; job_ndx<num_jobs; job_ndx++) {

         if (job_ref[job_ndx].queued)
            break;

         job = job_ref[job_ndx].job;

         if (total_share_tree_tickets > 0)
            calc_job_share_tree_tickets_pass2(&job_ref[job_ndx],
                                           sum_of_adjusted_proportions,
                                           total_share_tree_tickets,
                                           sge_scheduling_run);

         if (total_functional_tickets > 0)
            calc_job_functional_tickets_pass2(&job_ref[job_ndx],
                                           sum_of_user_functional_shares,
                                           sum_of_project_functional_shares,
                                           sum_of_department_functional_shares,
                                           sum_of_jobclass_functional_shares,
                                           sum_of_job_functional_shares,
                                           total_functional_tickets,
                                           weight,
                                           share_functional_shares);

         sum_of_active_override_tickets +=
                  calc_job_override_tickets(&job_ref[job_ndx], share_override_tickets);

         sum_of_active_tickets += calc_job_tickets(&job_ref[job_ndx]);

      }
   }

   /* set scheduler configuration information to go back to GUI */
   if (sconf_is()) {
      sconf_set_weight_tickets_override(sum_of_active_override_tickets);
   }

   PROF_STOP_MEASUREMENT(SGE_PROF_SCHEDLIB4);
   prof_pass2 = prof_get_measurement_utime(SGE_PROF_SCHEDLIB4,false, NULL);
   PROF_START_MEASUREMENT(SGE_PROF_SCHEDLIB4); 

   /*-----------------------------------------------------------------
    * NEW PENDING JOB TICKET CALCULATIONS
    *-----------------------------------------------------------------*/
   if (queued_jobs) {
      lList *sorted_job_node_list;

      policy_hierarchy_t hierarchy[4];
      int policy_ndx;

      sconf_ph_fill_array(hierarchy);
      sconf_ph_print_array(hierarchy);  

      for(policy_ndx = 0; 
          policy_ndx < sizeof(hierarchy) / sizeof(policy_hierarchy_t); 
          policy_ndx++) {

         switch(hierarchy[policy_ndx].policy) {

         case SHARE_TREE_POLICY:
         DPRINTF(("calc share tree pending tickets: %.3f\n", total_share_tree_tickets));
      /*-----------------------------------------------------------------
       * Calculate pending share tree tickets
       *
       *    The share tree tickets for pending jobs are calculated
       *    by adding each job to the share tree as leaf nodes
       *    under the associated user/project node. The jobs are
       *    then sorted based on their respective short term
       *    entitlements to ensure that the overall goals of the
       *    share tree are met.
       *
       *-----------------------------------------------------------------*/

      if (total_share_tree_tickets > 0 && root) {

         for(job_ndx=0; job_ndx<num_jobs; job_ndx++) {
            sge_ref_t *jref = &job_ref[job_ndx];
            lListElem *node = jref->node;
            if (jref->queued) {
               if (node) {
                  /* add each job to the share tree as a temporary sibling node */
                  char tmpstr[64];
                  lListElem *child;

                  job = jref->job;
                  sprintf(tmpstr, u32"."u32, lGetUlong(job, JB_job_number),
                         REF_GET_JA_TASK_NUMBER(jref));
                  child = lAddSubStr(node, STN_name, tmpstr, STN_children, STN_Type);
                  lSetUlong(child, STN_jobid, lGetUlong(job, JB_job_number));
                  lSetUlong(child, STN_taskid, REF_GET_JA_TASK_NUMBER(jref));
                  lSetUlong(child, STN_temp, 1);
                  /* save the job reference, so we can refer to it later to set job fields */
                  lSetUlong(child, STN_ref, job_ndx+1);
                  if (hierarchy[policy_ndx].dependent) {
                     /* set the sort value based on tickets of higher level policy */
                     lSetDouble(child, STN_tickets, jref->tickets);
                     lSetDouble(child, STN_sort,
                                jref->tickets + (0.01 * (double)lGetUlong(job, JB_jobshare)));
                  } else
                     /* set the sort value based on the priority of the job */
                     lSetDouble(child, STN_sort, (double)lGetUlong(job, JB_jobshare));
               }
            }
         }

         if ((sorted_job_node_list = sge_sort_pending_job_nodes(root, root, total_share_tree_tickets))) {
            lListElem *job_node;

            /* 
             * set share tree tickets of each pending job 
             * based on the returned sorted node list 
             */
            for_each(job_node, sorted_job_node_list) {
               sge_ref_t *jref = &job_ref[lGetUlong(job_node, STN_ref)-1];
               REF_SET_STICKET(jref, 
                     lGetDouble(job_node, STN_shr) * total_share_tree_tickets);
               if (hierarchy[policy_ndx].dependent)
                  jref->tickets += REF_GET_STICKET(jref);
            }
            lFreeList(sorted_job_node_list);
         }
      }

            break;

         case FUNCTIONAL_POLICY:

      /*-----------------------------------------------------------------
       * Calculate pending functional tickets 
       *        
       *    We use a somewhat brute force method where we order the pending
       *    jobs based on which job would get the largest number of 
       *    functional tickets if it were to run next. Because of the
       *    overhead, we limit the number of pending jobs that we will
       *    order based on the configurable schedd parameter called
       *    max_functional_jobs_to_schedule. The default is 100.
       *             
       *    The algorithm works like:
       *    - order all pending jobs by functional categories. A functional
       *      category consits of: user_shares, dept_shares, project_shares,
       *      and job_shares.
       *    - For all max_functional_jobs_to_schedule compute the functional
       *      ticket for the first job of each fcategory. Our currently used
       *      algorithm computes for all jobs under the same fcategory the
       *      same functional tickets. Therefore we have only to compute
       *      the first job of each fcategory. The job list under each category
       *      has to be sorted by time and previous computed tickets.
       *      
       *    - When the job of one functional category is found, increase the
       *      counter for running jobs in the user, dept, proj, and other structures.
       *      Afterwards, compute the real functional tickets.
       *
       *      When this is done, undo all the changes to the different structures 
       *      (running job counters) and free the fcategory data-structure.
       *-----------------------------------------------------------------*/
      DPRINTF(("calc funktional tickets %.3f\n", total_functional_tickets));
      if (total_functional_tickets > 0 && num_queued_jobs > 0) {
         sge_ref_t **sort_list=NULL;
         lList *fcategories = NULL;

         int i=0;
         
         double weight[k_last];


         double pending_user_fshares = sum_of_user_functional_shares;
         double pending_proj_fshares = sum_of_project_functional_shares;
         double pending_dept_fshares = sum_of_department_functional_shares;
         double pending_jobclass_fshares = sum_of_jobclass_functional_shares;
         double pending_job_fshares = sum_of_job_functional_shares;
         int max =  MIN(num_queued_jobs, sconf_get_max_functional_jobs_to_schedule());

         sort_list = malloc(max * sizeof(*sort_list)); 
         build_functional_categories(job_ref, num_jobs, &fcategories, hierarchy[policy_ndx].dependent);
         
         if(!sort_list || !fcategories){
            /* error message to come */
            DEXIT;
            return sge_scheduling_run;
         }
        
         get_functional_weighting_parameters(1, 1, 1, 1, 1, weight);

         /* Loop through all the jobs calculating the functional tickets and
            find the job with the most functional tickets.  Move it to the
            top of the list� and start the process all over again with the
            remaining jobs. */

         for(i=0; i<max; i++) {
            double ftickets, max_ftickets=-1;
            u_long jid, save_jid=0, save_tid=0;
            lListElem *current = NULL;
            lListElem *max_current = NULL;

            /* loop over all first jobs from the different categories */
            for_each(current, fcategories){ 
               sge_ref_list_t *tmp = (lGetRef(current, FCAT_jobrelated_ticket_first)); 
               sge_ref_t *jref = tmp->ref;
               double user_fshares = pending_user_fshares + 0.001;
               double proj_fshares = pending_proj_fshares + 0.001;
               double dept_fshares = pending_dept_fshares + 0.001;
               double jobclass_fshares = pending_jobclass_fshares + 0.001;
               double job_fshares = pending_job_fshares + 0.001;

               /* calc the tickets */

               calc_intern_pending_job_functional_tickets(current,
                                                   user_fshares,
                                                   proj_fshares,
                                                   dept_fshares,
                                                   jobclass_fshares,
                                                   job_fshares,
                                                   total_functional_tickets,
                                                   weight,
                                                   share_functional_shares);

               ftickets = REF_GET_FTICKET(jref);

               if (hierarchy[policy_ndx].dependent)
                  ftickets += jref->tickets;

               /* controll, if the current job has the most tickets */
               if(ftickets >= max_ftickets){
                  if (max_current == NULL ||
                        ftickets > max_ftickets ||
                        (jid=lGetUlong(jref->job, JB_job_number)) < save_jid ||
                        (jid == save_jid && 
                        REF_GET_JA_TASK_NUMBER(jref) < save_tid)) {
                     max_ftickets = ftickets;
                     save_jid = lGetUlong(jref->job, JB_job_number);
                     save_tid = REF_GET_JA_TASK_NUMBER(jref);
                     sort_list[i] = jref;
                     max_current = current;
                  }
               }
            }

            /* - set the results from first entry of the winning category to the second 
               - remove the winning element
               - if it was the last element, remove the fcategory*/
            {
               sge_ref_list_t *tmp =lGetRef(max_current, FCAT_jobrelated_ticket_first);  
               copy_ftickets (tmp, tmp->next);
               lSetRef(max_current, FCAT_jobrelated_ticket_first, tmp->next);  
               free(tmp);
               if(lGetRef(max_current, FCAT_jobrelated_ticket_first) == NULL){
                  lSetRef(max_current, FCAT_jobrelated_ticket_last,  NULL);
                  lSetRef(current, FCAT_user, NULL);
                  lSetRef(current, FCAT_dept, NULL);
                  lSetRef(current, FCAT_project, NULL);

                  lRemoveElem(fcategories, max_current);
               }
            }

            /* This is the job with the most functional tickets, consider it active */
            sge_set_job_cnts(sort_list[i], 0);

            /* recompute the results afterwards for the winnnig entry */
            calc_pending_job_functional_tickets(sort_list[i],
                                                &pending_user_fshares,
                                                &pending_proj_fshares,
                                                &pending_dept_fshares,
                                                &pending_jobclass_fshares,
                                                &pending_job_fshares,
                                                total_functional_tickets,
                                                weight,
                                                share_functional_shares);
         }

         /* Reset the pending jobs to inactive */
         {
            int depend = hierarchy[policy_ndx].dependent;
            for(job_ndx=0; job_ndx < max; job_ndx++){
               sge_ref_t *jref = sort_list[job_ndx];
               sge_unset_job_cnts(jref, 0);  

               /* sum up the tickets, if there are dependencies */             
               if(depend)
                  jref->tickets += REF_GET_FTICKET(jref);
            }

            destribute_ftickets(fcategories, depend);
         }
         /* free the allocated memory */
         free(sort_list);
         free_fcategories(&fcategories);
      }

            break;

         case OVERRIDE_POLICY:

      /*-----------------------------------------------------------------
       * Calculate the pending override tickets
       *-----------------------------------------------------------------*/
      {
         bool share_override_tickets = sconf_get_share_override_tickets();
      for(job_ndx=0; job_ndx<num_jobs; job_ndx++) {
         sge_ref_t *jref = &job_ref[job_ndx];
         if (jref->queued) {
            calc_job_override_tickets(jref, share_override_tickets);
            if (hierarchy[policy_ndx].dependent)
               jref->tickets += REF_GET_OTICKET(jref);
         }
      }
      }
            break;

         default:
            break;
         }
      }

      /*-----------------------------------------------------------------
       * Combine the ticket values into the total tickets for the job
       *-----------------------------------------------------------------*/

      for(job_ndx=0; job_ndx<num_jobs; job_ndx++)
         calc_job_tickets(&job_ref[job_ndx]);

      /* calculate the ticket % of each job */

      sum_of_active_tickets = 0;
      sum_of_pending_tickets = 0;
      for(job_ndx=0; job_ndx<num_jobs; job_ndx++) {
         sge_ref_t *jref = &job_ref[job_ndx];
         if (jref->queued)
            sum_of_pending_tickets += REF_GET_TICKET(jref);
         else
            sum_of_active_tickets += REF_GET_TICKET(jref);
      }

      for(job_ndx=0; job_ndx<num_jobs; job_ndx++) {
         sge_ref_t *jref = &job_ref[job_ndx];
         double ticket_sum = jref->queued ? sum_of_pending_tickets : sum_of_active_tickets;

         if (REF_GET_TICKET(jref) > 0) {
            REF_SET_SHARE(jref, REF_GET_TICKET(jref) / ticket_sum);
         } else {
            REF_SET_SHARE(jref, 0);
         }
      }

   } else {
      for(job_ndx=0; job_ndx<num_jobs; job_ndx++) {
         sge_ref_t *jref = &job_ref[job_ndx];
         lListElem *ja_task;

         job = jref->job;
         ja_task = jref->ja_task;

         if (REF_GET_TICKET(jref)  > 0) {
            REF_SET_SHARE(jref, REF_GET_TICKET(jref) / sum_of_active_tickets);
         } else {
            REF_SET_SHARE(jref, 0);
         }

      }

   }

   /* update share tree node for finished jobs */

   /* NOTE: what purpose does this serve? */

   if (do_usage && finished_jobs) {
      for_each(job, finished_jobs) {
         sge_ref_t jref;
         lListElem *ja_task;

         for_each(ja_task, lGetList(job, JB_ja_tasks)) {
            memset(&jref, 0, sizeof(jref));
            sge_set_job_refs(job, ja_task, &jref, NULL, lists, 0);
            if (jref.node) {
               if (sge_scheduling_run != lGetUlong(jref.node, STN_pass2_seqno)) {
                  sge_zero_node_fields(jref.node, NULL);
                  lSetUlong(jref.node, STN_pass2_seqno, sge_scheduling_run);
               }
            }
         }
      }
   }

   /* 
    * copy tickets 
    * 
    * Tickets for unenrolled pending tasks were stored an internal table.
    * Now it is necessary to find the ja_task of a job which got the 
    * most tickets. These ticket numbers will be stored in the template
    * element within the job. 
    */
   if (queued_jobs != NULL) {
      sge_task_ref_t *tref = task_ref_get_first_job_entry();

      while(tref != NULL) {
         lListElem *job = job_list_locate(queued_jobs, tref->job_number); 

         if (job) {
            lListElem *ja_task_template;

            ja_task_template = lFirst(lGetList(job, JB_ja_template));

            task_ref_copy_to_ja_task(tref, ja_task_template);
         } 
         tref = task_ref_get_next_job_entry();
      }
   }
   
   if (job_ref) {
      for(job_ndx=0; job_ndx<num_jobs; job_ndx++)
         if (job_ref[job_ndx].task_jobclass)
            free(job_ref[job_ndx].task_jobclass);
      free(job_ref);
   }

   if (decay_list)
      lFreeList(decay_list);

   PROF_STOP_MEASUREMENT(SGE_PROF_SCHEDLIB4);
   prof_calc = prof_get_measurement_utime(SGE_PROF_SCHEDLIB4, false, NULL);
      
   if(prof_is_active()){
      u_long32 saved_logginglevel = log_state_get_log_level();
      log_state_set_log_level(LOG_INFO); 
      INFO((SGE_EVENT, "PROF: SGEEE job ticket calculation: init: %.3f s, pass 0: %.3f s, pass 1: %.3f, pass2: %.3f, calc: %.3f s\n",
               prof_init, prof_pass0, prof_pass1, prof_pass2, prof_calc));
      log_state_set_log_level(saved_logginglevel);
   }

   DEXIT;
   return sge_scheduling_run;
}


/*--------------------------------------------------------------------
 * sge_sort_pending_job_nodes - return a sorted list of pending job
 * nodes with the pending priority set in STN_sort.
 *--------------------------------------------------------------------*/

static lList *
sge_sort_pending_job_nodes(lListElem *root,
                           lListElem *node,
                           double total_share_tree_tickets)
{
   lList *job_node_list = NULL;
   lListElem *child, *job_node;
   double node_stt;
   double job_count=0;
   int job_nodes = 0;
   
   if(root == node){
      int active_nodes = 0;
      lListElem *temp_root = NULL;

      for_each(child, lGetList(node, STN_children)) {
         if (lGetUlong(child, STN_ref)) {
            active_nodes++;
            break;
         } else if ((lGetUlong(child, STN_job_ref_count)-lGetUlong(child, STN_active_job_ref_count))>0) {
            temp_root = child;
            active_nodes ++; 
         }
         if (active_nodes >1)
               break;
      }
      if (active_nodes == 1 && temp_root)
         return sge_sort_pending_job_nodes(temp_root, temp_root, total_share_tree_tickets);
   }
   /* get the child job nodes in a single list */

   for_each(child, lGetList(node, STN_children)) {
      if (lGetUlong(child, STN_ref)) {
         /* this is a job node, chain it onto our list */
         if (!job_node_list)
            job_node_list = lCreateList("sorted job node list", STN_Type);
         lAppendElem(job_node_list, lCopyElem(child));
         job_nodes++;
      } else if ((lGetUlong(child, STN_job_ref_count)-lGetUlong(child, STN_active_job_ref_count))>0) {
         lList *child_job_node_list;
         /* recursively get all the child job nodes onto our list */
         if ((child_job_node_list = sge_sort_pending_job_nodes(root, child, total_share_tree_tickets))) {
            if (job_node_list == NULL)
               job_node_list = child_job_node_list;
            else
               lAddList(job_node_list, child_job_node_list);
         }
      }
   }
   /* free the temporary job nodes */
   if (job_nodes)
      lSetList(node, STN_children, NULL);

   /* sort the job nodes based on the calculated pending priority */
   if (root != node || job_nodes) { 
      lListElem *u;

      if (job_node_list && lGetNumberOfElem(job_node_list)>1)
         lPSortList(job_node_list, "%I- %I+ %I+", STN_sort, STN_jobid, STN_taskid);

      /* calculate a new pending priority -
         The priority of each pending job associated with this node is the
         node's short term entitlement (STN_stt) divided by the number of jobs
         which are scheduled ahead of this node times the number of share tree
         tickets. If we are dependent on another higher-level policy, we also add
         the tickets from those policies. */

      job_count = lGetUlong(node, STN_active_job_ref_count);
      if ((u=lGetElemStr(lGetList(node, STN_usage_list), UA_name,
                         "finished_jobs")))
         job_count += lGetDouble(u, UA_value);
      node_stt = lGetDouble(node, STN_stt);
      for_each(job_node, job_node_list) {
         job_count++;
         lSetDouble(job_node, STN_shr, node_stt / job_count);
         lSetDouble(job_node, STN_sort, lGetDouble(job_node, STN_tickets) +
                    ((node_stt / job_count) * total_share_tree_tickets));
      }

   }

   /* return the new list */

   return job_node_list;
}


/*--------------------------------------------------------------------
 * sge_calc_sharetree_targets - calculate the targeted 
 * proportional share for each node in the sharetree.
 *--------------------------------------------------------------------*/

static int
sge_calc_sharetree_targets( lListElem *root,
                            sge_Sdescr_t *lists,
                            lList *decay_list,
                            u_long curr_time,
                            u_long seqno )
{

   DENTER(TOP_LAYER, "sge_calc_sharetree_targets");

   /* decay and store user and project usage into sharetree nodes */

   sge_calc_node_usage(root,
                       lists->user_list,
                       lists->project_list,
                       decay_list,
                       curr_time,
                       NULL,
                       seqno);

   /* Calculate targeted proportions for each node */

   sge_calc_node_targets(root, root, lists);

   DEXIT;
   return 0;
}


/*--------------------------------------------------------------------
 * sge_calc_node_targets - calculate the targeted proportions
 * for the sub-tree rooted at this node
 *--------------------------------------------------------------------*/

static int
sge_calc_node_targets( lListElem *root,
                       lListElem *node,
                       sge_Sdescr_t *lists )
{
   lList *children;
   lListElem *child;
   double sum_shares, sum, compensation_factor;
   /* char *node_name = lGetString(node, STN_name); */

   DENTER(TOP_LAYER, "sge_calc_node_targets");

   /*---------------------------------------------------------------
    * if this is the root node, initialize proportions
    *    ltt - long term targeted % among siblings
    *    oltt - overall long term targeted % among entire share tree
    *    stt - short term targeted % among siblings
    *    ostt - overall short term targeted % among entire share tree
    *---------------------------------------------------------------*/

   if (node == root) {
      lSetDouble(node, STN_stt, 1.0);
      lSetDouble(node, STN_ltt, 1.0);
      lSetDouble(node, STN_ostt, 1.0);
      lSetDouble(node, STN_oltt, 1.0);
   }

   /*---------------------------------------------------------------
    * if this node has no children, there's nothing to do
    *---------------------------------------------------------------*/

   if ((!(children = lGetList(node, STN_children))) ||
       lGetNumberOfElem(children) == 0) {
      DEXIT;
      return 0;
   }

/*
   do for each active sub-node
      sum_shares += subnode.shares
      subnode.shr = subnode.shares * subnode.shares / subnode.usage
      sum += subnode.shr 
   end do
*/

   /*---------------------------------------------------------------
    * calculate the shr value of each child node
    * calculate the short term targeted proportions 
    *---------------------------------------------------------------*/

   sum_shares = 0;
   for_each(child, children) {
      if (lGetUlong(child, STN_job_ref_count)>0) {
         sum_shares += lGetUlong(child, STN_shares);
      }
   }

   sum = 0;
   for_each(child, children) {
      if (lGetUlong(child, STN_job_ref_count)>0) {
         double shares, shr, ltt, oltt;
         shares = lGetUlong(child, STN_shares);
         if (sum_shares > 0) {
            ltt = shares / (double)sum_shares;
         } else {
            ltt = 0;
         }
         oltt = lGetDouble(node, STN_oltt) * ltt;
         lSetDouble(child, STN_ltt, ltt);
         lSetDouble(child, STN_oltt, oltt);
         if (oltt > 0) {
            shr = shares * shares / 
                MAX(lGetDouble(child, STN_combined_usage), SGE_MIN_USAGE*oltt);
         } else {
            shr = 0;
         }
         lSetDouble(child, STN_shr, shr);
         sum += shr;
      }
   }

/*
   do for each active sub-node
      subnode.ltt = subnode.shares / sum_shares;
      subnode.oltt = node.oltt * subnode.ltt;
      subnode.stt = subnode.shr / sum;
      subnode.ostt = node.stt * subnode.stt;
   end do
*/

   /*---------------------------------------------------------------
    * calculate the short term targeted proportions 
    *---------------------------------------------------------------*/

   for_each(child, children) {
      if (lGetUlong(child, STN_job_ref_count)>0) {
         double stt;
  
         if (sum > 0) { 
            stt = lGetDouble(child, STN_shr) / sum;
         } else {
            stt = 0;
         }

         lSetDouble(child, STN_stt, stt);
         lSetDouble(child, STN_ostt, lGetDouble(node, STN_ostt) * stt);
         lSetDouble(child, STN_adjusted_current_proportion, lGetDouble(child, STN_ostt));
      }
   }

/* psydo code:

   if compensation_factor != 0

      sum = 0
      do for each active sub-node
         if subnode.ostt > (compensation_factor * subnode.oltt)
            subnode.shr = subnode.shares * subnode.shares / 
                  (subnode.usage * (subnode.ostt / (compensation_factor * subnode.oltt)))
            sum += subnode.shr
         endif
      enddo

      do for each active sub-node
         subnode.stt = subnode.shr / sum;
         subnode.ostt = node.stt * subnode.stt;
      end do

   endif
*/

   /*---------------------------------------------------------------
    * adjust targeted proportion based on compensation factor
    *---------------------------------------------------------------*/

   compensation_factor = sconf_get_compensation_factor();

   if (compensation_factor != 0) {
      u_long compensation = 0;

      /*---------------------------------------------------------------
       * recalculate the shr value of each child node based on compensation factor
       *---------------------------------------------------------------*/

      sum = 0;
      for_each(child, children) {
         if (lGetUlong(child, STN_job_ref_count)>0) {
            double ostt = lGetDouble(child, STN_ostt);
            double oltt = lGetDouble(child, STN_oltt);
            if (ostt > (compensation_factor * oltt)) {
               double shares = lGetUlong(child, STN_shares);
               double shr = shares * shares /
                     (MAX(lGetDouble(child, STN_combined_usage), SGE_MIN_USAGE*oltt) *
                     (ostt / (compensation_factor * oltt)));
               lSetDouble(child, STN_shr, shr);
               compensation = 1;
            }
            sum += lGetDouble(child, STN_shr);
         }
      }

      /*---------------------------------------------------------------
       * reset short term targeted proportions (if necessary)
       *---------------------------------------------------------------*/

      if (compensation) {
         for_each(child, children) {
            if (lGetUlong(child, STN_job_ref_count)>0) {
               double stt = lGetDouble(child, STN_shr) / sum;
               lSetDouble(child, STN_stt, stt);
               lSetDouble(child, STN_ostt, lGetDouble(node, STN_ostt) * stt);
               lSetDouble(child, STN_adjusted_current_proportion, lGetDouble(child, STN_ostt));
            }
         }
      }

   }

/*
   do for each active sub-node
      call sge_calc_node_targets(subnode, lists);
   end do
*/

   /*---------------------------------------------------------------
    * recursively call self for all active child nodes
    *---------------------------------------------------------------*/

   for_each(child, children) {
      if (lGetUlong(child, STN_job_ref_count)>0) {
         sge_calc_node_targets(root, child, lists);
      }
   }

   DEXIT;
   return 0;
}

/*--------------------------------------------------------------------
 * get_mod_share_tree - return reduced modified share tree
 *--------------------------------------------------------------------*/

static lListElem *
get_mod_share_tree( lListElem *node,
                    lEnumeration *what,
                    int seqno )
{
   lListElem *new_node=NULL;
   lList *children;
   lListElem *child;

   if (((children = lGetList(node, STN_children))) &&
       lGetNumberOfElem(children) > 0) {

      lList *child_list=NULL;

      for_each(child, children) {
         lListElem *child_node;
         child_node = get_mod_share_tree(child, what, seqno);
         if (child_node) {
            if (!child_list)
               child_list = lCreateList("", STN_Type);
            lAppendElem(child_list, child_node);
         }
      }

      if (child_list) {
#ifdef lmodifywhat_shortcut
         new_node = lCreateElem(STN_Type);
         lModifyWhat(new_node, node, what);
#else
         new_node = lCopyElem(node);
#endif
         lSetList(new_node, STN_children, child_list);
      }
      
   } else {

      if (lGetUlong(node, STN_pass2_seqno) > (u_long32)seqno &&
          lGetUlong(node, STN_temp) == 0) {
#ifdef lmodifywhat_shortcut
         new_node = lCreateElem(STN_Type);
         lModifyWhat(new_node, node, what);
#else
         new_node = lCopyElem(node);
#endif
      }

   }

   return new_node;
}

/****** sgeee/sge_build_sgeee_orders() *******************************************
*  NAME
*     sge_build_sgeee_orders() -- build orders for updating qmaster

*
*  SYNOPSIS
*     static lList * sge_build_sgeee_orders(sge_Sdescr_t *lists, lList *running_jobs, 
*     lList *queued_jobs, lList *finished_jobs, lList *order_list, int 
*     update_usage_and_configuration, int seqno) 
*
*  FUNCTION
*     Builds generates the orderlist for sending the scheduling decisions
*     to the qmaster. The following orders are generated:
*     - running job tickets
*     - pending job tickets
*     - delete order for finished jobs
*     - update user usage order
*     - update project usage order 
*     - update share tree order
*     - update scheduler configuration order
*     -  orders updating user/project resource usage (ORT_update_project_usage)
*     -  orders updating running tickets needed for dynamic repriorization (ORT_ticket)
*     Most orders are generated by using the sge_create_orders function.
*
*  INPUTS
*     sge_Sdescr_t *lists                - ??? 
*     lList *running_jobs                - list of running jobs 
*     lList *queued_jobs                 - list of queued jobs (should be sorted by ticktes)
*     lList *finished_jobs               - list of finished jobs
*     lList *order_list                  - existing order list (new orders will be added to it
*     bool update_usage_and_configuration - if true, the update usage orders are generated
*     int seqno                          - a seqno, changed with each scheduling run
*     bool max_queued_ticket_orders      - if true, pending tickets are submited to the qmaster 
*     bool updated_execd                 - if true, the queue information is send with the running job tickets
*
*  RESULT
*     lList * -  new order list
*
*******************************************************************************/
static lList *sge_build_sgeee_orders( sge_Sdescr_t *lists,
                      lList *running_jobs,
                      lList *queued_jobs,
                      lList *finished_jobs,
                      lList *order_list,
                      bool update_usage_and_configuration,
                      int seqno,
                      bool max_queued_ticket_orders,
                      bool update_execd)
{
   static lEnumeration *usage_what = NULL;
   static lEnumeration *share_tree_what = NULL;
   static lEnumeration *config_what = NULL;
   lCondition *where=NULL;
   lList *up_list;
   lListElem *order;
   lListElem *root;
   lListElem *job;
   int norders;
   u_long32 max_pending_tasks_per_job = sconf_get_max_pending_tasks_per_job();
   static int last_seqno = 0;

   static bool last_max_queued_ticket_orders=true; /* allows me to identify changes in the configuration */

   double prof_job_orders=0, prof_update_orders=0;

   DENTER(TOP_LAYER, "sge_build_sgeee_orders");
   PROF_STOP_MEASUREMENT(SGE_PROF_SCHEDLIB4);

   if (!config_what)
      config_what = lWhat("%T(%I )", SC_Type,
                   SC_weight_tickets_override);
                   
   if (!share_tree_what)
      share_tree_what = lWhat("%T(%I %I %I %I %I %I)", STN_Type,
                         STN_version, STN_name, STN_job_ref_count, STN_m_share,
                         STN_last_actual_proportion,
                         STN_adjusted_current_proportion);

   if (!usage_what)
      usage_what = lWhat("%T(%I %I %I %I %I %I %I)", UP_Type,
                   UP_name, UP_usage, UP_usage_time_stamp,
                   UP_long_term_usage, UP_project, UP_debited_job_usage,
                   UP_version);


   if (!order_list)
      order_list = lCreateList("orderlist", OR_Type);

   /*-----------------------------------------------------------------
    * build ticket orders for running jobs
    *-----------------------------------------------------------------*/

   if (running_jobs) {
      norders = lGetNumberOfElem(order_list);

      DPRINTF(("   got %d running jobs\n", lGetNumberOfElem(running_jobs)));

      for_each(job, running_jobs) {
         const char *pe_str;
         lList *granted;
         lListElem *pe;
         lListElem *ja_task;

         for_each(ja_task, lGetList(job, JB_ja_tasks)) {
            if ((pe_str=lGetString(ja_task, JAT_granted_pe))
                  && (pe=pe_list_locate(all_lists->pe_list, pe_str))
                  && lGetBool(pe, PE_control_slaves))

               granted=lGetList(ja_task, JAT_granted_destin_identifier_list);
            else
               granted=NULL;

            order_list = sge_create_orders(order_list, ORT_tickets, job, ja_task, granted, false, update_execd);
         }
      }
      DPRINTF(("   added %d ticket orders for running jobs\n", 
         lGetNumberOfElem(order_list) - norders));
   }


/*
 * check for the amount of pending ticket orders should be checkted.
 * If the report_pjob_ticktes has changed to false, we need to send empty ptickets orders to the qmaster
 * to remove the pticket values. This prevents qstat from reporting wrong pticket values. Its only done
 * once after the config change.
 */
   if (queued_jobs && (max_queued_ticket_orders || (max_queued_ticket_orders != last_max_queued_ticket_orders))) {
      lListElem *qep;
      u_long32 free_qslots = 0;
      last_max_queued_ticket_orders = max_queued_ticket_orders;
      norders = lGetNumberOfElem(order_list);
      for_each(qep, lists->queue_list)
         free_qslots += MAX(0, lGetUlong(qep, QU_job_slots) - qinstance_slots_used(qep));
      for_each(job, queued_jobs) {
         lListElem *ja_task;
         int tasks=0;
   
         /* NOTE: only send pending tickets for the first sub-task */
         /* when the first sub-task gets scheduled, then the other sub-tasks didn't have
            any tickets specified */
         for_each(ja_task, lGetList(job, JB_ja_tasks)) {
            if (++tasks > MIN(max_pending_tasks_per_job, free_qslots+1))
               break;
            order_list = sge_create_orders(order_list, ORT_ptickets, job, ja_task, NULL, !max_queued_ticket_orders, false);
         }
         if (job_get_not_enrolled_ja_tasks(job) > 0) {
            lListElem *task_template = NULL;

            task_template = lFirst(lGetList(job, JB_ja_template));
            if (task_template) {
               order_list = sge_create_orders(order_list, ORT_ptickets, job, task_template, NULL, !max_queued_ticket_orders, false);
            }
         }  
      }
      DPRINTF(("   added %d ticket orders for queued jobs\n", 
         lGetNumberOfElem(order_list) - norders));

   }

   /*-----------------------------------------------------------------
    * build delete job orders for finished jobs
    *-----------------------------------------------------------------*/
   if (finished_jobs) {
      order_list  = create_delete_job_orders(finished_jobs, order_list);
   }

   PROF_STOP_MEASUREMENT(SGE_PROF_SCHEDLIB4);
   prof_job_orders = prof_get_measurement_utime(SGE_PROF_SCHEDLIB4,false, NULL);
   PROF_START_MEASUREMENT(SGE_PROF_SCHEDLIB4); 

   if (update_usage_and_configuration) {

      /*-----------------------------------------------------------------
       * build update user usage order
       *-----------------------------------------------------------------*/

      /* NOTE: make sure we get all usage entries which have been decayed
         or have accumulated additional usage */

      where = lWhere("%T(%I > %u)", UP_Type, UP_usage_seqno, last_seqno);

      if (lists->user_list) {
         norders = lGetNumberOfElem(order_list); 
         if ((up_list = lSelect("", lists->user_list, where, usage_what))) {
            if (lGetNumberOfElem(up_list)>0) {
               order = lCreateElem(OR_Type);
               lSetUlong(order, OR_seq_no, get_seq_nr());
               lSetUlong(order, OR_type, ORT_update_user_usage);
               lSetList(order, OR_joker, up_list);
               lAppendElem(order_list, order);
            } else
               lFreeList(up_list);
         }
         DPRINTF(("   added %d orders for updating usage of user\n",
            lGetNumberOfElem(order_list) - norders));      
      }

      /*-----------------------------------------------------------------
       * build update project usage order
       *-----------------------------------------------------------------*/

      if (lists->project_list) {
         norders = lGetNumberOfElem(order_list); 
         if ((up_list = lSelect("", lists->project_list, where, usage_what))) {
            if (lGetNumberOfElem(up_list)>0) {
               order = lCreateElem(OR_Type);
               lSetUlong(order, OR_seq_no, get_seq_nr());
               lSetUlong(order, OR_type, ORT_update_project_usage);
               lSetList(order, OR_joker, up_list);
               lAppendElem(order_list, order);
            } else
               lFreeList(up_list);
         }
         DPRINTF(("   added %d orders for updating usage of project\n",
            lGetNumberOfElem(order_list) - norders));
      }

      lFreeWhere(where);

      /*-----------------------------------------------------------------
       * build update share tree order
       *-----------------------------------------------------------------*/

      if (lists->share_tree && ((root = lFirst(lists->share_tree)))) {
         lListElem *node;
         norders = lGetNumberOfElem(order_list);

         if ((node = get_mod_share_tree(root, share_tree_what, last_seqno))) {
            up_list = lCreateList("", STN_Type);
            lAppendElem(up_list, node);
            order = lCreateElem(OR_Type);
            lSetUlong(order, OR_seq_no, get_seq_nr());
            lSetUlong(order, OR_type, ORT_share_tree);
            lSetList(order, OR_joker, up_list);
            lAppendElem(order_list, order);
         }
         DPRINTF(("   added %d orders for updating share tree\n",
            lGetNumberOfElem(order_list) - norders)); 
      } 

      /*-----------------------------------------------------------------
       * build update scheduler configuration order
       *-----------------------------------------------------------------*/

      if (sconf_is()) {
         lList *config_list;
         norders = lGetNumberOfElem(order_list); 
         if ((config_list = lSelect("", *sconf_get_config_list(), NULL, config_what))) {
            if (lGetNumberOfElem(config_list)>0) {
               order = lCreateElem(OR_Type);
               lSetUlong(order, OR_seq_no, get_seq_nr());
               lSetUlong(order, OR_type, ORT_sched_conf);
               lSetList(order, OR_joker, config_list);
               lAppendElem(order_list, order);
            } else
               lFreeList(config_list);
         }
         DPRINTF(("   added %d orders for scheduler configuration\n",
            lGetNumberOfElem(order_list) - norders));   
      }
      last_seqno = seqno;
   }

   PROF_STOP_MEASUREMENT(SGE_PROF_SCHEDLIB4);
   prof_update_orders = prof_get_measurement_utime(SGE_PROF_SCHEDLIB4,false, NULL);
   if (prof_is_active()) {
      u_long32 saved_logginglevel = log_state_get_log_level();
      log_state_set_log_level(LOG_INFO); 

      INFO((SGE_EVENT, "PROF: SGEEE update orders: job orders: %.3f s, update orders: %.3f s\n",
               prof_job_orders, prof_update_orders));

      log_state_set_log_level(saved_logginglevel);
   }

   DEXIT;
   return order_list;
}

/*--------------------------------------------------------------------
 * sge_scheduler
 *--------------------------------------------------------------------*/


/****** sgeee/sgeee_scheduler() ************************************************
*  NAME
*     sgeee_scheduler() -- calc tickets, send orders, and sort job list 
*
*  SYNOPSIS
*     int sgeee_scheduler(sge_Sdescr_t *lists, lList *running_jobs, lList 
*     *finished_jobs, lList *pending_jobs, lList **orderlist) 
*
*  FUNCTION
*     - calculates the running and pending job tickets. 
*     - send the orders to the qmaster about the job tickets
*     - order the pending job list according the the job tickets
*
*    On a "normal" scheduling interval:
*	   - calculate tickets for new and running jobs
*	   - don't decay and sum usage
*	   - don't update qmaster
*
*    On a SGEEE scheduling interval:
*	   - calculate tickets for new and running jobs
*	   - decay and sum usage
*	   - handle finished jobs
*	   - update qmaster
*
*  INPUTS
*     sge_Sdescr_t *lists  - a ref to all lists in this scheduler 
*     lList *running_jobs  - a list of all running jobs 
*     lList *finished_jobs -  a list of all finished jobs
*     lList *pending_jobs  -  a list of all pending jobs
*     lList **orderlist    -  the order list
*
*  RESULT
*     int - 0 if everthing went fine, -1 if not
*
*******************************************************************************/
int sgeee_scheduler( sge_Sdescr_t *lists,
               lList *running_jobs,
               lList *finished_jobs,
               lList *pending_jobs,
               lList **orderlist,
               bool has_queues,
               bool has_pending_jobs )
{
   static u_long32 past = 0;
   u_long32 now = sge_get_gmt();
   u_long seqno;
   lListElem *job;
   double min_tix  = DBL_MAX;
   double max_tix  = DBL_MIN;

   DENTER(TOP_LAYER, "sgeee_scheduler");

   /* clear SGEEE fields for queued jobs */
   for_each(job, pending_jobs)
      sge_clear_job(job);

   /* calculate per job static urgency values */
   sge_do_urgency(now, pending_jobs, running_jobs, lists);

   /* calculate tickets for pending jobs */
   seqno = sge_calc_tickets(lists, running_jobs, finished_jobs, 
                              pending_jobs, 1);
         
   /* calculate tickets for running jobs */
   seqno = sge_calc_tickets(lists, running_jobs, NULL, NULL, 0);

   /* determine min/max tix */
   get_max_ptix(&min_tix, &max_tix, running_jobs);
   get_max_ptix(&min_tix, &max_tix, pending_jobs);
   {
      int i;
      for (i = 0; i < task_ref_entries; i++) {
         sge_task_ref_t *tref = task_ref_get_entry(i);
         if (tref == NULL) {
            break;
         }
         max_tix = MAX(max_tix, tref->ja_task_ticket);
         min_tix = MIN(min_tix, tref->ja_task_ticket);
      }
   }


   /* use min/max tix for normalizing 
      - now to determine initial normalized ticket amount
      - but also later on after tickets recomputation when a job was assigned */
   tix_range_set(min_tix, max_tix);

#ifdef DEBUG_TASK_REF
   task_ref_print_table();
#endif

   /*
    * now combine ticket amount and static urgency scheme into 
    * absolute priority
    */
   DPRINTF(("Normalizing tickets using %f/%f as min_tix/max_tix\n", min_tix, max_tix));
   sge_do_sgeee_priority(running_jobs, min_tix, max_tix); 
   sge_do_sgeee_priority(pending_jobs, min_tix, max_tix); 

   /* 
    * Order Jobs in descending order according to tickets and 
    * then job number 
    */
   PROF_START_MEASUREMENT(SGE_PROF_CUSTOM3);

   sgeee_sort_jobs(&pending_jobs);

   PROF_STOP_MEASUREMENT(SGE_PROF_CUSTOM3);

   if (do_profiling) {
      u_long32 saved_logginglevel = log_state_get_log_level();

      log_state_set_log_level(LOG_INFO);
      INFO((SGE_EVENT, "PROF: SGEEE job sorting took %.3f s\n",
            prof_get_measurement_wallclock(SGE_PROF_CUSTOM3, false, NULL)));
      log_state_set_log_level(saved_logginglevel);
   }

   /* somebody might have played with the system clock. */
   if (now < past)
      past = now;

   {
      u_long32 reprioritize_interval = sconf_get_reprioritize_interval(); 
      bool update_execd = ( reprioritize_interval == 0 || (now >= (past + reprioritize_interval))); 
      if (update_execd)
         past = now;
      *orderlist = sge_build_sgeee_orders(lists, running_jobs, pending_jobs, finished_jobs,
                                        *orderlist, true, seqno, sconf_get_report_pjob_tickets(), update_execd);
   }
   if(!has_pending_jobs || !has_queues)
      return 0;

#if 0
   /* 
    * calculate the number of tickets for all jobs already 
    * running on the host 
    */
   PROF_START_MEASUREMENT(SGE_PROF_CUSTOM2);

   if (calculate_host_tickets(&running_jobs, 
                              &(lists->host_list)))  {
      DPRINTF(("no host for which to calculate host tickets\n"));
      DEXIT;
      return -1;
   }

   PROF_STOP_MEASUREMENT(SGE_PROF_CUSTOM2);
#endif

   if (prof_is_active()) {
       u_long32 saved_logginglevel = log_state_get_log_level();

       log_state_set_log_level(LOG_INFO);
       INFO((SGE_EVENT, "PROF: SGEEE active host job ticket calculation took %.3f s\n",
             prof_get_measurement_wallclock(SGE_PROF_CUSTOM2, false, NULL)));
       log_state_set_log_level(saved_logginglevel);
    }



   DEXIT;
   return 0;
}

/****** sgeee/sge_do_sgeee_priority() ******************************************
*  NAME
*     sge_do_sgeee_priority() -- determine GEEE priority for a list of jobs
*
*  SYNOPSIS
*     static void sge_do_sgeee_priority(lList *job_list, double min_tix, double 
*     max_tix) 
*
*  FUNCTION
*     Determines for a list of jobs the GEEE priority. Prior
*     sge_do_sgeee_priority() can be called the normalized urgency value must 
*     already be known for each job. The ticket range passed is used for 
*     normalizing ticket amount.
*
*  INPUTS
*     lList *job_list - The job list
*     double min_tix  - Minumum ticket amount
*     double max_tix  - Maximum ticket amount
*
*  NOTES
*     MT-NOTE: sge_do_sgeee_priority() is MT safe
*******************************************************************************/
static void sge_do_sgeee_priority(lList *job_list, double min_tix, double max_tix) 
{
   lListElem *job, *task;
   u_long32 jobid;
   bool enrolled;
   double nsu;

   for_each (job, job_list) {
      jobid = lGetUlong(job, JB_job_number);
      nsu = lGetDouble(job, JB_nurg);
   
      enrolled = false;

      for_each (task, lGetList(job, JB_ja_tasks)) {
         sgeee_priority(task, jobid, nsu, min_tix, max_tix);
         enrolled = true;
      }

      if (!enrolled) {
         task = lFirst(lGetList(job, JB_ja_template));
         sgeee_priority(task, jobid, nsu, min_tix, max_tix);
      }
   }
}

/****** sgeee/sgeee_priority() *************************************************
*  NAME
*     sgeee_priority() -- Compute final GEEE priority 
*
*  SYNOPSIS
*     static void sgeee_priority(lListElem *task, u_long32 jobid, double nsu, 
*     double min_tix, double max_tix) 
*
*  FUNCTION
*     The GEEE priority is computed for the task based on the already known
*     ticket amount and already normalized urgency value. The ticket amount
*     is normalized based on the ticket range passed. The weights for
*     ticket and urgency value are applied.
*
*  INPUTS
*     lListElem *task - The task whose priority is computed
*     u_long32 jobid  - The jobs id
*     double nsu      - The normalized urgency value that applies to all 
*                       tasks of the job.
*     double min_tix  - minimum ticket amount 
*     double max_tix  - maximum ticket amount 
*
*  NOTES
*     MT-NOTE: sgeee_priority() is MT safe
*******************************************************************************/
static void sgeee_priority(lListElem *task, u_long32 jobid, double nsu, 
      double min_tix, double max_tix)
{

   double nta, geee_priority;
   double weight_ticket = sconf_get_weight_ticket();
   double weight_urgency = sconf_get_weight_urgency();

   DENTER(TOP_LAYER, "sgeee_priority");

   /* now compute normalized ticket amount (NTA) for each job/task */
   nta = sge_normalize_value(lGetDouble(task, JAT_tix), min_tix, max_tix);
   lSetDouble(task, JAT_ntix, nta);

   /* combine per task NTA with with per job normalized static urgency 
      (NSU) into SGEEE priority */
   geee_priority = weight_urgency * nsu + weight_ticket * nta;

   DPRINTF(("SGEEE priority (" u32 "."u32 ") %f = %f * %f + %f * %f\n",
      jobid, lGetUlong(task, JAT_task_number), geee_priority, 
      weight_urgency, nsu, weight_ticket, nta));

   lSetDouble(task, JAT_ntix, nta);
   lSetDouble(task, JAT_prio, geee_priority);

   DEXIT;
}

/****** sgeee/get_max_ptix() ***************************************************
*  NAME
*     get_max_ptix() -- Get maximum ticket amount
*
*  SYNOPSIS
*     static void get_max_ptix(double *max, lList *job_list) 
*
*  FUNCTION
*     Determines maximum ticket amount in the job list.
*
*  INPUTS
*     double *min     - IN/OUT parameter for minimum
*     double *max     - IN/OUT parameter for maximum
*     lList *job_list - a job list
*
*  NOTES
*
*******************************************************************************/
static void get_max_ptix(double *min, double *max, lList *job_list)
{
   lListElem *job, *task;

   for_each (job, job_list) {
      /* template task contains max amount of tickets for
         unenrolled job arrays */
      if ((task = lFirst(lGetList(job, JB_ja_template)))) {
         *max = MAX(*max,  lGetDouble(task, JAT_tix));
         *min = MIN(*min,  lGetDouble(task, JAT_tix));
      }

      for_each (task, lGetList(job, JB_ja_tasks)) {
         *max = MAX(*max,  lGetDouble(task, JAT_tix));
         *min = MIN(*min,  lGetDouble(task, JAT_tix));
      }
   }

   return;
}


#if 0
/* ----------------------------------------

   calculate_host_tickets()

   calculates the total number of tickets on a host from the
   JAT_tix field for jobs associated with the host


   returns:
      0 successful
     -1 errors in functions called by calculate_host_tickets

*/
int
calculate_host_tickets( lList **running,   /* JB_Type */
                        lList **hosts )    /* EH_Type */
{
   const char *host_name;
   double host_sge_tickets;
   lListElem *hep, *running_job_elem, *rjq;
   lList *help_list;
   const void *iterator = NULL;


   DENTER(TOP_LAYER, "calculate_host_tickets");

   if (!hosts) {
      DEXIT;
      return -1;
   }

  if (!running) {
      for_each (hep, *hosts) {
         lSetDouble(hep, EH_sge_tickets, 0);
      }
      DEXIT;
      return 0;
   }

   for_each (hep, *hosts) { 
      host_name = lGetHost(hep, EH_name);
      lSetDouble(hep, EH_sge_tickets, 0);
      host_sge_tickets = 0;
      for_each (running_job_elem, *running) { 
         lListElem* ja_task;

         for_each(ja_task, lGetList(running_job_elem, JB_ja_tasks)) {  
            help_list = lGetList(ja_task, JAT_granted_destin_identifier_list);
            rjq = lGetElemHostFirst(help_list, JG_qhostname, host_name, &iterator );
            while (rjq != NULL) {
               host_sge_tickets += lGetDouble(ja_task, JAT_tix);
               rjq = lGetElemHostNext(help_list, JG_qhostname, host_name, &iterator);
            }
         }
      }
      lSetDouble(hep, EH_sge_tickets, host_sge_tickets);
   }

   DEXIT;
   return 0;
}
#endif


#ifdef MODULE_TEST

int
main(int argc, char **argv)
{
   int job_number;
   sge_Sdescr_t *lists;
   lList *child_list = NULL;
   lList *group_list = NULL;
   lListElem *job, *config, *node, *usage;

   lListElem *ep;

   lists = (sge_Sdescr_t *)malloc(sizeof(sge_Sdescr_t));
   memset(lists, 0, sizeof(sge_Sdescr_t));


#ifdef __SGE_COMPILE_WITH_GETTEXT__  
   /* init language output for gettext() , it will use the right language */
   sge_init_language_func((gettext_func_type)        gettext,
                         (setlocale_func_type)      setlocale,
                         (bindtextdomain_func_type) bindtextdomain,
                         (textdomain_func_type)     textdomain);
   sge_init_language(NULL,NULL);   
#endif /* __SGE_COMPILE_WITH_GETTEXT__  */


   lInit(nmv);

   /* build host list */
   lAddElemHost(&(lists->host_list), EH_name, "racerx", EH_Type); 
   lAddElemHost(&(lists->host_list), EH_name, "yosemite_sam", EH_Type); 
   lAddElemHost(&(lists->host_list), EH_name, "fritz", EH_Type); 

   /* build queue list */
   ep = lAddElemStr(&(lists->all_queue_list), QU_full_name, "racerx.q", QU_Type); 
   lSetHost(ep, QU_qhostname, "racerx");

   ep = lAddElemStr(&(lists->all_queue_list), QU_full_name, "yosemite_sam.q", QU_Type); 
   lSetHost(ep, QU_qhostname, "yosemite_sam");

   ep = lAddElemStr(&(lists->all_queue_list), QU_full_name, "fritz.q", QU_Type); 
   lSetHost(ep, QU_qhostname, "fritz");
   
   /* build configuration */

   config = lCreateElem(SC_Type);
   lSetUlong(config, SC_halftime, 60*60);
   lSetList(config, SC_usage_weight_list,
       build_usage_list("usageweightlist", NULL));
   for_each(usage, lGetList(config, SC_usage_weight_list))
      lSetDouble(usage, UA_value, 1.0/3.0);
   lSetDouble(config, SC_compensation_factor, 2);
   lSetDouble(config, SC_weight_user, 0.25);
   lSetDouble(config, SC_weight_project, 0.25);
   lSetDouble(config, SC_weight_jobclass, 0.25);
   lSetDouble(config, SC_weight_department, 0.25);
   lSetUlong(config, SC_weight_tickets_functional, 10000);
   lSetUlong(config, SC_weight_tickets_share, 10000);
   lists->config_list = lCreateList("config_list", SC_Type);
   lAppendElem(lists->config_list, config);

   /* build user list */

   ep = lAddElemStr(&(lists->user_list), UP_name, "davidson", UP_Type); 
   lSetUlong(ep, UP_oticket, 0);
   lSetUlong(ep, UP_fshare, 200);

   ep = lAddElemStr(&(lists->user_list), UP_name, "garrenp", UP_Type); 
   lSetUlong(ep, UP_oticket, 0);
   lSetUlong(ep, UP_fshare, 100);

   ep = lAddElemStr(&(lists->user_list), UP_name, "stair", UP_Type); 
   lSetUlong(ep, UP_oticket, 0);
   lSetUlong(ep, UP_fshare, 100);

   /* build project list */
    
   ep = lAddElemStr(&(lists->project_list), UP_name, "sgeee", UP_Type); 
   lSetUlong(ep, UP_oticket, 0);
   lSetUlong(ep, UP_fshare, 200);

   ep = lAddElemStr(&(lists->project_list), UP_name, "ms", UP_Type); 
   lSetUlong(ep, UP_oticket, 0);
   lSetUlong(ep, UP_fshare, 100);


   /* build department list */

   ep = lAddElemStr(&(lists->dept_list), US_name, "software", US_Type); 
   lSetUlong(ep, US_oticket, 0);
   lSetUlong(ep, US_fshare, 200);

   ep = lAddElemStr(&(lists->dept_list), US_name, "hardware", US_Type); 
   lSetUlong(ep, US_oticket, 0);
   lSetUlong(ep, US_fshare, 100);

   /* build share tree list */

   child_list = lCreateList("childlist", STN_Type);

   group_list = lCreateList("grouplist", STN_Type);

   node = lAddElemStr(&child_list, STN_name, "davidson", STN_Type);
   lSetUlong(node, STN_shares, 100);

   node = lAddElemStr(&child_list, STN_name, "garrenp", STN_Type);
   lSetUlong(node, STN_shares, 100);

   node = lAddElemStr(&group_list, STN_name, "groupA", STN_Type);
   lSetUlong(node, STN_shares, 1000);
   lSetList(node, STN_children, child_list);


   child_list = lCreateList("childlist", STN_Type);

   node = lAddElemStr(&child_list, STN_name, "stair", STN_Type);
   lSetUlong(node, STN_shares, 100);

   node = lAddElemStr(&group_list, STN_name, "groupB", STN_Type);
   lSetUlong(node, STN_shares, 100);
   lSetList(node, STN_children, child_list);

   node = lAddElemStr(&(lists->share_tree), STN_name, "root", STN_Type);
   lSetUlong(node, STN_type, STT_USER);
   lSetList(node, STN_children, group_list);

   /* build job list */

   job_number = 1;


   job = lAddElemUlong(&(lists->job_list), JB_job_number, job_number++, 
                           JB_Type);
   lSetUlong(job, JB_jobshare, 0);
   lSetString(job, JB_owner, "davidson");
   lSetString(job, JB_project, "sgeee");
   lSetString(job, JB_department, "software");
   lSetUlong(job, JB_submission_time, sge_get_gmt() - 60*2);
   lSetUlong(job, JB_deadline, 0);
   lSetHost(job, JB_host, "racerx");
   lSetUlong(job, JB_override_tickets, 0);
   lSetList(job, JB_scaled_usage_list, build_usage_list("jobusagelist", NULL));
   for_each(usage, lGetList(job, JB_scaled_usage_list))
      lSetDouble(usage, UA_value, drand48());

   job = lAddElemUlong(&(lists->job_list), JB_job_number, job_number++, 
                           JB_Type);
   lSetUlong(job, JB_jobshare, 1000);
   lSetString(job, JB_owner, "davidson");
   lSetString(job, JB_project, "ms");
   lSetString(job, JB_department, "software");
   lSetUlong(job, JB_submission_time, sge_get_gmt() - 60*2);
   lSetUlong(job, JB_deadline, 0);
   lSetHost(job, JB_host, "racerx");
   lSetUlong(job, JB_override_tickets, 0);
   lSetList(job, JB_scaled_usage_list, build_usage_list("jobusagelist", NULL));
   for_each(usage, lGetList(job, JB_scaled_usage_list))
      lSetDouble(usage, UA_value, drand48());

   job = lAddElemUlong(&(lists->job_list), JB_job_number, job_number++, 
                           JB_Type);
   lSetUlong(job, JB_job_number, job_number++);
   lSetUlong(job, JB_jobshare, 0);
   lSetString(job, JB_owner, "stair");
   lSetString(job, JB_project, "sgeee");
   lSetString(job, JB_department, "hardware");
   lSetUlong(job, JB_submission_time, sge_get_gmt() - 60*2);
   lSetUlong(job, JB_deadline, 0);
   lSetHost(job, JB_host, "racerx");
   lSetUlong(job, JB_override_tickets, 0);
   lSetList(job, JB_scaled_usage_list, build_usage_list("jobusagelist", NULL));
   for_each(usage, lGetList(job, JB_scaled_usage_list))
      lSetDouble(usage, UA_value, drand48());

   job = lAddElemUlong(&(lists->job_list), JB_job_number, job_number++, 
                           JB_Type);
   lSetUlong(job, JB_jobshare, 0);
   lSetString(job, JB_owner, "garrenp");
   lSetString(job, JB_project, "sgeee");
   lSetString(job, JB_department, "software");
   lSetUlong(job, JB_submission_time, sge_get_gmt() - 60*2);
   lSetUlong(job, JB_deadline, sge_get_gmt() + 60*2);
   lSetHost(job, JB_host, "racerx");
   lSetUlong(job, JB_override_tickets, 0);
   lSetList(job, JB_scaled_usage_list, build_usage_list("jobusagelist", NULL));
   for_each(usage, lGetList(job, JB_scaled_usage_list))
      lSetDouble(usage, UA_value, drand48());


   /* call the SGEEE scheduler */

   sge_setup_lists(lists, lists->job_list, NULL);

   sge_calc_tickets(lists, lists->job_list, NULL, NULL);

   sge_calc_tickets(lists, lists->job_list, NULL, NULL);

   lWriteListTo(lists->job_list, stdout);

   sge_calc_share_tree_proportions( lists->share_tree,
                                    lists->user_list,
                                    lists->project_list,
                                    lists->config_list);

   lWriteListTo(lists->share_tree, stdout);

   return 0;
}

#endif


/*-------------------------------------------------------------------------*/
/*-------------------------------------------------------------------------*/


