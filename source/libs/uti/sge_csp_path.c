/*___INFO__MARK_BEGIN__*/
/*************************************************************************
 * 
 *  The Contents of thiz file are made available subject to the terms of
 *  the Sun Industry Standards Source License Version 1.2
 * 
 *  Sun Microsystems Inc., March, 2001
 * 
 * 
 *  Sun Industry Standards Source License Version 1.2
 *  =================================================
 *  The contents of thiz file are subject to the Sun Industry Standards
 *  Source License Version 1.2 (the "License"); You may not use thiz file
 *  except in compliance with the License. You may obtain a copy of the
 *  License at http://gridengine.sunsource.net/Gridengine_SISSL_license.html
 * 
 *  Software provided under thiz License is provided on an "AS IS" basis,
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

#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pwd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>  

#include "sge.h"
#include "sgermon.h"
#include "sge_hostname.h"
#include "sge_log.h"
#include "sge_stdlib.h"
#include "sge_string.h"
#include "sge_unistd.h"
#include "sge_uidgid.h"
#include "sge_answer.h"
#include "sge_prog.h"
#include "msg_gdilib.h"

#include "sge_csp_path.h"

#define SGE_QMASTER_PORT_ENVIRONMENT_NAME "SGE_QMASTER_PORT"
#define SGE_COMMD_SERVICE "sge_qmaster"
#define CA_DIR          "common/sgeCA"
#define CA_LOCAL_DIR    "/var/sgeCA"
#define CaKey           "cakey.pem"
#define CaCert          "cacert.pem"
#define SGESecPath      ".sge"
#define UserKey         "key.pem"
#define RandFile        "rand.seed"
#define UserCert        "cert.pem"
#define ReconnectFile   "private/reconnect.dat"
#define VALID_MINUTES    7          /* expiry of connection        */


typedef struct {
   char* CA_cert_file;           /* CA certificate file */
   char* CA_key_file;            /* CA's private key file */
   char* cert_file;              /* user certificate file */
   char* key_file;               /* user's key file */
   char* rand_file;              /* rand file */
   char* reconnect_file;         /* reconnect data file (not used) */
   unsigned long refresh_time;   /* connection refresh time (not used) */
   char* password;               /* password for encrypted keyfiles (not used) */
   cl_ssl_verify_func_t verify_func; /* cert verify function */
} sge_csp_path_t;

static bool sge_csp_path_setup(sge_csp_path_class_t *thiz, 
                               sge_env_state_class_t *sge_env, 
                               sge_prog_state_class_t *sge_prog, 
                               sge_error_class_t *eh);
static void sge_csp_path_destroy(void *theState);
static void sge_csp_path_dprintf(sge_csp_path_class_t *thiz);
static const char* get_CA_cert_file(sge_csp_path_class_t *thiz);
static const char* get_CA_key_file(sge_csp_path_class_t *thiz);
static const char* get_cert_file(sge_csp_path_class_t *thiz);
static const char* get_key_file(sge_csp_path_class_t *thiz);
static const char* get_rand_file(sge_csp_path_class_t *thiz);
static const char* get_reconnect_file(sge_csp_path_class_t *thiz);
static const char* get_password(sge_csp_path_class_t *thiz);
static int get_refresh_time(sge_csp_path_class_t *thiz);
static cl_ssl_verify_func_t get_verify_func(sge_csp_path_class_t *thiz);
static void set_CA_cert_file(sge_csp_path_class_t *thiz, const char *CA_cert_file);
static void set_CA_key_file(sge_csp_path_class_t *thiz, const char *CA_key_file);
static void set_cert_file(sge_csp_path_class_t *thiz, const char *cert_file);
static void set_key_file(sge_csp_path_class_t *thiz, const char *key_file);
static void set_rand_file(sge_csp_path_class_t *thiz, const char *rand_file);
static void set_reconnect_file(sge_csp_path_class_t *thiz, const char *reconnect_file);
static void set_refresh_time(sge_csp_path_class_t *thiz, u_long32 refresh_time);
static void set_password(sge_csp_path_class_t *thiz, const char *password);
static void set_verify_func(sge_csp_path_class_t *thiz, cl_ssl_verify_func_t func);
static cl_bool_t ssl_cert_verify_func(cl_ssl_verify_mode_t mode, cl_bool_t service_mode, const char* value);


/* TODO: move to sge_prog.c */
static bool is_daemon(const char* progname);
/* static bool is_master(const char* progname); */

static bool is_daemon(const char* progname) {
   if (progname != NULL) {
      if ( !strcmp(prognames[QMASTER], progname) ||
           !strcmp(prognames[EXECD]  , progname) ||
           !strcmp(prognames[SCHEDD] , progname)) {
         return true;
      }
   }
   return false;
}

#if 0
static bool is_master(const char* progname) {
   if (progname != NULL) {
      if ( !strcmp(prognames[QMASTER],progname)) {
         return true;
      } 
   }
   return false;
}
#endif

static cl_bool_t ssl_cert_verify_func(cl_ssl_verify_mode_t mode, cl_bool_t service_mode, const char* value) {

   /*
    *   CR:
    *
    * - This callback function can be used to make additonal security checks 
    * 
    * - this callback is not called from commlib with a value == NULL 
    * 
    * - NOTE: This callback is called from the commlib. If the commlib is initalized with
    *   thread support (see cl_com_setup_commlib() ) this may be a problem because the thread has
    *   no application specific context initalization. So never call functions within this callback 
    *   which need thread specific setup.
    */
   DENTER(TOP_LAYER, "ssl_cert_verify_func");

   DPRINTF(("ssl_cert_verify_func()\n"));

   if (value == NULL) {
      /* This should never happen */
      CRITICAL((SGE_EVENT, MSG_SEC_CERT_VERIFY_FUNC_NO_VAL));
      DEXIT;
      return CL_FALSE;
   }

   if (service_mode == CL_TRUE) {
      switch(mode) {
         case CL_SSL_PEER_NAME: {
            DPRINTF(("local service got certificate from peer \"%s\"\n", value));
#if 0
            if (strcmp(value,"SGE admin user") != 0) {
               DEXIT;
               return CL_FALSE;
            }
#endif
            break;
         }
         case CL_SSL_USER_NAME: {
            DPRINTF(("local service got certificate from user \"%s\"\n", value));
#if 0
            if (strcmp(value,"") != 0) {
               DEXIT;
               return CL_FALSE;
            }
#endif
            break;
         }
      }
   } else {
      switch(mode) {
         case CL_SSL_PEER_NAME: {
            DPRINTF(("local client got certificate from peer \"%s\"\n", value));
#if 0
            if (strcmp(value,"SGE admin user") != 0) {
               DEXIT;
               return CL_FALSE;
            }
#endif
            break;
         }
         case CL_SSL_USER_NAME: {
            DPRINTF(("local client got certificate from user \"%s\"\n", value));
#if 0
            if (strcmp(value,"") != 0) {
               DEXIT;
               return CL_FALSE;
            }
#endif
            break;
         }
      }
   }
   DEXIT;
   return CL_TRUE;
}


sge_csp_path_class_t *sge_csp_path_class_create(sge_env_state_class_t *sge_env, sge_prog_state_class_t *sge_prog, sge_error_class_t *eh)
{
   sge_csp_path_class_t *ret = (sge_csp_path_class_t *)sge_malloc(sizeof(sge_csp_path_class_t));

   DENTER(TOP_LAYER, "sge_csp_path_class_create");

   if (!ret) {
      eh->error(eh, STATUS_EMALLOC, ANSWER_QUALITY_ERROR, MSG_MEMORY_MALLOCFAILED);
      DEXIT;
      return NULL;
   }   
   
   ret->dprintf = sge_csp_path_dprintf;
   
   ret->get_CA_cert_file = get_CA_cert_file;
   ret->get_CA_key_file = get_CA_key_file;
   ret->get_cert_file = get_cert_file;
   ret->get_key_file = get_key_file;
   ret->get_rand_file = get_rand_file;
   ret->get_reconnect_file = get_reconnect_file;
   ret->get_refresh_time = get_refresh_time;
   ret->get_password = get_password;
   ret->get_verify_func = get_verify_func;

   ret->set_CA_cert_file = set_CA_cert_file;
   ret->set_CA_key_file = set_CA_key_file;
   ret->set_cert_file = set_cert_file;
   ret->set_key_file = set_key_file;
   ret->set_rand_file = set_rand_file;
   ret->set_reconnect_file = set_reconnect_file;
   ret->set_refresh_time = set_refresh_time;
   ret->set_password = set_password;
   ret->set_verify_func = set_verify_func;

   ret->sge_csp_path_handle = (sge_csp_path_t*)sge_malloc(sizeof(sge_csp_path_t));
   if (ret->sge_csp_path_handle == NULL) {
      eh->error(eh, STATUS_EMALLOC, ANSWER_QUALITY_ERROR, MSG_MEMORY_MALLOCFAILED);
      sge_csp_path_class_destroy(&ret);
      DEXIT;
      return NULL;
   }
   memset(ret->sge_csp_path_handle, 0, sizeof(sge_csp_path_t));

   if (!sge_csp_path_setup(ret, sge_env, sge_prog, eh)) {
      sge_csp_path_class_destroy(&ret);
      DEXIT;
      return NULL;
   }

   DEXIT;
   return ret;
}   

void sge_csp_path_class_destroy(sge_csp_path_class_t **pst)
{
   DENTER(TOP_LAYER, "sge_csp_path_class_destroy");

   if (!pst || !*pst) {
      DEXIT;
      return;
   }   
   sge_csp_path_destroy((*pst)->sge_csp_path_handle);
   FREE(*pst);
   *pst = NULL;
   DEXIT;
}

static bool sge_csp_path_setup(sge_csp_path_class_t *thiz, sge_env_state_class_t *sge_env, sge_prog_state_class_t *sge_prog, sge_error_class_t *eh)
{
   char buffer[2*1024];
   dstring bw;
   const char* sge_root = NULL;
   const char* sge_cell = NULL;
   const char* ca_root = NULL;
   const char* ca_local_root = NULL;
   const char* user_dir = NULL;
   const char* user_local_dir = NULL;
   const char* username = NULL;
   const char* progname = NULL;
   const char* sge_cakeyfile = NULL;
   const char* sge_certfile = NULL;
   const char* sge_keyfile = NULL;
   int sge_qmaster_port = 0;
/*    bool sge_no_ca_local_root = false;  */

   DENTER(TOP_LAYER, "sge_csp_path_setup");
 
   if (!sge_env) {
      eh->error(eh, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, "sge_env is NULL");
      DEXIT;
      return FALSE;
   }
   
   /* get the necessary info to build the paths */
   sge_root = sge_env->get_sge_root(sge_env);
   sge_cell = sge_env->get_sge_cell(sge_env);
   sge_qmaster_port = sge_env->get_sge_qmaster_port(sge_env);
   
   progname = sge_prog->get_sge_formal_prog_name(sge_prog);
   username = sge_prog->get_user_name(sge_prog);
   
   DTRACE;

#if 0
   TODO: currently not supported by sge_ca script
   if (getenv("SGE_NO_CA_LOCAL_ROOT")) {
      sge_no_ca_local_root = true;
   }   
#endif   
   /*
   ** TODO: certificate handling does not work since usually the servlet runs as
   **       a specific user and has no access to the users key
   **       a different mechanism must be delivered to get access to the users key
   **       setuid wrapper for the file access or different storage for the key ???
   */
   sge_dstring_init(&bw, buffer, sizeof(buffer)); 

   DTRACE;
   sge_dstring_sprintf(&bw, "%s/%s/%s", sge_root, sge_cell, CA_DIR);
   ca_root = strdup(sge_dstring_get_string(&bw));

   DTRACE;
   if (sge_qmaster_port) {
      sge_dstring_sprintf(&bw, "%s/port%d/%s", CA_LOCAL_DIR, sge_qmaster_port, sge_cell);   
   } else {
      sge_dstring_sprintf(&bw, "%s/%s/%s", CA_LOCAL_DIR, SGE_COMMD_SERVICE, sge_cell);   
   }
   DTRACE;
   ca_local_root = strdup(sge_dstring_get_string(&bw));

   /*
   ** determine user_dir: 
   ** - ca_root, ca_local_root for daemons 
   ** - $HOME/.sge/{port$COMMD_PORT|SGE_COMMD_SERVICE}/$SGE_CELL
   **   and as fallback
   **   /var/sgeCA/{port$COMMD_PORT|SGE_COMMD_SERVICE}/$SGE_CELL/userkeys/$USER/{cert.pem,key.pem}
   */
   if (is_daemon(progname)) {
   DTRACE;
      user_dir = ca_root;
      user_local_dir = ca_local_root;
   } else {
      struct passwd *pw;
      struct passwd pw_struct;
      char buffer[2048];

   DTRACE;
      pw = sge_getpwnam_r(username, &pw_struct, buffer, sizeof(buffer));

      if (!pw) {   
         eh->error(eh, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_SEC_USERNOTFOUND_S, username);
         DEXIT;
         return FALSE;
      }
   DTRACE;
      if (sge_qmaster_port) {                     
         sge_dstring_sprintf(&bw, "%s/%s/port%d/%s", pw->pw_dir, SGESecPath, sge_qmaster_port, sge_cell);   
      } else {         
         sge_dstring_sprintf(&bw, "%s/%s/%s/%s", pw->pw_dir, SGESecPath, SGE_COMMD_SERVICE, sge_cell);   
      }            
      user_dir = strdup(sge_dstring_get_string(&bw));
      user_local_dir = user_dir;
   }

   DTRACE;
   sge_dstring_sprintf(&bw, "%s/%s", ca_root, CaCert);   
   thiz->set_CA_cert_file(thiz, sge_dstring_get_string(&bw));

   DTRACE;
   if ((sge_cakeyfile=getenv("SGE_CAKEYFILE"))) {
      thiz->set_CA_key_file(thiz, sge_cakeyfile);
   } else {
      sge_dstring_sprintf(&bw, "%s/private/%s", ca_local_root, CaKey);
      thiz->set_CA_key_file(thiz, sge_dstring_get_string(&bw));
   }

   DTRACE;
   if ((sge_certfile = getenv("SGE_CERTFILE"))) {
      thiz->set_cert_file(thiz, sge_certfile);
   } else {   
      if (is_daemon(progname)) {
         sge_dstring_sprintf(&bw, "%s/certs/%s", user_dir, UserCert);
      } else {
         sge_dstring_sprintf(&bw, "%s/userkeys/%s/%s", ca_local_root, username, UserCert);
      }
      thiz->set_cert_file(thiz, sge_dstring_get_string(&bw));
   }
    
   DTRACE;
   if ((sge_keyfile = getenv("SGE_KEYFILE"))) {
      thiz->set_key_file(thiz, sge_keyfile); 
   } else {   
      if (is_daemon(progname)) {
         sge_dstring_sprintf(&bw, "%s/private/%s", user_local_dir, UserKey);   
      } else {
         sge_dstring_sprintf(&bw, "%s/userkeys/%s/%s", ca_local_root, username, UserKey);
      }   
      thiz->set_key_file(thiz, sge_dstring_get_string(&bw));
   }   


   DTRACE;
   sge_dstring_sprintf(&bw, "%s/%s", user_dir, RandFile);
   thiz->set_rand_file(thiz, sge_dstring_get_string(&bw));

   DTRACE;
   sge_dstring_sprintf(&bw, "%s/%s", user_dir, ReconnectFile);
   thiz->set_reconnect_file(thiz, sge_dstring_get_string(&bw));

   DTRACE;
   thiz->set_password(thiz, NULL);
   thiz->set_refresh_time(thiz, 60*VALID_MINUTES);

   DTRACE;
   thiz->set_verify_func(thiz, ssl_cert_verify_func);

   DTRACE;
   FREE(ca_root);
   FREE(ca_local_root);
   FREE(user_dir);

   DTRACE;
   thiz->dprintf(thiz);

   DEXIT;
   return TRUE;
}

static void sge_csp_path_destroy(void *theState)
{
   sge_csp_path_t *s = (sge_csp_path_t *)theState;

   DENTER(TOP_LAYER, "sge_csp_path_destroy");

   FREE(s->CA_cert_file);
   FREE(s->CA_key_file);
   FREE(s->cert_file);
   FREE(s->key_file);
   FREE(s->rand_file);
   FREE(s->reconnect_file);
   FREE(s->password);
   sge_free((char*)s);

   DEXIT;
}

static void sge_csp_path_dprintf(sge_csp_path_class_t *thiz)
{
   sge_csp_path_t *es = (sge_csp_path_t *)thiz->sge_csp_path_handle;

   DENTER(TOP_LAYER, "sge_csp_path_dprintf");

   DPRINTF(("CA_cert_file        >%s<\n", es->CA_cert_file ? es->CA_cert_file : "NA"));
   DPRINTF(("CA_key_file         >%s<\n", es->CA_key_file ? es->CA_key_file : "NA"));
   DPRINTF(("cert_file           >%s<\n", es->cert_file ? es->cert_file : "NA"));
   DPRINTF(("key_file            >%s<\n", es->key_file ? es->key_file : "NA"));
   DPRINTF(("rand_file           >%s<\n", es->rand_file ? es->rand_file : "NA"));
   DPRINTF(("reconnect_file      >%s<\n", es->reconnect_file ? es->reconnect_file : "NA"));
   DPRINTF(("refresh_time        >%d<\n", es->refresh_time));
   DPRINTF(("password            >%s<\n", es->password ? es->password : "NA"));

   DEXIT;
}

static const char* get_CA_cert_file(sge_csp_path_class_t *thiz) 
{
   sge_csp_path_t *es = (sge_csp_path_t *) thiz->sge_csp_path_handle;
   return es->CA_cert_file;
}

static const char* get_CA_key_file(sge_csp_path_class_t *thiz) 
{
   sge_csp_path_t *es = (sge_csp_path_t *) thiz->sge_csp_path_handle;
   return es->CA_key_file;
}

static const char* get_cert_file(sge_csp_path_class_t *thiz) 
{
   sge_csp_path_t *es = (sge_csp_path_t *) thiz->sge_csp_path_handle;
   return es->cert_file;
}

static const char* get_key_file(sge_csp_path_class_t *thiz) 
{
   sge_csp_path_t *es = (sge_csp_path_t *) thiz->sge_csp_path_handle;
   return es->key_file;
}

static const char* get_rand_file(sge_csp_path_class_t *thiz) 
{
   sge_csp_path_t *es = (sge_csp_path_t *) thiz->sge_csp_path_handle;
   return es->rand_file;
}

static const char* get_reconnect_file(sge_csp_path_class_t *thiz) 
{
   sge_csp_path_t *es = (sge_csp_path_t *) thiz->sge_csp_path_handle;
   return es->reconnect_file;
}

static const char* get_password(sge_csp_path_class_t *thiz) 
{
   sge_csp_path_t *es = (sge_csp_path_t *) thiz->sge_csp_path_handle;
   return es->password;
}

static int get_refresh_time(sge_csp_path_class_t *thiz)
{
   sge_csp_path_t *es = (sge_csp_path_t *) thiz->sge_csp_path_handle;
   return es->refresh_time;
}

static cl_ssl_verify_func_t get_verify_func(sge_csp_path_class_t *thiz)
{
   sge_csp_path_t *es = (sge_csp_path_t *) thiz->sge_csp_path_handle;
   return es->verify_func;
}

static void set_CA_cert_file(sge_csp_path_class_t *thiz, const char *CA_cert_file)
{
   sge_csp_path_t *es = (sge_csp_path_t *) thiz->sge_csp_path_handle;
   es->CA_cert_file = sge_strdup(es->CA_cert_file, CA_cert_file);
}

static void set_CA_key_file(sge_csp_path_class_t *thiz, const char *CA_key_file)
{
   sge_csp_path_t *es = (sge_csp_path_t *) thiz->sge_csp_path_handle;
   es->CA_key_file = sge_strdup(es->CA_key_file, CA_key_file);
}

static void set_cert_file(sge_csp_path_class_t *thiz, const char *cert_file)
{
   sge_csp_path_t *es = (sge_csp_path_t *) thiz->sge_csp_path_handle;
   es->cert_file = sge_strdup(es->cert_file, cert_file);
}

static void set_key_file(sge_csp_path_class_t *thiz, const char *key_file)
{
   sge_csp_path_t *es = (sge_csp_path_t *) thiz->sge_csp_path_handle;
   es->key_file = sge_strdup(es->key_file, key_file);
}

static void set_rand_file(sge_csp_path_class_t *thiz, const char *rand_file)
{
   sge_csp_path_t *es = (sge_csp_path_t *) thiz->sge_csp_path_handle;
   es->rand_file = sge_strdup(es->rand_file, rand_file);
}

static void set_reconnect_file(sge_csp_path_class_t *thiz, const char *reconnect_file)
{
   sge_csp_path_t *es = (sge_csp_path_t *) thiz->sge_csp_path_handle;
   es->reconnect_file = sge_strdup(es->reconnect_file, reconnect_file);
}

static void set_password(sge_csp_path_class_t *thiz, const char *password)
{
   sge_csp_path_t *es = (sge_csp_path_t *) thiz->sge_csp_path_handle;
   es->password = sge_strdup(es->password, password);
}

static void set_refresh_time(sge_csp_path_class_t *thiz, u_long32 refresh_time)
{
   sge_csp_path_t *es = (sge_csp_path_t *) thiz->sge_csp_path_handle;
   es->refresh_time = refresh_time; 
}

static void set_verify_func(sge_csp_path_class_t *thiz, cl_ssl_verify_func_t verify_func)
{
   sge_csp_path_t *es = (sge_csp_path_t *) thiz->sge_csp_path_handle;
   es->verify_func = verify_func; 
}

