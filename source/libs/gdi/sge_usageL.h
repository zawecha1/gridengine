#ifndef __SGE_USAGEL_H
#define __SGE_USAGEL_H

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

#include "sge_boundaries.h"
#include "cull.h"

#ifdef  __cplusplus
extern "C" {
#endif

/* *INDENT-ON* */

/*
 * sge standard usage value names
 *
 * use these defined names for refering special usage values
 */

#define USAGE_ATTR_CPU      "cpu"

/* integral memory usage */
#define USAGE_ATTR_MEM      "mem"
#define USAGE_ATTR_IO       "io"
#define USAGE_ATTR_IOW      "iow"

#define USAGE_ATTR_CPU_ACCT "acct_cpu"

/* integral memory usage */
#define USAGE_ATTR_MEM_ACCT "acct_mem"
#define USAGE_ATTR_IO_ACCT  "acct_io"
#define USAGE_ATTR_IOW_ACCT "acct_iow"

/* actual amount of used memory */
#define USAGE_ATTR_VMEM     "vmem"

/* max. vmem */
#define USAGE_ATTR_HIMEM    "himem"


/* 
 * This is the list type we use to hold the 
 * usage information generated by the DC.
 */

enum {
   UA_name = UA_LOWERBOUND,
   UA_value                  /* usage value */
};

SLISTDEF(UA_Type, Usage)
   SGE_STRINGHU(UA_name)
   SGE_DOUBLE(UA_value)       /* 960710 SVD - changed from to */
LISTEND 

NAMEDEF(UAN)
   NAME("UA_name")
   NAME("UA_value")
NAMEEND

/* *INDENT-OFF* */ 

#define UAS sizeof(UAN)/sizeof(char*)
#ifdef  __cplusplus
}
#endif
#endif                          /* __SGE_USAGEL_H */
