
#!/usr/local/bin/tclsh
# expect script 
#___INFO__MARK_BEGIN__
##########################################################################
#
#  The Contents of this file are made available subject to the terms of
#  the Sun Industry Standards Source License Version 1.2
#
#  Sun Microsystems Inc., March, 2001
#
#
#  Sun Industry Standards Source License Version 1.2
#  =================================================
#  The contents of this file are subject to the Sun Industry Standards
#  Source License Version 1.2 (the "License"); You may not use this file
#  except in compliance with the License. You may obtain a copy of the
#  License at http://gridengine.sunsource.net/Gridengine_SISSL_license.html
#
#  Software provided under this License is provided on an "AS IS" basis,
#  WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
#  WITHOUT LIMITATION, WARRANTIES THAT THE SOFTWARE IS FREE OF DEFECTS,
#  MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE, OR NON-INFRINGING.
#  See the License for the specific provisions governing your rights and
#  obligations concerning the Software.
#
#  The Initial Developer of the Original Code is: Sun Microsystems, Inc.
#
#  Copyright: 2001 by Sun Microsystems, Inc.
#
#  All Rights Reserved.
#
##########################################################################
#___INFO__MARK_END__


#****** sge_ckpt/assign_queues_with_ckpt_object() ************************
#  NAME
#     assign_queues_with_ckpt_object() -- setup queue <-> ckpt connection
#
#  SYNOPSIS
#     assign_queues_with_ckpt_object { queue_list ckpt_obj }
#
#  FUNCTION
#     This procedure will setup the queue - ckpt connections.
#
#  INPUTS
#     queue_list - queue list for the ckpt object
#     ckpt_obj   - name of ckpt object
#
#  SEE ALSO
#     sge_procedures/assign_queues_with_pe_object()
#*******************************************************************************
#****** sge_ckpt/get_checkpointobj() *************************************
#  NAME
#     get_checkpointobj() -- get checkpoint configuration information
#
#  SYNOPSIS
#     get_checkpointobj { ckpt_obj change_array } 
#
#  FUNCTION
#     Get the actual configuration settings for the named checkpoint object
#
#  INPUTS
#     ckpt_obj     - name of the checkpoint object
#     change_array - name of an array variable that will get set by 
#                    get_checkpointobj
#
#  SEE ALSO
#     sge_ckpt/set_checkpointobj()
#     sge_procedures/get_queue() 
#     sge_procedures/set_queue()
#*******************************************************************************
proc get_checkpointobj { ckpt_obj change_array } {
  global ts_config
  global CHECK_ARCH CHECK_OUTPUT
  upvar $change_array chgar

  set catch_result [ catch {  eval exec "$ts_config(product_root)/bin/$CHECK_ARCH/qconf" "-sckpt" "$ckpt_obj"} result ]
  if { $catch_result != 0 } {
     add_proc_error "get_checkpointobj" "-1" "qconf error or binary not found ($ts_config(product_root)/bin/$CHECK_ARCH/qconf)\n$result"
     return
  } 

  # split each line as list element
  set help [split $result "\n"]
  foreach elem $help {
     set id [lindex $elem 0]
     set value [lrange $elem 1 end]
     if { [string compare $value ""] != 0 } {
       set chgar($id) $value
     }
  }
}

#****** sge_ckpt/set_checkpointobj() *************************************
#  NAME
#     set_checkpointobj() -- set or change checkpoint object configuration
#
#  SYNOPSIS
#     set_checkpointobj { ckpt_obj change_array }
#
#  FUNCTION
#     Set a checkpoint configuration corresponding to the content of the
#     change_array.
#
#  INPUTS
#     ckpt_obj     - name of the checkpoint object to configure
#     change_array - name of array variable that will be set by
#                    set_checkpointobj()
#
#  RESULT
#     0  : ok
#     -1 : timeout
#
#  SEE ALSO
#     sge_ckpt/get_checkpointobj()
#*******************************************************************************
proc set_checkpointobj { ckpt_obj change_array } {
 global ts_config
   global CHECK_ARCH open_spawn_buffer
   global CHECK_USER CHECK_OUTPUT

   upvar $change_array chgar

   validate_checkpointobj chgar

   set vi_commands [build_vi_command chgar]

   set args "-mckpt $ckpt_obj"

   set ALREADY_EXISTS [ translate $ts_config(master_host) 1 0 0 [sge_macro MSG_SGETEXT_ALREADYEXISTS_SS] "*" $ckpt_obj]
   set MODIFIED [translate $ts_config(master_host) 1 0 0 [sge_macro MSG_SGETEXT_MODIFIEDINLIST_SSSS] $CHECK_USER "*" $ckpt_obj "checkpoint interface" ]

   if { $ts_config(gridengine_version) == 53 } {
      set REFERENCED_IN_QUEUE_LIST_OF_CHECKPOINT [translate $ts_config(master_host) 1 0 0 [sge_macro MSG_SGETEXT_UNKNOWNQUEUE_SSSS] "*" "*" "*" "*"]

      set result [ handle_vi_edit "$ts_config(product_root)/bin/$CHECK_ARCH/qconf" $args $vi_commands $MODIFIED $ALREADY_EXISTS $REFERENCED_IN_QUEUE_LIST_OF_CHECKPOINT ]
  } else {
      set result [ handle_vi_edit "$ts_config(product_root)/bin/$CHECK_ARCH/qconf" $args $vi_commands $MODIFIED $ALREADY_EXISTS ]
  }

   if { $result == -1 } { add_proc_error "add_checkpointobj" -1 "timeout error" }
   if { $result == -2 } { add_proc_error "add_checkpointobj" -1 "already exists" }
   if { $result == -3 } { add_proc_error "add_checkpointobj" -1 "queue reference does not exist" }
   if { $result != 0  } { add_proc_error "add_checkpointobj" -1 "could nod modify checkpoint object" }

   return $result
}

#                                                             max. column:     |
#****** sge_procedures/del_checkpointobj() ******
#
#  NAME
#     del_checkpointobj -- delete checkpoint object definition
#
#  SYNOPSIS
#     del_checkpointobj { checkpoint_name }
#
#  FUNCTION
#     This procedure will delete a checkpoint object definition by its name.
#
#  INPUTS
#     checkpoint_name - name of the checkpoint object
#
#  RESULT
#      0  - ok
#     -1  - timeout error
#
#  SEE ALSO
#     sge_procedures/add_checkpointobj()
#*******************************
proc del_checkpointobj { checkpoint_name } {
   global ts_config
   global CHECK_ARCH open_spawn_buffer CHECK_CORE_MASTER CHECK_USER CHECK_HOST
   global CHECK_OUTPUT

   unassign_queues_with_ckpt_object $checkpoint_name

  set REMOVED [translate $CHECK_CORE_MASTER 1 0 0 [sge_macro MSG_SGETEXT_REMOVEDFROMLIST_SSSS] $CHECK_USER "*" $checkpoint_name "*" ]

  log_user 0
  set id [ open_remote_spawn_process $CHECK_HOST $CHECK_USER "$ts_config(product_root)/bin/$CHECK_ARCH/qconf" "-dckpt $checkpoint_name"  ]
  set sp_id [ lindex $id 1 ]
  set timeout 30
  set result -1

  log_user 0

  expect {
    -i $sp_id full_buffer {
      set result -1
      add_proc_error "del_checkpointobj" "-1" "buffer overflow please increment CHECK_EXPECT_MATCH_MAX_BUFFER value"
    }
    -i $sp_id $REMOVED {
      set result 0
    }
    -i $sp_id "removed" {
      set result 0
    }

    -i $sp_id default {
      set result -1
    }

  }
  close_spawn_process $id
  log_user 1

  if { $result != 0 } {
     add_proc_error "del_checkpointobj" -1 "could not delete checkpoint object $checkpoint_name"
  }

  return $result
}
