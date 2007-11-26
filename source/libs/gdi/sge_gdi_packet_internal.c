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

#include <stdlib.h>
#include <string.h>

#ifdef KERBEROS
#  include "krb_lib.h"
#endif

#include "basis_types.h"

#include "comm/commlib.h"

#include "rmon/sgermon.h"

#include "lck/sge_mtutil.h"

#include "uti/sge_log.h"

#include "gdi/sge_gdi2.h"
#include "gdi/sge_gdi_packet_pb_cull.h"
#include "gdi/sge_security.h"
#include "gdi/sge_gdi_packet.h"
#include "gdi/sge_gdi_packet_queue.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_multiL.h"

#include "msg_common.h"
#include "msg_gdilib.h"


/****** gdi/request_internal/sge_gdi_packet_create_multi_answer() ***********
*  NAME
*     sge_gdi_packet_create_multi_answer() -- create multi answer 
*
*  SYNOPSIS
*     static bool 
*     sge_gdi_packet_create_multi_answer(sge_gdi_ctx_class_t* ctx, 
*                                        lList **answer_list, 
*                                        sge_gdi_packet_class_t **packet, 
*                                        lList **malpp) 
*
*  FUNCTION
*     Creates a multi answer element ("malpp") from the given "packet".
*     The lists and answer lists for the (multi) GDI request will be
*     moved from the task structures conteined in the packet, into the
*     multi answer list. After all information has been moved the packet
*     and all subelement will be freed so that the *packet will be NULL
*     when the function returns.
*
*     Threre are no errors expected from this function. So the return 
*     value "false" or a filled "answer_list" will never bee seen
*     after return.
*
*  INPUTS
*     sge_gdi_ctx_class_t* ctx        - context (not used) 
*     lList **answer_list             - answer_list (not used) 
*     sge_gdi_packet_class_t **packet - packet 
*     lList **malpp                   - multi answer
*
*  RESULT
*     static bool - error state
*        true  - always
*
*  NOTES
*     MT-NOTE: sge_gdi_packet_create_multi_answer() is MT safe 
*
*  SEE ALSO
*     gdi/request_internal/sge_gdi_packet_execute_external() 
*     gdi/request_internal/sge_gdi_packet_execute_internal() 
*     gdi/request_internal/sge_gdi_packet_wait_for_result_external()
*     gdi/request_internal/sge_gdi_packet_wait_for_result_internal()
*     sge_gdi_packet_is_handled()
*******************************************************************************/
static bool
sge_gdi_packet_create_multi_answer(sge_gdi_ctx_class_t* ctx, lList **answer_list,
                                   sge_gdi_packet_class_t **packet, lList **malpp)
{
   sge_gdi_task_class_t *task = NULL;
   bool ret = true;

   DENTER(TOP_LAYER, "sge_packet_create_multi_answer");

   /* 
    * make multi answer list and move all data contained in packet 
    * into that structure 
    */
   task = (*packet)->first_task;
   while (task != NULL) {
      u_long32 operation = SGE_GDI_GET_OPERATION(task->command);
      u_long32 sub_command = SGE_GDI_GET_SUBCOMMAND(task->command);
      lListElem *map = lAddElemUlong(malpp, MA_id, task->id, MA_Type);

      if (operation == SGE_GDI_GET || operation == SGE_GDI_PERMCHECK ||
          (operation == SGE_GDI_ADD && sub_command == SGE_GDI_RETURN_NEW_VERSION)) {
         lSetList(map, MA_objects, task->data_list);
         task->data_list = NULL;
      }

      lSetList(map, MA_answers, task->answer_list);
      task->answer_list = NULL;

      task = task->next;
   }

   /*
    * It is time to free the element. It is really not needed anymore.
    */
   sge_gdi_packet_free(packet);

   DRETURN(ret);
}


/****** gdi/request_internal/sge_gdi_packet_wait_till_handled() *************
*  NAME
*     sge_gdi_packet_wait_till_handled() -- wait til packet is handled 
*
*  SYNOPSIS
*     void 
*     sge_gdi_packet_wait_till_handled(sge_gdi_packet_class_t *packet) 
*
*  FUNCTION
*     This function blocks the calling thread till another one executes
*     sge_gdi_packet_broadcast_that_handled(). Mutiple threads can use
*     this call to get response if the packet is accessed by someone 
*     else anymore.
*
*     This function is used to synchronize packet producers (listerner,
*     scheduler, jvm thread ...) with packet consumers (worker threads)
*     which all use a packet queue to synchronize the access to
*     packet elements. 
*
*     Packet producers store packets in the packet queue and then
*     they call this function to wait that they can access the packet
*     structure again. 
*
*  INPUTS
*     sge_gdi_packet_class_t *packet - packet element 
*
*  RESULT
*     void - none
*
*  NOTES
*     MT-NOTE: sge_gdi_packet_wait_till_handled() is MT safe 
*
*  SEE ALSO
*     gdi/request_internal/Master_Packet_Queue
*     gdi/request_internal/sge_gdi_packet_queue_wait_for_new_packet()
*     gdi/request_internal/sge_gdi_packet_queue_store_notify()
*     gdi/request_internal/sge_gdi_packet_broadcast_that_handled()
*     gdi/request_internal/sge_gdi_packet_is_handled()
*******************************************************************************/
void
sge_gdi_packet_wait_till_handled(sge_gdi_packet_class_t *packet)
{
   DENTER(TOP_LAYER, "sge_gdi_packet_wait_till_handled");

   if (packet != NULL) {
      cl_thread_settings_t *thread_config = cl_thread_get_thread_config();

      sge_mutex_lock(GDI_PACKET_MUTEX, SGE_FUNC, __LINE__, &(packet->mutex));

      while (packet->is_handled == false) {
         DPRINTF((SFN" is waiting for packet to be handling by worker\n", 
                  thread_config->thread_name));
         pthread_cond_wait(&(packet->cond), &(packet->mutex));
      }
      DPRINTF((SFN" got signal that packet is handled\n", thread_config->thread_name));

      sge_mutex_unlock(GDI_PACKET_MUTEX, SGE_FUNC, __LINE__, &(packet->mutex));
   }

   DRETURN_VOID;   
}

/****** gdi/request_internal/sge_gdi_packet_is_handled() ********************
*  NAME
*     sge_gdi_packet_is_handled() -- returns if packet was handled by worker
*
*  SYNOPSIS
*     void 
*     sge_gdi_packet_is_handled(sge_gdi_packet_class_t *packet) 
*
*  FUNCTION
*     Returns if the given packet was already handled by a worker thread.
*     "true" means that the packet is completely done so that a call
*     to sge_gdi_packet_wait_till_handled() will return immediately. If 
*     "false" is returned the the packet is not finished so a call to
*     sge_gdi_packet_wait_till_handled() might block when it is called 
*     afterwards.
*
*  INPUTS
*     sge_gdi_packet_class_t *packet - packet element 
*
*  RESULT
*     bool - true    packet was already handled by a worker
*            false   packet is not done. 
*
*  NOTES
*     MT-NOTE: sge_gdi_packet_is_handled() is MT safe 
*
*  SEE ALSO
*     gdi/request_internal/Master_Packet_Queue
*     gdi/request_internal/sge_gdi_packet_queue_wait_for_new_packet()
*     gdi/request_internal/sge_gdi_packet_queue_store_notify()
*     gdi/request_internal/sge_gdi_packet_broadcast_that_handled()
*******************************************************************************/
bool
sge_gdi_packet_is_handled(sge_gdi_packet_class_t *packet)
{
   bool ret = true;

   DENTER(TOP_LAYER, "sge_gdi_packet_wait_till_handled");
   if (packet != NULL) {   
      sge_mutex_lock(GDI_PACKET_MUTEX, SGE_FUNC, __LINE__, &(packet->mutex));
      ret = packet->is_handled;
      sge_mutex_unlock(GDI_PACKET_MUTEX, SGE_FUNC, __LINE__, &(packet->mutex));
   }
   DRETURN(ret);
}

/****** gdi/request_internal/sge_gdi_packet_broadcast_that_handled() ********
*  NAME
*     sge_gdi_packet_broadcast_that_handled() -- broadcast to waiting threads 
*
*  SYNOPSIS
*     void 
*     sge_gdi_packet_broadcast_that_handled(sge_gdi_packet_class_t *packet) 
*
*  FUNCTION
*     This functions wakes up all threads waiting in 
*     sge_gdi_packet_wait_till_handled(). 
*
*     This function is used to synchronize packet producers (listerner,
*     scheduler, jvm thread ...) with packet consumers (worker threads)
*     which all use a packet queue to synchronize the access to
*     packet elements. 
*
*     Packet producers store packets in the packet queue and then
*     they call sge_gdi_packet_wait_till_handled(). Packet consumers
*     fetch a packet from the queue. After they have finished using
*     the packet structure they call this function to notify
*     the waiting threads that the packet is not accessed anymore.
*
*  INPUTS
*     sge_gdi_packet_class_t *packet - packet element 
*
*  RESULT
*     void - NONE
*
*  NOTES
*     MT-NOTE: sge_gdi_packet_broadcast_that_handled() is MT safe 
*
*  SEE ALSO
*     gdi/request_internal/Master_Packet_Queue
*     gdi/request_internal/sge_gdi_packet_queue_wait_for_new_packet()
*     gdi/request_internal/sge_gdi_packet_queue_store_notify()
*     gdi/request_internal/sge_gdi_packet_wait_till_handled()
*******************************************************************************/
void
sge_gdi_packet_broadcast_that_handled(sge_gdi_packet_class_t *packet)
{
   cl_thread_settings_t *thread_config = cl_thread_get_thread_config();

   DENTER(TOP_LAYER, "sge_gdi_packet_broadcast_that_handled");

   sge_mutex_lock(GDI_PACKET_MUTEX, SGE_FUNC, __LINE__, &(packet->mutex));
   packet->is_handled = true; 
   DPRINTF((SFN" broadcasts that packet is handled\n", thread_config->thread_name));
   pthread_cond_broadcast(&(packet->cond));
   sge_mutex_unlock(GDI_PACKET_MUTEX, SGE_FUNC, __LINE__, &(packet->mutex));

   DRETURN_VOID;   
}

/****** gdi/request_internal/sge_gdi_packet_execute_external() ****************
*  NAME
*     sge_gdi_packet_execute_external() -- execute a GDI packet 
*
*  SYNOPSIS
*     bool 
*     sge_gdi_packet_execute_external(sge_gdi_ctx_class_t* ctx, 
*                                     lList **answer_list, 
*                                     sge_gdi_packet_class_t *packet) 
*
*  FUNCTION
*     This functions sends a GDI "packet" from an external client
*     to the qmaster process. If the packet is handled on master side
*     the response is send back to the client which then will fill
*     "packet" with the received information.
*
*     To send packets from internal clients (threads) the function 
*     sge_gdi_packet_execute_internal() has to be used.
*
*     Please note that in contrast to sge_gdi_packet_execute_internal()
*     this function assures that the GDI request contained in the
*     "packet" is completely executed (either successfull or with errors)
*     after this function returns.
*
*     a GDI multi answer lists structure from the information contained
*     in the packet after this function has been called.
*     
*  INPUTS
*     sge_gdi_ctx_class_t* ctx       - context handle 
*     lList **answer_list            - answer list 
*     sge_gdi_packet_class_t *packet - packet 
*
*  RESULT
*     bool - error state
*        true   - success
*        false  - error (answer_lists will contain details)
*
*  NOTES
*     MT-NOTE: sge_gdi_packet_execute_extern() is MT safe 
*
*  SEE ALSO
*     gdi/request_internal/sge_gdi_packet_execute_external() 
*     gdi/request_internal/sge_gdi_packet_execute_internal() 
*     gdi/request_internal/sge_gdi_packet_wait_for_result_external()
*     gdi/request_internal/sge_gdi_packet_wait_for_result_internal()
*******************************************************************************/
bool 
sge_gdi_packet_execute_external(sge_gdi_ctx_class_t* ctx, lList **answer_list, 
                                sge_gdi_packet_class_t *packet) 
{
   bool ret = true;
   sge_pack_buffer pb;
   sge_pack_buffer rpb;
   sge_gdi_packet_class_t *ret_packet = NULL;
   int commlib_error;
   u_long32 message_id;

   DENTER(TOP_LAYER, "sge_gdi_packet_execute_extern");

   /* here the packet gets a unique request id */
   packet->id = gdi_state_get_next_request_id();

#ifdef KERBEROS
   /* request that the Kerberos library forward the TGT */
   if (ret && packet->first_task->target == SGE_JOB_LIST && 
       SGE_GDI_GET_OPERATION(packet->first_task->command) == SGE_GDI_ADD) {
      krb_set_client_flags(krb_get_client_flags() | KRB_FORWARD_TGT);
      krb_set_tgt_id(packet->id);
   }
#endif

   /* 
    * pack packet into packbuffer
    */ 
   /* 
    * EB: TODO: dry run necessary to calculate initial buffer size?  
    *    JG told me during review of ST that it might be possible that
    *    the dry run to calculate the buffer size might be slower
    *    than direcly packing. RD might have done tests...
    */
   if (ret) {
      size_t size = sge_gdi_packet_get_pb_size(packet);

      if (size > 0) {
         int pack_ret;

         pack_ret = init_packbuffer(&pb, size, 0);
         if (pack_ret != PACK_SUCCESS) {
            SGE_ADD_MSG_ID(sprintf(SGE_EVENT, "unable to prepare packbuffer for sending request"));
            ret = false;
         }
      }
   }
   if (ret) {
      ret = sge_gdi_packet_pack(packet, answer_list, &pb);
   }

   /* 
    * send packbuffer to master. keep care that user does not see
    * commlib related error messages if master is not up and running
    */
   if (ret) {
      const char *commproc = prognames[QMASTER];
      const char *host = ctx->get_master(ctx, false);
      int id = 1;
      int response_id = 0;
      lList *tmp_answer_list = NULL;

      commlib_error = sge_gdi2_send_any_request(ctx, 1, &message_id, host, commproc, id, &pb,
                                                TAG_GDI_REQUEST, response_id, &tmp_answer_list);
      if (commlib_error != CL_RETVAL_OK) {
         ret = false;
         commlib_error = ctx->is_alive(ctx);
         if (commlib_error != CL_RETVAL_OK) {
            u_long32 sge_qmaster_port = ctx->get_sge_qmaster_port(ctx);
            const char *mastername = ctx->get_master(ctx, false);

            if (commlib_error == CL_RETVAL_CONNECT_ERROR ||
                commlib_error == CL_RETVAL_CONNECTION_NOT_FOUND ) {
               /* For the default case, just print a simple message */
               SGE_ADD_MSG_ID(sprintf(SGE_EVENT, MSG_GDI_UNABLE_TO_CONNECT_SUS,
                                      prognames[QMASTER], sge_u32c(sge_qmaster_port),
                                      mastername?mastername:"<NULL>"));            
            } else { 
               /* For unusual errors, give more detail */
               SGE_ADD_MSG_ID(sprintf(SGE_EVENT, MSG_GDI_CANT_SEND_MSG_TO_PORT_ON_HOST_SUSS,
                                      prognames[QMASTER], sge_u32c(sge_qmaster_port),
                                      mastername?mastername:"<NULL>", 
                                      cl_get_error_text(commlib_error))); 
            }
            lFreeList(&tmp_answer_list);
         } else {
            SGE_ADD_MSG_ID(sprintf(SGE_EVENT, MSG_GDI_SENDINGGDIREQUESTFAILED));
         }
         answer_list_add(answer_list, SGE_EVENT, STATUS_NOQMASTER, ANSWER_QUALITY_ERROR);
         ret = false;
      }
   }

   /* 
    * wait for response from master; also here keep care that commlib
    * related error messages are hidden if master is not up and running anymore
    */
   if (ret) {
      const char *commproc = prognames[QMASTER];
      const char *host = ctx->get_master(ctx, false);
      char rcv_host[CL_MAXHOSTLEN+1];
      char rcv_commproc[CL_MAXHOSTLEN+1];
      int tag = TAG_GDI_REQUEST;
      u_short id = 1;

      strcpy(rcv_host, host);
      strcpy(rcv_commproc, commproc);
      commlib_error = sge_gdi2_get_any_request(ctx, rcv_host, rcv_commproc, &id, &rpb, &tag, 
                                               true, message_id, NULL);
      if (commlib_error != CL_RETVAL_OK) {
         ret = false;
         commlib_error = ctx->is_alive(ctx);
         if (commlib_error != CL_RETVAL_OK) {
            u_long32 sge_qmaster_port = ctx->get_sge_qmaster_port(ctx);
            const char *mastername = ctx->get_master(ctx, false);

            if (commlib_error == CL_RETVAL_CONNECT_ERROR ||
                commlib_error == CL_RETVAL_CONNECTION_NOT_FOUND ) {
               /* For the default case, just print a simple message */
               SGE_ADD_MSG_ID(sprintf(SGE_EVENT, MSG_GDI_UNABLE_TO_CONNECT_SUS,
                                      prognames[QMASTER], sge_u32c(sge_qmaster_port),
                                      mastername?mastername:"<NULL>"));            
            } else { 
               /* For unusual errors, give more detail */
               SGE_ADD_MSG_ID(sprintf(SGE_EVENT, MSG_GDI_CANT_SEND_MSG_TO_PORT_ON_HOST_SUSS,
                                      prognames[QMASTER], sge_u32c(sge_qmaster_port),
                                      mastername?mastername:"<NULL>", 
                                      cl_get_error_text(commlib_error))); 
            }
         } else {
            SGE_ADD_MSG_ID(sprintf(SGE_EVENT, MSG_GDI_RECEIVEGDIREQUESTFAILED));
         }
         answer_list_add(answer_list, SGE_EVENT, STATUS_NOQMASTER, ANSWER_QUALITY_ERROR);
         ret = false;
      }
   }

   /* 
    * unpack result. the returned packet contains data and/or answer lists 
    */
   if (ret) {
      ret = sge_gdi_packet_unpack(&ret_packet, answer_list, &rpb);
      clear_packbuffer(&rpb); 
   }
  
   /* 
    * consistency check of received data:
    *    - is the packet id the same
    *    - does it contain the same ammount of tasks
    *    - is the task sequence and the task id of each received task the same
    */
   if (ret) {
      sge_gdi_task_class_t *send;
      sge_gdi_task_class_t *recv;
      bool gdi_mismatch = false;

      if (packet->id != ret_packet->id) {
         gdi_mismatch = true;
      }

      send = packet->first_task;
      recv = ret_packet->first_task;
      while (send != NULL && recv != NULL) {
         if (send->id == recv->id) {
            send->data_list = recv->data_list;
            send->answer_list = recv->answer_list;
            recv->data_list = NULL;
            recv->answer_list = NULL;
         } else {
            gdi_mismatch = true;
            break;
         }
         send = send->next;
         recv = recv->next;
      }
      if (send != NULL || recv != NULL) {
         gdi_mismatch = true;
      }
      if (gdi_mismatch) {
         /* For unusual errors, give more detail */
         SGE_ADD_MSG_ID(sprintf(SGE_EVENT, MSG_GDI_MISMATCH_SEND_RECEIVE));
         answer_list_add(answer_list, SGE_EVENT, STATUS_NOQMASTER, ANSWER_QUALITY_ERROR);
         ret = false;
      }
   }

#ifdef KERBEROS
   /* clear the forward TGT request */
   if (ret && state->first->target == SGE_JOB_LIST &&
       SGE_GDI_GET_OPERATION(packet->first_task->command) == SGE_GDI_ADD) {
      krb_set_client_flags(krb_get_client_flags() & ~KRB_FORWARD_TGT);
      krb_set_tgt_id(0);
   }
#endif

   DRETURN(ret);
}

/****** gdi/request_internal/sge_gdi_packet_execute_internal() ****************
*  NAME
*     sge_gdi_packet_execute_internal() -- execute a GDI packet 
*
*  SYNOPSIS
*     bool 
*     sge_gdi_packet_execute_internal(sge_gdi_ctx_class_t* ctx, 
*                                     lList **answer_list, 
*                                     sge_gdi_packet_class_t *packet) 
*
*  FUNCTION
*     This functions stores a GDI "packet" in the "Master_Packet_Queue"
*     so that it will be executed in future. This function can only
*     be used in the context of an internal GDI client (thread in 
*     qmaster). 
*
*     To send packets from external clients 
*     the function sge_gdi_packet_execute_external() has to be used.
*
*     Please note that in contrast to sge_gdi_packet_execute_external()
*     this function does not assures that the GDI request contained in the
*     "packet" is already executed after this function returns.
*
*     sge_gdi_packet_wait_for_result_internal() has to be called to 
*     assure this. This function will also creates a GDI multi answer 
*     lists structure from the information contained in the handled
*     packet after this function has been called.
*
*     
*  INPUTS
*     sge_gdi_ctx_class_t* ctx       - context handle 
*     lList **answer_list            - answer list 
*     sge_gdi_packet_class_t *packet - packet 
*
*  RESULT
*     bool - error state
*        true   - success
*        false  - error (answer_lists will contain details)
*
*  NOTES
*     MT-NOTE: sge_gdi_packet_execute_extern() is MT safe 
*
*  SEE ALSO
*     gdi/request_internal/sge_gdi_packet_execute_external() 
*     gdi/request_internal/sge_gdi_packet_execute_internal() 
*     gdi/request_internal/sge_gdi_packet_wait_for_result_external()
*     gdi/request_internal/sge_gdi_packet_wait_for_result_internal()
*******************************************************************************/
bool 
sge_gdi_packet_execute_internal(sge_gdi_ctx_class_t* ctx, lList **answer_list, 
                                sge_gdi_packet_class_t *packet) 
{
   bool ret = true;

   DENTER(TOP_LAYER, "sge_gdi_packet_execute_internal");

   /* 
    * here the packet gets a unique request id and source for host
    * user and group is initialized
    */
   packet->id = gdi_state_get_next_request_id();
   packet->commproc = strdup(prognames[QMASTER]);      
   packet->host = strdup(ctx->get_master(ctx, false));
   packet->is_intern_request = true;

   ret = sge_gdi_packet_parse_auth_info(packet, &(packet->first_task->answer_list),
                                        &(packet->uid), packet->user, sizeof(packet->user),
                                        &(packet->gid), packet->group, sizeof(packet->group));

   /* 
    * append the packet to the packet list of the worker threads
    */
   sge_gdi_packet_queue_store_notify(&Master_Packet_Queue, packet);


   DRETURN(ret);
}

/****** gdi/request_internal/sge_gdi_packet_wait_for_result_external() ******
*  NAME
*     sge_gdi_packet_wait_for_result_external() -- wait for packet result 
*
*  SYNOPSIS
*     bool 
*     sge_gdi_packet_wait_for_result_external(sge_gdi_ctx_class_t* ctx, 
*                                             lList **answer_list, 
*                                             sge_gdi_packet_class_t *packet, 
*                                             lList **malpp) 
*
*  FUNCTION
*     Despite to its name this function does not wait. This is not necessary
*     because the GDI request handled in the execution process previously
*     is already done. A call to this function simply breates a GDI multi 
*     answer list.
*
*  INPUTS
*     sge_gdi_ctx_class_t* ctx        - context handle 
*     lList **answer_list             - answer list 
*     sge_gdi_packet_class_t **packet - GDI packet 
*     lList **malpp                   - multi answer list 
*
*  RESULT
*     bool - error state
*        true  - success
*        false - error
*
*  NOTES
*     MT-NOTE: sge_gdi_packet_wait_for_result_external() is MT safe 
*
*  SEE ALSO
*     gdi/request_internal/sge_gdi_packet_execute_external() 
*     gdi/request_internal/sge_gdi_packet_execute_internal() 
*     gdi/request_internal/sge_gdi_packet_wait_for_result_external()
*     gdi/request_internal/sge_gdi_packet_wait_for_result_internal()
*******************************************************************************/
bool 
sge_gdi_packet_wait_for_result_external(sge_gdi_ctx_class_t* ctx, lList **answer_list,
                                        sge_gdi_packet_class_t **packet, lList **malpp)
{
   bool ret = true;

   DENTER(TOP_LAYER, "sge_gdi_packet_wait_for_result_extern");

   /* 
    * The packet itself has already be executed in sge_gdi_packet_execute_external() 
    * so it is only necessary to create the muti answer and do cleanup
    */
   ret = sge_gdi_packet_create_multi_answer(ctx, answer_list, packet, malpp);

   DRETURN(ret);
}

/****** gdi/request_internal/sge_gdi_packet_wait_for_result_internal() ******
*  NAME
*     sge_gdi_packet_wait_for_result_internal() -- wait for handled packet 
*
*  SYNOPSIS
*     bool 
*     sge_gdi_packet_wait_for_result_internal(sge_gdi_ctx_class_t* ctx, 
*                                             lList **answer_list, 
*                                             sge_gdi_packet_class_t *packet, 
*                                             lList **malpp) 
*
*  FUNCTION
*     This function can only be called in a qmaster thread. Then
*     this function blocks until the GDI packet, which has to be
*     given to qmaster via sge_gdi_packet_execute_internal(), is
*     executed completely (either successfull or with errors). 
*
*     After that it creates a multi answer list.
*
*  INPUTS
*     sge_gdi_ctx_class_t* ctx        - context handle 
*     lList **answer_list             - answer list 
*     sge_gdi_packet_class_t **packet - GDI packet 
*     lList **malpp                   - multi answer list 
*
*  RESULT
*     bool - error state
*        true  - success
*        false - error
*
*  NOTES
*     MT-NOTE: sge_gdi_packet_wait_for_result_internal() is MT safe 
*
*  SEE ALSO
*     gdi/request_internal/sge_gdi_packet_execute_external() 
*     gdi/request_internal/sge_gdi_packet_execute_internal() 
*     gdi/request_internal/sge_gdi_packet_wait_for_result_external()
*     gdi/request_internal/sge_gdi_packet_wait_for_result_internal()
*******************************************************************************/
bool 
sge_gdi_packet_wait_for_result_internal(sge_gdi_ctx_class_t* ctx, lList **answer_list,
                                        sge_gdi_packet_class_t **packet, lList **malpp)
{
   bool ret = true;

   DENTER(TOP_LAYER, "sge_gdi_packet_wait_for_result_internal");

   /* 
    * wait for response from worker thread that the packet is handled
    */
   sge_gdi_packet_wait_till_handled(*packet);

   /*
    * create the multi answer and destroy the packet
    */
   ret = sge_gdi_packet_create_multi_answer(ctx, answer_list, packet, malpp);

   DRETURN(ret);
}
