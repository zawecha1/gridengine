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
/*
   This is the module for handling usermap.
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

#include "sge.h"
#include "sgermon.h"
#include "sge_conf.h"
#include "sge_log.h"
#include "sge_c_gdi.h"
#include "sge_string.h"
#include "sge_utility.h"
#include "sge_answer.h"
#include "sge_unistd.h"
#include "sge_hgroup.h"
#include "sge_cuser.h"
#include "sge_event_master.h"
#include "sge_persistence_qmaster.h"

#include "spool/classic/read_write_ume.h"
#include "spool/sge_spooling.h"

#include "msg_common.h"
#include "msg_qmaster.h"

#ifndef __SGE_NO_USERMAPPING__

int usermap_mod(lList **answer_list, lListElem *cuser, lListElem *reduced_elem,                 int add, const char *remote_user, const char *remote_host,
                gdi_object_t *object, int sub_command) 
{
   bool ret = true;
   int pos;

   DENTER(TOP_LAYER, "usermap_mod");
   
   if (ret) {
      pos = lGetPosViaElem(reduced_elem, CU_name);
      if (pos >= 0) {
         const char *name = lGetPosString(reduced_elem, pos);

         if (add) {
            if (!verify_str_key(answer_list, name, "cuser")) {
               lSetString(cuser, CU_name, name);
            } else {
               ERROR((SGE_EVENT, MSG_UM_CLUSTERUSERXNOTGUILTY_S, name));
               answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, 
                               ANSWER_QUALITY_ERROR);
               ret = false;
            }
         } else {
            const char *old_name = lGetString(cuser, CU_name);

            if (strcmp(old_name, name)) {
               /* EB: move to message file */
               ERROR((SGE_EVENT, MSG_UME_NONAMECHANGE));
               answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX,
                               ANSWER_QUALITY_ERROR);
               ret = false;
            }
         }
      } else {
         ERROR((SGE_EVENT, MSG_SGETEXT_MISSINGCULLFIELD_SS,
                lNm2Str(HGRP_name), SGE_FUNC));
         answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN,
                         ANSWER_QUALITY_ERROR);
         ret = false;
      }
   }

   if (ret) {
      pos = lGetPosViaElem(reduced_elem, CU_ruser_list);
   
      if (pos >= 0) {
         lList *hostattr_list = lGetPosList(reduced_elem, pos);

         lSetList(cuser, CU_ruser_list, lCopyList("", hostattr_list));
      }
   }
   
   if (ret) {
      pos = lGetPosViaElem(reduced_elem, CU_ulong32);
   
      if (pos >= 0) {
         lList *hostattr_list = lGetPosList(reduced_elem, pos);

         lSetList(cuser, CU_ulong32, lCopyList("", hostattr_list));
      }
   }

   if (ret) {
      pos = lGetPosViaElem(reduced_elem, CU_bool);
   
      if (pos >= 0) {
         lList *hostattr_list = lGetPosList(reduced_elem, pos);

         lSetList(cuser, CU_bool, lCopyList("", hostattr_list));
      }
   }

   DEXIT;
   if (ret) {
      return 0;
   } else {
      return STATUS_EUNKNOWN;
   }
}

int usermap_success(lListElem *cuser, lListElem *old_cuser, 
                    gdi_object_t *object) 
{
   DENTER(TOP_LAYER, "usermap_success");
   sge_add_event(NULL, 0, old_cuser?sgeE_CUSER_MOD:sgeE_CUSER_ADD, 0, 
                 0, lGetString(cuser, CU_name), cuser);
   lListElem_clear_changed_info(cuser);
   DEXIT;
   return 0;
}

int usermap_spool(lList **answer_list, lListElem *upe, gdi_object_t *object) 
{  
   DENTER(TOP_LAYER, "usermap_spool");
 
   if (!spool_write_object(NULL, spool_get_default_context(), upe, lGetString(upe, CU_name), SGE_TYPE_CUSER)) {
      const char* clusterUser = NULL;
      clusterUser = lGetString(upe, CU_name); 
      ERROR((SGE_EVENT, MSG_UM_ERRORWRITESPOOLFORUSER_S, clusterUser ));
      answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      DEXIT;
      return 1;
   }
 
   DEXIT;
   return 0;
}

int sge_del_usermap(lListElem *this_elem, lList **answer_list, 
                    char *remote_user, char *remote_host) 
{
   bool ret = true;

   DENTER(TOP_LAYER, "sge_del_usermap");
   if (this_elem != NULL && remote_user != NULL && remote_host != NULL) {
      const char *name = lGetString(this_elem, CU_name);

      if (name != NULL) {
         lList *master_cuser_list = *(cuser_list_get_master_list());
         lListElem *cuser = cuser_list_locate(master_cuser_list, name);
   
         if (cuser != NULL) {
            if (sge_event_spool(answer_list, 0, sgeE_CUSER_DEL,
                                0, 0, name,
                                NULL, NULL, NULL, true)) {
               lRemoveElem(master_cuser_list, cuser);

               INFO((SGE_EVENT, MSG_SGETEXT_REMOVEDFROMLIST_SSSS, 
                     remote_user, remote_host, name, 
                     "user mapping entry"  ));
               answer_list_add(answer_list, SGE_EVENT, STATUS_OK, 
                               ANSWER_QUALITY_INFO);
            } else {
               ERROR((SGE_EVENT, MSG_SGETEXT_CANTSPOOL_SS, 
                      "user mapping entry", name));
               answer_list_add(answer_list, SGE_EVENT, STATUS_EEXIST, 
                               ANSWER_QUALITY_ERROR);
               ret = false;
            }
         } else {
            ERROR((SGE_EVENT, MSG_SGETEXT_DOESNOTEXIST_SS,
                   "user mapping entry", name));
            answer_list_add(answer_list, SGE_EVENT, STATUS_EEXIST,
                            ANSWER_QUALITY_ERROR);
            ret = false;
         }
      } else {
         ERROR((SGE_EVENT, MSG_SGETEXT_MISSINGCULLFIELD_SS,
                lNm2Str(CU_name), SGE_FUNC));
         answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, 
                         ANSWER_QUALITY_ERROR);
         ret = false;
      } 
   } else {
      CRITICAL((SGE_EVENT, MSG_SGETEXT_NULLPTRPASSED_S, SGE_FUNC));
      answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, 
                      ANSWER_QUALITY_ERROR);
      ret = false;
   }
    
   DEXIT;
   if (ret) {
      return STATUS_OK;
   } else {
      return STATUS_EUNKNOWN;
   } 
}

#endif
