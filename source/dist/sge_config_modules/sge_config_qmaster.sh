#! /bin/sh 
#
# SGE/SGEEE configuration script (Installation/Uninstallation/Upgrade/Downgrade)
# Scriptname: sge_config_qmaster.sh
# Module: qmaster installation functions
#
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

#set -x


#-------------------------------------------------------------------------
# GetCellRoot
#
GetCellRoot()
{
   if [ $AUTO = true ]; then
    SGE_CELL_ROOT=$SGE_CELL_ROOT
    $INFOTEXT -log "Using >%s< as SGE_CELL_ROOT." "$SGE_CELL_ROOT"
   else
   $CLEAR
   $INFOTEXT -u "\nGrid Engine cell root"
   $INFOTEXT -n "Enter the cell root <<<"
   INP=`Enter `
   eval SGE_CELL_ROOT=$INP
   $INFOTEXT -wait -auto $AUTO -n "\nUsing cell root >%s<. Hit <RETURN> to continue >> " $SGE_CELL_ROOT
   $CLEAR
   fi
}



#-------------------------------------------------------------------------
# GetCell
#
GetCell()
{
   if [ $AUTO = true ]; then
    SGE_CELL=$CELL_NAME
    SGE_CELL_VAL=$CELL_NAME
    $INFOTEXT -log "Using >%s< as CELL_NAME." "$CELL_NAME"
   else
   $CLEAR
   $INFOTEXT -u "\nGrid Engine cells"
   $INFOTEXT -n "\nGrid Engine supports multiple cells.\n\n" \
                "If you are not planning to run multiple Grid Engine clusters or if you don't\n" \
                "know yet what is a Grid Engine cell it is safe to keep the default cell name\n\n" \
                "   default\n\n" \
                "If you want to install multiple cells you can enter a cell name now.\n\n" \
                "The environment variable\n\n" \
                "   \$SGE_CELL=<your_cell_name>\n\n" \
                "will be set for all further Grid Engine commands.\n\n" \
                "Enter cell name or hit <RETURN> to use default cell >default< >> "
   INP=`Enter default`
   eval SGE_CELL=$INP
   SGE_CELL_VAL=`eval echo $SGE_CELL`
   $INFOTEXT -wait -auto $AUTO -n "\nUsing cell >%s<. Hit <RETURN> to continue >> " $SGE_CELL_VAL
   $CLEAR
   fi
   export SGE_CELL
}


#-------------------------------------------------------------------------
# GetQmasterSpoolDir()
#
GetQmasterSpoolDir()
{
   if [ $AUTO = true ]; then
      QMDIR=$QMASTER_SPOOL_DIR
      $INFOTEXT -log "Using >%s< as QMASTER_SPOOL_DIR." "$QMDIR"
   else
   euid=$1

   done=false
   while [ $done = false ]; do
      $CLEAR
      $INFOTEXT -u "\nGrid Engine qmaster spool directory"
      $INFOTEXT "\nThe qmaster spool directory is the place where the qmaster daemon stores\n" \
                "the configuration and the state of the queuing system.\n\n"

      if [ $euid = 0 ]; then
         if [ $ADMINUSER = default ]; then
            $INFOTEXT "User >root< on this host must have read/write accessto the qmaster\n" \
                      "spool directory.\n"
         else
            $INFOTEXT "The admin user >%s< must have read/write access\n" \
                      "to the qmaster spool directory.\n" $ADMINUSER
         fi
      else
         $INFOTEXT "Your account on this host must have read/write access\n" \
                   "to the qmaster spool directory.\n"
      fi

      $INFOTEXT -n "If you will install shadow master hosts or if you want to be able to start\n" \
                   "the qmaster daemon on other hosts (see the corresponding sectionin the\n" \
                   "Grid Engine Installation and Administration Manual for details) the account\n" \
                   "on the shadow master hosts also needs read/write access to this directory.\n\n" \
                   "Enter spool directory or hit <RETURN> to use default\n" \
                   "[%s] >> " \
                   $SGE_ROOT_VAL/$SGE_CELL_VAL/spool/qmaster

      QMDIR=`Enter $SGE_ROOT_VAL/$SGE_CELL_VAL/spool/qmaster`

      $INFOTEXT "\nThe following directory has been selected as qmaster spool directory:\n\n" \
                "   %s\n" $QMDIR

      $INFOTEXT -auto $AUTO -ask "y" "n" -def "n" -n \
                "Do you want to select another qmaster spool directory (y/n) [n] >> "

      if [ $? = 1 ]; then
         done=true
      fi
   done
   fi
   export QMDIR
}



#--------------------------------------------------------------------------
# SetPermissions
#    - set permission for regular files to 644
#    - set permission for executables and directories to 755
#
SetPermissions()
{
   $CLEAR
   $INFOTEXT -u "\nVerifying and setting file permissions"
   $ECHO

   euid=`$SGE_UTILBIN/uidgid -euid`

   if [ $euid != 0 ]; then
      $INFOTEXT "You are not installing as user >root<\n"
      $INFOTEXT "Can't set the file owner/group and permissions\n"
      $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> to continue >> "
      $CLEAR
      return 0
   else
      $INFOTEXT -auto $AUTO -ask "y" "n" -def "y" -n \
                "Did you install this version with >pkgadd< or did you already\n" \
                "verify and set the file permissions of your distribution (y/n) [y] >> "
      if [ $? = 0 ]; then
         $INFOTEXT -wait -auto $AUTO -n "We do not verify file permissions. Hit <RETURN> to continue >> "
         $CLEAR
         return 0
      fi
   fi

   rm -f ./tst$$ 2> /dev/null > /dev/null
   touch ./tst$$ 2> /dev/null > /dev/null
   ret=$?
   rm -f ./tst$$ 2> /dev/null > /dev/null
   if [ $ret != 0 ]; then
      $INFOTEXT -u "\nVerifying and setting file permissions (continued)"

      $INFOTEXT "\nWe can't set file permissions on this machine, because user root\n" \
                  "has not the necessary privileges to change file permissions\n" \
                  "on this file system.\n" \
                  "Probably this file system is an NFS mount where user root is\n" \
                  "mapped to user >nobody<.\n" \
                  "Please login now at your file server and set the file permissions and\n" \
                  "ownership of the entire distribution with the command:\n" \
                  "   # \$SGE_ROOT/util/setfileperm.sh <adminuser> <admingroup> \$SGE_ROOT\n\n" \
                  "where <adminuser> and <admingroup> are the Unix user/group names under which\n" \
                  "the files should be installed and created.\n\n"

      $INFOTEXT -wait -auto $AUTO -n "Please hit <RETURN> to continue once you set your file permissions >> "
      $CLEAR
      return 0
   elif [ $fast = false ]; then
      $CLEAR
      $INFOTEXT -u "\nVerifying and setting file permissions"
      $INFOTEXT "\nWe may now verify and set the file permissions of your Grid Engine\n" \
                "distribution.\n\n" \
                 "This may be useful since due to unpacking and copying of your distribution\n" \
                 "your files may be unaccessible to other users.\n\n" \
                 "We will set the permissions of directories and binaries to\n\n" \
                 "   755 - that means executable are accessible for the world\n\n" \
                 "and for ordinary files to\n\n" \
                 "   644 - that means readable for the world\n\n"

      $INFOTEXT -auto $AUTO -ask "y" "n" -def "y" -n \
               "Do you want to verify and set your file permissions (y/n) [y] >> "
      ret=$?
   else
      ret=0
   fi

   if [ $ret = 0 ]; then

      if [ $resport = true ]; then
         resportarg="-resport"
      else
         resportarg="-noresport"
      fi

      if [ $ADMINUSER = default ]; then
         fileowner=root
      else
         fileowner=$ADMINUSER
      fi

      filegid=`$SGE_UTILBIN/uidgid -gid`

      $CLEAR

      util/setfileperm.sh -auto $resportarg $fileowner $filegid $SGE_ROOT

      $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> to continue >> "
   else
      $INFOTEXT -wait -auto $AUTO -n "We will not verify your file permissions. Hit <RETURN> to continue >>"
   fi
   $CLEAR
}


#--------------------------------------------------------------------------
# SetSpoolingOptions sets / queries options for the spooling framework
#
SetSpoolingOptions()
{
   $INFOTEXT -u "\nSelect spooling method"
   SPOOLING_METHOD=`ExecuteAsAdmin $SPOOLINIT method`
   $INFOTEXT -log "Setting spooling method to %s" $SPOOLING_METHOD
   case $SPOOLING_METHOD in 
      classic)
         SPOOLING_LIB=none
         SPOOLING_ARGS="$SGE_ROOT_VAL/$COMMONDIR;$QMDIR"
         ;;
      berkeleydb)
         SPOOLING_LIB=none
         SPOOLING_SERVER=none
         SPOOLING_DIR="$QMDIR/spooldb"
         params_ok=0
         if [ $AUTO = "true" ]; then
            SPOOLING_SERVER=$DB_SPOOLING_SERVER
            SPOOLING_DIR="$QMDIR/$DB_SPOOLING_DIR"
            SpoolingCheckParams
            params_ok=1
            #TODO: exec rcrpc script
         fi 
         while [ $params_ok -eq 0 ]; do
            SpoolingQueryChange
            SpoolingCheckParams
            params_ok=$?
         done

         if [ $SPOOLING_SERVER = "none" ]; then
            Makedir $SPOOLING_DIR
            SPOOLING_ARGS="$SPOOLING_DIR"
         else
            SPOOLING_ARGS="$SPOOLING_SERVER:`basename $SPOOLING_DIR`"
         fi

         ;;
      *)
         $INFOTEXT -e "\nUnknown spooling method. Exit."
         exit 1
         ;;
   esac
}

SpoolingQueryChange()
{
   $INFOTEXT -u "\nBerkeley Database spooling parameters"
   $INFOTEXT "\nWe can use local spooling, which will go to a local filesystem" \
             "\nor use a RPC Client/Server mechanism. In this case, qmaster will" \
             "\ncontact a RPC server running on a separate server machine." \
             "\nIf you want to use the SGE shadowd, you have to use the " \
             "\nRPC Client/Server mechanism.\n"
   $INFOTEXT "\nEnter database server (none for local spooling)\n" \
             "or hit <RETURN> to use default [%s] >> " $SPOOLING_SERVER
             SPOOLING_SERVER=`Enter $SPOOLING_SERVER`

   $INFOTEXT "\nEnter the database directory\n" \
             "or hit <RETURN> to use default [%s] >> " $SPOOLING_DIR
             SPOOLING_DIR=`Enter $SPOOLING_DIR`
}


SpoolingCheckParams()
{
   # if we use local spooling, check if the database directory is on local fs
   if [ $SPOOLING_SERVER = "none" ]; then
      CheckLocalFilesystem $SPOOLING_DIR
      ret=$?
      if [ $ret -eq 0 ]; then
      $INFOTEXT -e "\nThe database directory >%s<\n" \
                   "is not on a local filesystem.\nPlease choose a local filesystem or configure the RPC Client/Server mechanism" $SPOOLING_DIR
         return 0
      fi
   else 
      # TODO: we should check if the hostname can be resolved
      # create a script to start the rpc server
      
      CreateRPCServerScript
      $INFOTEXT "Now we have to startup the rc script >%s< on the RPC server machine\n" $COMMONDIR/sgebdb
      $INFOTEXT -auto $AUTO -ask "y" "n" -def "y" -n "Shall the installtion script try to start the RPC server? (y/n) [y] >>"

      if [ $? = 0 -o $AUTO = "true" -a $START_RPC_SERVICE = "true" ]; then
         $INFOTEXT -log "Starting rpc server on host %s!" $SPOOLING_SERVER
         $INFOTEXT "Starting rpc server on host %s!" $SPOOLING_SERVER
         echo "cd $SGE_ROOT/$COMMONDIR && ./sgebdb start &" | rsh $SPOOLING_SERVER /bin/sh &
         sleep 5
      else
         $INFOTEXT "Please start the rc script >%s< on the RPC server machine\n" $COMMONDIR/sgebdb       
      fi

   fi

   return 1
}

CreateRPCServerScript()
{
   RPCSCRIPT=$COMMONDIR/sgebdb
   Execute sed -e "s%GENROOT%${SGE_ROOT_VAL}%g" \
               -e "s%GENCELL%${SGE_CELL_VAL}%g" \
               -e "s%SPOOLING_DIR%${SPOOLING_DIR}%g" \
               -e "/#+-#+-#+-#-/,/#-#-#-#-#-#/d" \
               util/rpc_startup_template > ${RPCSCRIPT}
   Execute $CHMOD a+x $RPCSCRIPT

}


CheckLocalFilesystem()
{  
   FS=`dirname $1`
   case $ARCH in
      solaris*)
         df -l $FS >/dev/null 2>&1
         if [ $? -eq 0 ]; then
            return 1
         else
            return 0
         fi
         ;;
      *linux)
         df -l $FS | grep $FS >/dev/null 2>&1
         if [ $? -eq 0 ]; then
            return 1
         else
            return 0
         fi
         ;;
      *)
         $INFOTEXT -e "\nDon't know how to test for local filesystem. Exit."
         exit 1
         ;;
   esac

   return 0
}


#-------------------------------------------------------------------------
# Ask the installer for the hostname resolving method
# (IGNORE_FQND=true/false)
#
SelectHostNameResolving()
{
   if [ $AUTO = true ]; then
     IGNORE_FQDN_DEFAULT=$HOSTNAME_RESOLVING
     $INFOTEXT -log "Using >%s< as IGNORE_FQDN_DEFAULT." "$IGNORE_FQDN_DEFAULT"
     $INFOTEXT -log "If it's >true<, the domainname will be ignored."
   else
   $CLEAR
   $INFOTEXT -u "\nSelect default Grid Engine hostname resolving method"
   $INFOTEXT "\nAre all hosts of your cluster in one DNS domain? If thisis\n" \
             "the case the hostnames\n\n" \
             "   >hostA< and >hostA.foo.com<\n\n" \
             "would be treated as eqal, because the DNS domain name >foo.com<\n" \
             "is ignored when comparing hostnames.\n\n"

   $INFOTEXT -auto $AUTO -ask "y" "n" -def "y" -n \
             "Are all hosts of your cluster in a single DNS domain (y/n) [y] >> "
   if [ $? = 0 ]; then
      IGNORE_FQDN_DEFAULT=true
      $INFOTEXT "Ignoring domainname when comparing hostnames."
   else
      IGNORE_FQDN_DEFAULT=false
      $INFOTEXT "The domainname is not ignored when comparing hostnames."
   fi
   $INFOTEXT -wait -auto $AUTO -n "\nHit <RETURN> to continue >> "
   $CLEAR
   fi
   #if [ $fast = false -a "$IGNORE_FQDN_DEFAULT" = false ]; then
    #  GetDefaultDomain
   #else
      CFG_DEFAULT_DOMAIN=none
   #fi
}


#-------------------------------------------------------------------------
# SetProductMode
#
SetProductMode()
{
   if [ $SGEEE = true ]; then
      PRODUCT_PREFIX=sgeee
   else
      PRODUCT_PREFIX=sge
   fi

   if [ $RESPORT = true ]; then
      RESPORT_PREFIX=-reserved_port
   else
      RESPORT_PREFIX=""
   fi

   if [ $AFS = true ]; then
      AFS_PREFIX=-afs
   else
      AFS_PREFIX=""
   fi

   if [ $CSP = true ]; then
      X509_COUNT=`strings $SGE_BIN/sge_qmaster | grep X509 | wc -l`
      if [ 50 -gt $X509_COUNT ]; then
         $INFOTEXT "\n>sge_qmaster< binary is not compiled with >-secure< option!\n"
         $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> to cancel the installation >> "
         exit 1
      else
         CSP_PREFIX=-csp
      fi  
   else
      CSP_PREFIX=""
   fi

   PRODUCT_MODE="${PRODUCT_PREFIX}${RESPORT_PREFIX}${AFS_PREFIX}${CSP_PREFIX}"
}


#-------------------------------------------------------------------------
# Make directories needed by qmaster
#
MakeDirsMaster()
{
   $INFOTEXT -u "\nMaking directories"
   $INFOTEXT -log "Making directories"
   Makedir $SGE_CELL_VAL
   Makedir $COMMONDIR
   Makedir $QMDIR
   Makedir $QMDIR/job_scripts

   $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> to continue >> "
   $CLEAR
}


#-------------------------------------------------------------------------
# Adding Bootstrap information
#
AddBootstrap()
{
   $INFOTEXT -u "\nDumping bootstrapping information"
   $INFOTEXT -log "Dumping bootstrapping information"

   PrintBootstrap > $COMMONDIR/bootstrap
}

#-------------------------------------------------------------------------
# PrintBootstrap: print SGE/SGEEE default configuration
#
PrintBootstrap()
{
   $ECHO "# Version: pre6.0"
   $ECHO "#"
   if [ $ADMINUSER != default ]; then
      $ECHO "admin_user             $ADMINUSER"
   else
      $ECHO "admin_user             none"
   fi
   $ECHO "default_domain         $CFG_DEFAULT_DOMAIN"
   $ECHO "ignore_fqdn            $IGNORE_FQDN_DEFAULT"
   $ECHO "spooling_method        $SPOOLING_METHOD"
   $ECHO "spooling_lib           $SPOOLING_LIB"
   $ECHO "spooling_params        $SPOOLING_ARGS"
   $ECHO "binary_path            $SGE_ROOT_VAL/bin"
   $ECHO "qmaster_spool_dir      $QMDIR"
   $ECHO "product_mode           $PRODUCT_MODE"
}


#-------------------------------------------------------------------------
# Initialize the spooling database (or directory structure)
#
InitSpoolingDatabase()
{
   $INFOTEXT -u "\nInitializing spooling database"
   $INFOTEXT -log "Initializing spooling database"
   ExecuteAsAdmin $SPOOLINIT $SPOOLING_LIB "$SPOOLING_ARGS" init

   $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> to continue >> "
   $CLEAR
}


#-------------------------------------------------------------------------
# AddConfiguration
#
AddConfiguration()
{
   useold=false
#
# JG: TODO: a maintenance command could dump the configuration to file
#           we display it and decide whether to overwrite it.
#           To Change: spoolinit has to check whether the database already
#           exists and not overwrite it.
#
#   if [ -f $COMMONDIR/configuration ]; then
#      $INFOTEXT -u "\nCreating global cluster configuration"
#      $INFOTEXT "\nA global cluster configuration file already exists.\n"
#      $INFOTEXT -wait -auto $autoinst -n "Hit <RETURN> to display the configuration >> "
#
#      cat $COMMONDIR/configuration |grep -v conf_version |more
#
#      QMDIR_IN_CFG=`grep qmaster_spool_dir $COMMONDIR/configuration | awk '{print $2}'`
#      if [ "$QMDIR_IN_CFG" != $QMDIR ]; then
#         $CLEAR
#         $INFOTEXT -u "\nERROR - new qmaster spool directory"
#         $INFOTEXT "\nThe qmaster spool directory in your existing configuration is set to\n\n" \
#                   "   %s\n\n" \
#                   "and differs from your previous selection during this installation where you\n" \
#                   "set the qmaster spool directory to\n\n" \
#                   "   %s\n\n" \
#                   "Please either copy your old qmaster spool directory to the new directory and\n" \
#                   "edit the existing cluster configuration file to reflect this change and\n" \
#                   "restart the installation or delete the current cluster configuration file.\n" \
#                   "$QMDIR_IN_CFG" "$QMDIR"
#         $INFOTEXT -wait -auto $autoinst -n "Hit <RETURN> to cancel the installation >> "
#         exit 1
#      else
#         $INFOTEXT -auto $autoinst -ask "y" "n" -def "y" -n \
#                   "Do you want to create a new configuration (y/n) [y] >> "
#         if [ $? = 1 ]; then
#            $INFOTEXT -wait -auto $autoinst -n "Using existing configuration. Hit <RETURN> to continue >> "
#            useold=true
#         fi
#      fi
#   fi

   if [ $useold = false ]; then
      GetConfiguration
      #TruncCreateAndMakeWriteable $COMMONDIR/configuration
      #PrintConf >> $COMMONDIR/configuration
      #SetPerm $COMMONDIR/configuration
      TMPC=/tmp/configuration
      rm -f $TMPC
      PrintConf > $TMPC
      ExecuteAsAdmin $SPOOLDEFAULTS configuration $TMPC
      rm -f $TMPC
   fi
}


#-------------------------------------------------------------------------
# PrintConf: print SGE/SGEEE default configuration
#
PrintConf()
{
   $ECHO "# Version: pre6.0"
   $ECHO "#"
   $ECHO "# DO NOT MODIFY THIS FILE MANUALLY!"
   $ECHO "#"
   $ECHO "conf_version           0"
   $ECHO "execd_spool_dir        $CFG_EXE_SPOOL"
   $ECHO "mailer                 $MAILER"
   $ECHO "xterm                  $XTERM"
   $ECHO "load_sensor            none"
   $ECHO "prolog                 none"
   $ECHO "epilog                 none"
   $ECHO "shell_start_mode       posix_compliant"
   $ECHO "login_shells           sh,ksh,csh,tcsh"
   $ECHO "min_uid                0"
   $ECHO "min_gid                0"
   $ECHO "user_lists             none"
   $ECHO "xuser_lists            none"
   if [ $SGEEE = true ]; then
      $ECHO "projects               none"
      $ECHO "xprojects              none"
      $ECHO "enforce_project        false"
      $ECHO "enforce_user           false"
   fi
   $ECHO "load_report_time       00:00:40"
   $ECHO "stat_log_time          48:00:00"
   $ECHO "max_unheard            00:05:00"
   $ECHO "reschedule_unknown     00:00:00"
   $ECHO "loglevel               log_warning"
   $ECHO "administrator_mail     $CFG_MAIL_ADDR"
   if [ $AFS = true ]; then
      $ECHO "set_token_cmd          /path_to_token_cmd/set_token_cmd"
      $ECHO "pag_cmd                /usr/afsws/bin/pagsh"
      $ECHO "token_extend_time      24:0:0"
   else
      $ECHO "set_token_cmd          none"
      $ECHO "pag_cmd                none"
      $ECHO "token_extend_time      none"
   fi
   $ECHO "shepherd_cmd           none"
   $ECHO "qmaster_params         none"
   $ECHO "schedd_params          none"
   $ECHO "execd_params           none"
   $ECHO "reporting_params       accounting=true reporting=false flush_time=00:00:15 joblog=false sharelog=00:00:00"
   $ECHO "finished_jobs          100"
   $ECHO "gid_range              $CFG_GID_RANGE"
   $ECHO "qlogin_command         $QLOGIN_COMMAND"
   $ECHO "qlogin_daemon          $QLOGIN_DAEMON"
   $ECHO "rlogin_daemon          $RLOGIN_DAEMON"
   $ECHO "max_aj_instances       2000"
   $ECHO "max_aj_tasks           75000"
   $ECHO "max_u_jobs             0"
   $ECHO "max_jobs               0"
}


#-------------------------------------------------------------------------
# PrintLocalConf:  print execution host local SGE/SGEEE configuration
#
#PrintLocalConf()
#{
#
#   arg=$1
#   if [ $arg = 1 ]; then
#      $ECHO "# Version: pre6.0"
#      $ECHO "#"
#      $ECHO "# DO NOT MODIFY THIS FILE MANUALLY!"
#      $ECHO "#"
#      $ECHO "conf_version           0"
#   fi
#   $ECHO "mailer                 $MAILER"
#   $ECHO "xterm                  $XTERM"
#   $ECHO "qlogin_daemon          $QLOGIN_DAEMON"
#   $ECHO "rlogin_daemon          $RLOGIN_DAEMON"
#}


#-------------------------------------------------------------------------
# AddLocalConfiguration
#
AddLocalConfiguration()
{
   useold=false

   $CLEAR
   $INFOTEXT -u "\nCreating local configuration"
   # JG: TODO: see comment regarding existing global configuration
#   if [ -f $LCONFDIR/$HOST ]; then
#      $INFOTEXT "\nA local configuration for this host already exists.\n"
#
#      $INFOTEXT -wait -auto $autoinst -n "Hit <RETURN> to display configuration >> "
#
#      cat $LCONFDIR/$HOST |grep -v conf_version |more
#
#      $INFOTEXT -auto $autoinst -ask "y" "n" -def "n" -n \
#                "\nDo you want to create a new configuration (y/n) [n] >> "
#      if [ $? = 1 ]; then
#         $INFOTEXT -wait -auto $autoinst -n "Keeping existing configuration. Hit <RETURN> to continue >> "
#         useold=true
#      else
#         $INFOTEXT -wait -auto $autoinst -n "Creating new local configuration. Hit <RETURN> to continue >> "
#      fi
#      $CLEAR
#   fi

   if [ $useold = false ]; then
#      TruncCreateAndMakeWriteable $LCONFDIR/$HOST
#      PrintLocalConf 1 >> $LCONFDIR/$HOST
#      SetPerm $LCONFDIR/$HOST
      TMPH=/tmp/$HOST
      rm -f $TMPH
      PrintLocalConf 1 > $TMPH
      ExecuteAsAdmin $SPOOLDEFAULTS local_conf $TMPH $HOST
      rm -f $TMPH
   fi
}


#-------------------------------------------------------------------------
# GetConfiguration: get some parameters for global configuration
#
GetConfiguration()
{

   GetGidRange

   #if [ $fast = true ]; then
   #   CFG_EXE_SPOOL=$SGE_ROOT_VAL/$SGE_CELL_VAL/spool
   #   CFG_MAIL_ADDR=none
   #   return 0
   #fi
   if [ $AUTO = true ]; then
     CFG_EXE_SPOOL=$EXECD_SPOOL_DIR
     CFG_MAIL_ADDR=$ADMIN_MAIL
     $INFOTEXT -log "Using >%s< as EXECD_SPOLL_DIR." "$CFG_EXE_SPOOL"
     $INFOTEXT -log "Using >%s< as ADMIN_MAIL." "$ADMIN_MAIL"
   else
   done=false
   while [ $done = false ]; do
      $CLEAR
      $INFOTEXT -u "\nGrid Engine cluster configuration"
      $INFOTEXT "\nPlease give the basic configuration parameters of your Grid Engine\n" \
                "installation:\n\n   <execd_spool_dir>\n\n"

      if [ $ADMINUSER != default ]; then
            $INFOTEXT "The pathname of the spool directory of the execution hosts. User >%s<\n" \
                      "must have the right to create this directory and to write into it.\n" "$ADMINUSER"
      elif [ $euid = 0 ]; then
            $INFOTEXT "The pathname of the spool directory of the execution hosts. User >root<\n" \
                      "must have the right to create this directory and to write into it.\n"
      else
            $INFOTEXT "The pathname of the spool directory of the execution hosts. You\n" \
                      "must have the right to create this directory and to write into it.\n"
      fi

      $INFOTEXT -n "Default: [%s] >> " $SGE_ROOT_VAL/$SGE_CELL_VAL/spool

      CFG_EXE_SPOOL=`Enter $SGE_ROOT_VAL/$SGE_CELL_VAL/spool`

      $CLEAR
      $INFOTEXT -u "\nGrid Engine cluster configuration (continued)"
      $INFOTEXT -n "\n<administrator_mail>\n\n" \
                   "The email address of the administrator to whom problem reports are sent.\n\n" \
                   "It's is recommended to configure this parameter. You may use >none<\n" \
                   "if you do not wish to receive administrator mail.\n\n" \
                   "Please enter an email address in the form >user@foo.com<.\n\n" \
                   "Default: [none] >> "

      CFG_MAIL_ADDR=`Enter none`

      $CLEAR

      $INFOTEXT "\nThe following parameters for the cluster configuration were configured:\n\n" \
                "   execd_spool_dir        %s\n" \
                "   administrator_mail     %s\n" $CFG_EXE_SPOOL $CFG_MAIL_ADDR

      $INFOTEXT -auto $AUTO -ask "y" "n" -def "n" -n \
                "Do you want to change the configuration parameters (y/n) [n] >> "
      if [ $? = 1 ]; then
         done=true
      fi
   done
   fi
   export CFG_EXE_SPOOL
}


#-------------------------------------------------------------------------
# GetGidRange
#
GetGidRange()
{
   done=false
   while [ $done = false ]; do
      $CLEAR
      $INFOTEXT -u "\nGrid Engine group id range"
      $INFOTEXT "\nWhen jobs are started under the control of Grid Engine an additional group id\n" \
                "is set on platforms which do not support jobs. This is done to provide maximum\n" \
                "control for Grid Engine jobs.\n\n" \
                "This additional UNIX group id range must be unused group id's in your system.\n" \
                "Each job will be assigned a unique id during the time it is running.\n" \
                "Therefore you need to provide a range of id's which will be assigned\n" \
                "dynamically for jobs.\n\n" \
                "The range must be big enough to provide enough numbers for the maximum number\n" \
                "of Grid Engine jobs running at a single moment on a single host. E.g. a range\n" \
                "like >20000-20100< means, that Grid Engine will use the group ids from\n" \
                "20000-20100 and provides a range for 100 Grid Engine jobs at the same time\n" \
                "on a single host.\n\n" \
                "You can change at any time the group id range in your cluster configuration.\n"

      $INFOTEXT -n "Please enter a range >> "

      CFG_GID_RANGE=`Enter $GID_RANGE`

      if [ "$CFG_GID_RANGE" != "" ]; then
         $INFOTEXT -wait -auto $AUTO -n "\nUsing >%s< as gid range. Hit <RETURN> to continue >> " \
                   "$CFG_GID_RANGE"
         $CLEAR
         done=true
      fi
     $INFOTEXT -log "Using >%s< as gid range." "$CFG_GID_RANGE"  
   done
}


#-------------------------------------------------------------------------
# AddActQmaster: create act_qmaster file
#
AddActQmaster()
{
   $INFOTEXT "Creating >act_qmaster< file"

   TruncCreateAndMakeWriteable $COMMONDIR/act_qmaster
   $ECHO $HOST >> $COMMONDIR/act_qmaster
   SetPerm $COMMONDIR/act_qmaster
}


#-------------------------------------------------------------------------
# AddDefaultComplexes
#
AddDefaultComplexes()
{
   $INFOTEXT "Adding default complex attributes"
#   for c in arch h_rt mem_free num_proc  s_rt swap_total calendar h_stack mem_total qname s_stack swap_used cpu h_vmem mem_used rerun s_vmem tmpdir h_core hostname min_cpu_interval s_core seq_no virtual_free h_cpu load_avg np_load_avg s_cpu slots virtual_total h_data load_long np_load_long s_data swap_free virtual_used h_fsize load_medium np_load_medium s_fsize swap_rate h_rss load_short np_load_short s_rss swap_rsvd; do
#      if [ -f $QMDIR/centry/$c -a -s $QMDIR/centry/$c ]; then
#         $INFOTEXT -auto $autoinst -ask "y" "n" -def "y" -n \
#                   "Complex attribute >%s< already exists - should Complex attribute be preserved (y/n) [y] >> " $c
#         if [ $? = 1 ]; then
#            $INFOTEXT "Overwriting existing complex attribute>%s<" $c
#            ExecuteAsAdmin cp util/resources/centry/$c $QMDIR/centry
#         fi
#      else
#         ExecuteAsAdmin cp util/resources/centry/$c $QMDIR/centry
#      fi
#   done
#   ExecuteAsAdmin chmod $FILEPERM $QMDIR/centry/*
   ExecuteAsAdmin $SPOOLDEFAULTS complexes $SGE_ROOT_VAL/util/resources/centry

}


#-------------------------------------------------------------------------
# AddCommonFiles
#    Copy files from util directory to common dir
#
AddCommonFiles()
{
   for f in sge_aliases qtask sge_request; do
      if [ $f = sge_aliases ]; then
         $INFOTEXT "Adding >%s< path aliases file" $f
      elif [ $f = qtask ]; then
         $INFOTEXT "Adding >%s< qtcsh sample default request file" $f
      else
         $INFOTEXT "Adding >%s< default submit options file" $f
      fi
      ExecuteAsAdmin cp util/$f $COMMONDIR
      ExecuteAsAdmin chmod $FILEPERM $COMMONDIR/$f
   done

   unset f
}

#-------------------------------------------------------------------------
# AddPEFiles
#    Copy files from PE template directory to qmaster spool dir
#
AddPEFiles()
{
   $INFOTEXT "Adding default parallel environments (PE)"
   $INFOTEXT -log "Adding default parallel environments (PE)"
#   for c in make; do
#      if [ -f $QMDIR/pe/$c -a -s $QMDIR/pe/$c ]; then
#         $INFOTEXT -auto $autoinst -ask "y" "n" -def "y" -n \
#                   "PE >%s< already exists - should PE be preserved (y/n) [y] >> " $c
#         if [ $? = 1 ]; then
#            $INFOTEXT "Overwriting existing PE >%s<" $c
#            ExecuteAsAdmin cp util/resources/pe/$c $QMDIR/pe
#         fi
#      else
#         ExecuteAsAdmin cp util/resources/pe/$c $QMDIR/pe
#      fi
#   done
#   ExecuteAsAdmin chmod $FILEPERM $QMDIR/pe/*
   ExecuteAsAdmin $SPOOLDEFAULTS pes $SGE_ROOT_VAL/util/resources/pe
}


#-------------------------------------------------------------------------
# AddDefaultDepartement
#
AddDefaultDepartement()
{
   if [ $SGEEE = true ]; then
      #$INFOTEXT "Adding SGEEE >defaultdepartment< userset"
      #ExecuteAsAdmin $CP util/resources/usersets/defaultdepartment $QMDIR/usersets
      #ExecuteAsAdmin $CHMOD $FILEPERM $QMDIR/usersets/defaultdepartment

      #$INFOTEXT "Adding SGEEE >deadlineusers< userset"
      #ExecuteAsAdmin $CP util/resources/usersets/deadlineusers $QMDIR/usersets
      #ExecuteAsAdmin $CHMOD 644 $QMDIR/usersets/deadlineusers

      $INFOTEXT "Adding SGEEE default usersets"
      ExecuteAsAdmin $SPOOLDEFAULTS usersets $SGE_ROOT_VAL/util/resources/usersets
   fi
}


#-------------------------------------------------------------------------
# CreateSettingsFile: Create resource files for csh/sh
#
CreateSettingsFile()
{
   $INFOTEXT "Creating settings files for >.profile/.cshrc<"


   if [ -f $SGE_ROOT_VAL/$COMMONDIR/settings.sh ]; then
      ExecuteAsAdmin $RM $SGE_ROOT_VAL/$COMMONDIR/settings.sh
   fi

   if [ -f $SGE_ROOT_VAL/$COMMONDIR/settings.csh ]; then
      ExecuteAsAdmin $RM $SGE_ROOT_VAL/$COMMONDIR/settings.csh
   fi

   ExecuteAsAdmin util/create_settings.sh $SGE_ROOT_VAL/$COMMONDIR

   SetPerm $SGE_ROOT_VAL/$COMMONDIR/settings.sh
   SetPerm $SGE_ROOT_VAL/$COMMONDIR/settings.csh
}


#--------------------------------------------------------------------------
# InitCA Create CA and initialize it for deamons and users
#
InitCA()
{

   if [ $CSP = false ]; then
      return
   fi

   # Initialize CA, make directories and get DN info
   #
   util/sgeCA/sge_ca -init -days 365

   if [ $? != 0 ]; then
      CAErrUsage
   fi
   $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> to continue >> "
   $CLEAR
}

#-------------------------------------------------------------------------
# AddSGEStartUpScript: Add startup script to rc files if root installs
#
#AddSGEStartUpScript()
#{
#   euid=$1
#   create=$2
#
#   $CLEAR
#   $INFOTEXT -u "\nGrid Engine startup script"
#   $ECHO
#   TMP_SGE_STARTUP_FILE=/tmp/rcsge.$$
#   STARTUP_FILE_NAME=rcsgedefault
#   S95NAME=S95rcsge
#
#   if [ -f $TMP_SGE_STARTUP_FILE ]; then
#      Execute rm $TMP_SGE_STARTUP_FILE
#   fi
#   if [ -f ${TMP_SGE_STARTUP_FILE}.0 ]; then
#      Execute rm ${TMP_SGE_STARTUP_FILE}.0
#   fi
#   if [ -f ${TMP_SGE_STARTUP_FILE}.1 ]; then
#      Execute rm ${TMP_SGE_STARTUP_FILE}.1
#   fi
#
#   SGE_STARTUP_FILE=$SGE_ROOT_VAL/$COMMONDIR/$STARTUP_FILE_NAME
#
#   if [ $create = true ]; then
#
#      Execute sed -e "s%GENROOT%${SGE_ROOT_VAL}%g" \
#                  -e "s%GENCELL%${SGE_CELL_VAL}%g" \
#                  -e "/#+-#+-#+-#-/,/#-#-#-#-#-#/d" \
#                  util/startup_template > ${TMP_SGE_STARTUP_FILE}.0
#
#      if [ "$COMMD_PORT" != "" ]; then
#         Execute sed -e "s/=GENCOMMD_PORT/=$COMMD_PORT/" \
#                     ${TMP_SGE_STARTUP_FILE}.0 > $TMP_SGE_STARTUP_FILE
#      else
#         Execute sed -e "/GENCOMMD_PORT/d" \
#                     ${TMP_SGE_STARTUP_FILE}.0 > $TMP_SGE_STARTUP_FILE
#      fi
#
#      ExecuteAsAdmin $CP $TMP_SGE_STARTUP_FILE $SGE_STARTUP_FILE
#      ExecuteAsAdmin $CHMOD a+x $SGE_STARTUP_FILE
#
#      rm -f $TMP_SGE_STARTUP_FILE ${TMP_SGE_STARTUP_FILE}.0 ${TMP_SGE_STARTUP_FILE}.1
#
#      if [ $euid = 0 -a $ADMINUSER != default -a $QMASTER = "install" ]; then
#         AddDefaultManager root $ADMINUSER
#         AddDefaultOperator $ADMINUSER
#      elif [ $euid != 0 ]; then
#         AddDefaultManager $USER
#         AddDefaultOperator $USER
#      fi
#
#      $INFOTEXT "Your Grid Engine cluster wide startup script is installed as:\n\n" \
#                "   %s<\n\n" $SGE_STARTUP_FILE
#      $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> to continue >> "
#   fi
#
#   $CLEAR
#
#   if [ $euid != 0 ]; then
#      return 0
#   fi
#
#   $INFOTEXT -u "\nGrid Engine startup script"
#
#   # --- from here only if root installs ---
#   $INFOTEXT -auto $AUTO -ask "y" "n" -def "n" -n \
#             "\nWe can install the startup script that\n" \
#             "Grid Engine is started at machine boot (y/n) [n] >> "
#
#   if [ $AUTO = true -a $ADD_TO_RC = true ]; then
#      :
#   else
#      if [ $? = 1 ]; then
#         $CLEAR
#         return
#      fi
#   fi
#
#   # If we have System V we need to put the startup script to $RC_PREFIX/init.d
#   # and make a link in $RC_PREFIX/rc2.d to $RC_PREFIX/init.d
#   if [ "$RC_FILE" = "sysv_rc" ]; then
#      $INFOTEXT "Installing startup script %s" "$RC_PREFIX/$RC_DIR/$S95NAME"
#      Execute rm -f $RC_PREFIX/$RC_DIR/$S95NAME
#      Execute cp $SGE_STARTUP_FILE $RC_PREFIX/init.d/$STARTUP_FILE_NAME
#      Execute chmod a+x $RC_PREFIX/init.d/$STARTUP_FILE_NAME
#      Execute ln -s $RC_PREFIX/init.d/$STARTUP_FILE_NAME $RC_PREFIX/$RC_DIR/$S95NAME
#
#      # runlevel management in Linux is different -
#      # each runlevel contains full set of links
#      # RedHat uses runlevel 5 and SUSE runlevel 3 for xdm
#      # RedHat uses runlevel 3 for full networked mode
#      # Suse uses runlevel 2 for full networked mode
#      # we already installed the script in level 3
#      if [ $ARCH = linux -o $ARCH = glinux -o $ARCH = alinux -o $ARCH = slinux ]; then
#         runlevel=`grep "^id:.:initdefault:"  /etc/inittab | cut -f2 -d:`
#         if [ "$runlevel" = 2 -o  "$runlevel" = 5 ]; then
#            $INFOTEXT "Installing startup script also in %s" "$RC_PREFIX/rc${runlevel}.d/$S95NAME"
#            Execute rm -f $RC_PREFIX/rc${runlevel}.d/$S95NAME
#            Execute ln -s $RC_PREFIX/init.d/$STARTUP_FILE_NAME $RC_PREFIX/rc${runlevel}.d/$S95NAME
#         fi
#      fi
#   elif [ "$RC_FILE" = "insserv-linux" ]; then
#      echo  cp $SGE_STARTUP_FILE $RC_PREFIX/$STARTUP_FILE_NAME
#      echo /sbin/insserv $RC_PREFIX/$STARTUP_FILE_NAME
#      Execute cp $SGE_STARTUP_FILE $RC_PREFIX/$STARTUP_FILE_NAME
#      /sbin/insserv $RC_PREFIX/$STARTUP_FILE_NAME
#   elif [ "$RC_FILE" = "freebsd" ]; then
#      echo  cp $SGE_STARTUP_FILE $RC_PREFIX/sge${RC_SUFFIX}
#      Execute cp $SGE_STARTUP_FILE $RC_PREFIX/sge${RC_SUFFIX}
#   else
#      # if this is not System V we simple add the call to the
#      # startup script to RC_FILE
#
#      # Start-up script already installed?
#      #------------------------------------
#      grep $STARTUP_FILE_NAME $RC_FILE > /dev/null 2>&1
#      status=$?
#      if [ $status != 0 ]; then
#         $INFOTEXT "Adding application startup to %s" $RC_FILE
#         # Add the procedure
#         #------------------
#         $ECHO "" >> $RC_FILE
#         $ECHO "" >> $RC_FILE
#         $ECHO "# Grid Engine start up" >> $RC_FILE
#         $ECHO "#-$LINE---------" >> $RC_FILE
#         $ECHO $SGE_STARTUP_FILE >> $RC_FILE
#      else
#         $INFOTEXT "Found a call of %s in %s. Replacing with new call.\n" \
#                   "Your old file %s is saved as %s" $STARTUP_FILE_NAME $RC_FILE $RC_FILE $RC_FILE.org.1
#
#         mv $RC_FILE.org.3 $RC_FILE.org.4    2>/dev/null
#         mv $RC_FILE.org.2 $RC_FILE.org.3    2>/dev/null
#         mv $RC_FILE.org.1 $RC_FILE.org.2    2>/dev/null
#
#         # save original file modes of RC_FILE
#         uid=`$SGE_UTILBIN/filestat -uid $RC_FILE`
#         gid=`$SGE_UTILBIN/filestat -gid $RC_FILE`
#         perm=`$SGE_UTILBIN/filestat -mode $RC_FILE`
#
#         Execute cp $RC_FILE $RC_FILE.org.1
#
#         savedfile=`basename $RC_FILE`
#
#         sed -e "s%.*$STARTUP_FILE_NAME.*%$SGE_STARTUP_FILE%" \
#                 $RC_FILE > /tmp/$savedfile.1
#
#         Execute cp /tmp/$savedfile.1 $RC_FILE
#         Execute chown $uid $RC_FILE
#         Execute chgrp $gid $RC_FILE
#         Execute chmod $perm $RC_FILE
#         Execute rm -f /tmp/$savedfile.1
#      fi
#   fi
#
#   $INFOTEXT -wait -auto $AUTO -n "\nHit <RETURN> to continue >> "
#   $CLEAR
#}


#--------------------------------------------------------------------------
# StartQmaster
#
StartQmaster()
{
   $INFOTEXT -u "\nGrid Engine qmaster and scheduler startup"
   $INFOTEXT "\nStarting qmaster and scheduler daemon. Please wait ..."
   $SGE_STARTUP_FILE -qmaster
   $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> to continue >> "
   $CLEAR
}


#-------------------------------------------------------------------------
# AddHosts
#
AddHosts()
{
   if [ $AUTO = "true" ]; then
      for h in $ADMIN_HOST_LIST; do
        if [ -f $h ]; then
           $INFOTEXT -log "Adding ADMIN_HOSTS from file %s" $h
           for tmp in `cat $h`; do
             $INFOTEXT -log "Adding ADMIN_HOST %s" $tmp
             $SGE_BIN/qconf -ah $tmp
           done
        else
             $INFOTEXT -log "Adding ADMIN_HOST %s" $h
             $SGE_BIN/qconf -ah $h
        fi
      done
      
      for h in $SUBMIT_HOST_LIST; do
        if [ -f $h ]; then
           $INFOTEXT -log "Adding SUBMIT_HOSTS from file %s" $h
           for tmp in `cat $h`; do
             $INFOTEXT -log "Adding SUBMIT_HOST %s" $tmp
             $SGE_BIN/qconf -as $tmp
           done
        else
             $INFOTEXT -log "Adding SUBMIT_HOST %s" $h
             $SGE_BIN/qconf -as $h
        fi
      done  
   else
      $INFOTEXT -u "\nAdding Grid Engine hosts"
      $INFOTEXT "\nPlease now add the list of hosts, where you will later install your execution\n" \
                "daemons. These hosts will be also added as valid submit hosts.\n\n" \
                "Please enter a blank separated list of your execution hosts. You may\n" \
                "press <RETURN> if the line is getting too long. Once you are finished\n" \
                "simply press <RETURN> without entering a name.\n\n" \
                "You also may prepare a file with the hostnames of the machines where you plan\n" \
                "to install Grid Engine. This may be convenient if you are installing Grid\n" \
                "Engine on many hosts.\n\n"

      $INFOTEXT -auto $AUTO -ask "y" "n" -def "n" -n \
                "Do you want to use a file which contains the list of hosts (y/n) [n] >> "
      ret=$?
      if [ $ret = 0 ]; then
         AddHostsFromFile
         ret=$?
      fi

      if [ $ret = 1 ]; then
         AddHostsFromTerminal
      fi

      $INFOTEXT -wait -auto $AUTO -n "Finished adding hosts. Hit <RETURN> to continue >> "
      $CLEAR
   fi

   $INFOTEXT -u "\nCreating the default <all.q> queue and <allhosts> hostgroup"
   $INFOTEXT -log "Creating the default <all.q> queue and <allhosts> hostgroup"
   TMPL=/tmp/hostqueue$$
   TMPL2=${TMPL}.q
   rm -f $TMPL $TMPL2
   if [ -f $TMPL -o -f $TMPL2 ]; then
      $INFOTEXT "\nCan't delete template files >%s< or >%s<" "$TMPL" "$TMPL2"
   else
      PrintHostGroup @allhosts > $TMPL
      Execute $SGE_BIN/qconf -Ahgrp $TMPL
      Execute $SGE_BIN/qconf -sq > $TMPL
      Execute sed -e "/qname/s/template/all.q/" \
                  -e "/hostlist/s/NONE/@allhosts/" \
                  -e "/pe_list/s/NONE/make/" $TMPL > $TMPL2
      Execute $SGE_BIN/qconf -Aq $TMPL2
      rm -f $TMPL $TMPL2        
   fi

   $INFOTEXT -wait -auto $AUTO -n "\nHit <RETURN> to continue >> "
   $CLEAR
}


#-------------------------------------------------------------------------
# AddHostsFromFile: Get a list of hosts and add them as
# admin and submit hosts
#
AddHostsFromFile()
{
   file=$1
   done=false
   while [ $done = false ]; do
      $CLEAR
      $INFOTEXT -u "\nAdding admin and submit hosts from file"
      $INFOTEXT -n "\nPlease enter the file name which contains the host list: "
      file=`Enter none`
      if [ "$file" = "none" -o ! -f "$file" ]; then
         $INFOTEXT "\nYou entered an invalid file name or the file does not exist."
         $INFOTEXT -auto $autoinst -ask "y" "n" -def "y" -n \
                   "Do you want to enter a new file name (y/n) [y] >> "
         if [ $? = 1 ]; then
            return 1
         fi
      else
         for h in `cat $file`; do
            $SGE_BIN/qconf -ah $h
            $SGE_BIN/qconf -as $h
         done
         done=true
      fi
   done
}

#-------------------------------------------------------------------------
# AddHostsFromTerminal
#    Get a list of hosts and add the mas admin and submit hosts
#
AddHostsFromTerminal()
{
   stop=false
   while [ $stop = false ]; do
      $CLEAR
      $INFOTEXT -u "\nAdding admin and submit hosts"
      $INFOTEXT "\nPlease enter a blank seperated list of hosts.\n\n" \
                "Stop by entering <RETURN>. You may repeat this step until you are\n" \
                "entering an empty list. You will see messages from Grid Engine\n" \
                "when the hosts are added.\n"

      $INFOTEXT -n "Host(s): "

      hlist=`Enter ""`
      for h in $hlist; do
         $SGE_BIN/qconf -ah $h
         $SGE_BIN/qconf -as $h
      done
      if [ "$hlist" = "" ]; then
         stop=true
      else
         $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> to continue >> "
      fi
  done
}


#-------------------------------------------------------------------------
# PrintHostGroup:  print an empty hostgroup
#
PrintHostGroup()
{
   $ECHO "group_name  $1"
   $ECHO "hostlist    NONE"
}


#-------------------------------------------------------------------------
# GetQmasterPort: get communication port SGE_QMASTER_PORT
#
GetQmasterPort()
{

    if [ $RESPORT = true ]; then
       comm_port_max=1023
    else
       comm_port_max=65500
    fi

    $SGE_UTILBIN/getservbyname $SGE_QMASTER_SRV > /dev/null 2>&1

    ret=$?

    if [ "$SGE_QMASTER_PORT" != "" ]; then
      $INFOTEXT -u "\nGrid Engine TCP/IP communication service"

      if [ $SGE_QMASTER_PORT -ge 1 -a $SGE_QMASTER_PORT -le $comm_port_max ]; then
         $INFOTEXT "\nUsing the environment variable\n\n" \
                   "   \$SGE_QMASTER_PORT=%s\n\n" \
                     "as port for communication.\n\n" $SGE_QMASTER_PORT
                      export SGE_QMASTER_PORT
                      $INFOTEXT -log "Using port >%s<." $SGE_QMASTER_PORT
         if [ $ret = 0 ]; then
            $INFOTEXT "This overrides the preset TCP/IP service >sge_qmaster<.\n"
         fi
         $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> to continue >> "
         $CLEAR
         return
      else
         $INFOTEXT "\nThe environment variable\n\n" \
                   "   \$SGE_QMASTER_PORT=%s\n\n" \
                   "has an invalid value (it must be in range 1..%s).\n\n" \
                   "Please set the environment variable \$SGE_QMASTER_PORT and restart\n" \
                   "the installation or configure the service >sge_qmaster<." $SGE_QMASTER_PORT $comm_port_max
         $INFOTEXT -log "Your \$SGE_QMASTER_PORT=%s\n\n" \
                   "has an invalid value (it must be in range 1..%s).\n\n" \
                   "Please check your configuration file and restart\n" \
                   "the installation or configure the service >sge_qmaster<." $SGE_QMASTER_PORT $comm_port_max
      fi
   fi         
      $INFOTEXT -u "\nGrid Engine TCP/IP service >sge_qmaster<"
   if [ $ret != 0 ]; then
      $INFOTEXT "\nThere is no service >sge_qmaster< available in your >/etc/services< file\n" \
                "or in your NIS/NIS+ database.\n\n" \
                "You may add this service now to your services database or choose a port number.\n" \
                "It is recommended to add the service now. If you are using NIS/NIS+ you should\n" \
                "add the service at your NIS/NIS+ server and not to the local >/etc/services<\n" \
                "file.\n\n" \
                "Please add an entry in the form\n\n" \
                "   sge_qmaster <port_number>/tcp\n\n" \
                "to your services database and make sure to use an unused port number.\n"

      $INFOTEXT -wait -auto $AUTO -n "Please add the service now or continue to enter a port number >> "

      # Check if $SGE_SERVICE service is available now
      service_available=false
      done=false
      while [ $done = false ]; do
         $SGE_UTILBIN/getservbyname $SGE_QMASTER_SRV 2>/dev/null
         if [ $? != 0 ]; then
            $CLEAR
            $INFOTEXT -u "\nNo TCP/IP service >sge_qmaster< yet"
            $INFOTEXT "\nThere is still no service for >sge_qmaster< available.\n\n" \
                      "If you have just added the service it may take a while until the service\n" \
                      "propagates in your network. If this is true we can again check for\n" \
                      "the service >sge_qmaster<. If you don't want to add this service or if\n" \
                      "you want to install Grid Engine just for testing purposes you can enter\n" \
                      "a port number.\n"

              if [ $AUTO != "true" ]; then
                 $INFOTEXT -auto $AUTO -ask "y" "n" -def "y" -n \
                      "Check again (if not you need to enter a port number later) (y/n) [y] >> "
              else
                 $INFOTEXT -log "Setting SGE_QMASTER_PORT"
                 `false`
              fi

            if [ $? = 0 ]; then
               :
            else
               $INFOTEXT -n "Please enter an unused port number >> "
               INP=`Enter $SGE_QMASTER_PORT`

               chars=`echo $INP | wc -c`
               chars=`expr $chars - 1`
               digits=`expr $INP : "[0-9][0-9]*"`
               if [ "$chars" != "$digits" ]; then
                  $INFOTEXT "\nInvalid input. Must be a number."
               elif [ $INP -le 1 -o $INP -ge $comm_port_max ]; then
                  $INFOTEXT "\nInvalid port number. Must be in range [1..%s]." $comm_port_max
               elif [ $INP -le 1024 -a $euid != 0 ]; then
                  $INFOTEXT "\nYou are not user >root<. You need to use a port above 1024."
               else
                  ser=`awk '{ print $2 }' /etc/services | grep "^${INP}/tcp"`
                  if [ "$ser" = "$INP/tcp" ]; then
                     $INFOTEXT "\nFound service with port number >%s< in >/etc/services<. Choose again." "$INP"
                  else
                     done=true
                  fi
               fi
               if [ $done = false ]; then
                  $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> to continue >> "
               fi
            fi
         else
            done=true
            service_available=true
         fi
      done

      if [ $service_available = false ]; then
         SGE_QMASTER_PORT=$INP
         export SGE_QMASTER_PORT
         $INFOTEXT "\nUsing port >%s<. No service >sge_qmaster< available.\n" $SGE_QMASTER_PORT
         $INFOTEXT -log "Using port >%s<. No service >sge_qmaster< available." $SGE_QMASTER_PORT
         $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> to continue >> "
         $CLEAR
      else
         unset SGE_QMASTER_PORT
         $INFOTEXT "\nService >sge_qmaster< is now available.\n"
         $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> to continue >> "
         $CLEAR
      fi
   else
      $INFOTEXT "\nUsing the service\n\n" \
                "   sge_qmaster\n\n" \
                "for communication with Grid Engine.\n"
      $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> to continue >> "
      $CLEAR
   fi

}

#-------------------------------------------------------------------------
# GetExecdPort: get communication port SGE_EXECD_PORT
#
GetExecdPort()
{

    if [ $RESPORT = true ]; then
       comm_port_max=1023
    else
       comm_port_max=65500
    fi

    $SGE_UTILBIN/getservbyname $SGE_EXECD_SRV > /dev/null 2>&1

    ret=$?

    if [ "$SGE_EXECD_PORT" != "" ]; then
      $INFOTEXT -u "\nGrid Engine TCP/IP communication service"

      if [ $SGE_EXECD_PORT -ge 1 -a $SGE_EXECD_PORT -le $comm_port_max ]; then
         $INFOTEXT "\nUsing the environment variable\n\n" \
                   "   \$SGE_EXECD_PORT=%s\n\n" \
                     "as port for communication.\n\n" $SGE_EXECD_PORT
                      export SGE_EXECD_PORT
                      $INFOTEXT -log "Using SGE_EXECD_PORT >%s<." $SGE_EXECD_PORT
         if [ $ret = 0 ]; then
            $INFOTEXT "This overrides the preset TCP/IP service >sge_execd<.\n"
         fi
         $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> to continue >> "
         $CLEAR
         return
      else
         $INFOTEXT "\nThe environment variable\n\n" \
                   "   \$SGE_EXECD_PORT=%s\n\n" \
                   "has an invalid value (it must be in range 1..%s).\n\n" \
                   "Please set the environment variable \$SGE_EXECD_PORT and restart\n" \
                   "the installation or configure the service >sge_execd<." $SGE_EXECD_PORT $comm_port_max
         $INFOTEXT -log "Your \$SGE_EXECD_PORT=%s\n\n" \
                   "has an invalid value (it must be in range 1..%s).\n\n" \
                   "Please check your configuration file and restart\n" \
                   "the installation or configure the service >sge_execd<." $SGE_EXECD_PORT $comm_port_max
      fi
   fi         
      $INFOTEXT -u "\nGrid Engine TCP/IP service >sge_execd<"
   if [ $ret != 0 ]; then
      $INFOTEXT "\nThere is no service >sge_execd< available in your >/etc/services< file\n" \
                "or in your NIS/NIS+ database.\n\n" \
                "You may add this service now to your services database or choose a port number.\n" \
                "It is recommended to add the service now. If you are using NIS/NIS+ you should\n" \
                "add the service at your NIS/NIS+ server and not to the local >/etc/services<\n" \
                "file.\n\n" \
                "Please add an entry in the form\n\n" \
                "   sge_execd <port_number>/tcp\n\n" \
                "to your services database and make sure to use an unused port number.\n"

      if [ "$EXECD" = "install" ]; then

         $INFOTEXT "Make sure to use a differnt port number for the Executionhost\n" \
                   "as on the qmaster machine\n"
         $INFOTEXT "The qmaster port SGE_QMASTER_PORT = %s\n" $SGE_QMASTER_PORT
      fi
      $INFOTEXT -wait -auto $AUTO -n "Please add the service now or continue to enter a port number >> "

      # Check if $SGE_SERVICE service is available now
      service_available=false
      done=false
      while [ $done = false ]; do
         $SGE_UTILBIN/getservbyname $SGE_EXECD_SRV 2>/dev/null
         if [ $? != 0 ]; then
            $CLEAR
            $INFOTEXT -u "\nNo TCP/IP service >sge_execd< yet"
            $INFOTEXT "\nThere is still no service for >sge_execd< available.\n\n" \
                      "If you have just added the service it may take a while until the service\n" \
                      "propagates in your network. If this is true we can again check for\n" \
                      "the service >sge_execd<. If you don't want to add this service or if\n" \
                      "you want to install Grid Engine just for testing purposes you can enter\n" \
                      "a port number.\n"

              if [ $AUTO != "true" ]; then
                 $INFOTEXT -auto $AUTO -ask "y" "n" -def "y" -n \
                      "Check again (if not you need to enter a port number later) (y/n) [y] >> "
              else
                 $INFOTEXT -log "Setting SGE_EXECD_PORT"
                 `false`
              fi

            if [ $? = 0 ]; then
               :
            else
               $INFOTEXT -n "Please enter an unused port number >> "
               INP=`Enter $SGE_EXECD_PORT`

               if [ $INP = $SGE_QMASTER_PORT ]; then
                  $INFOTEXT "Please use any other port number!!!"
                  $INFOTEXT "This %s port number is used by sge_qmaster" $SGE_QMASTER_PORT
                  if [ $AUTO = "true" ]; then
                     $INFOTEXT -log "Please use any other port number!!!"
                     $INFOTEXT -log "This %s port number is used by sge_qmaster" $SGE_QMASTER_PORT
                     $INFOTEXT -log "Installation failed!!!"
                     exit 1
                  fi
               fi
               chars=`echo $INP | wc -c`
               chars=`expr $chars - 1`
               digits=`expr $INP : "[0-9][0-9]*"`
               if [ "$chars" != "$digits" ]; then
                  $INFOTEXT "\nInvalid input. Must be a number."
               elif [ $INP -le 1 -o $INP -ge $comm_port_max ]; then
                  $INFOTEXT "\nInvalid port number. Must be in range [1..%s]." $comm_port_max
               elif [ $INP -le 1024 -a $euid != 0 ]; then
                  $INFOTEXT "\nYou are not user >root<. You need to use a port above 1024."
               else
                  ser=`awk '{ print $2 }' /etc/services | grep "^${INP}/tcp"`
                  if [ "$ser" = "$INP/tcp" ]; then
                     $INFOTEXT "\nFound service with port number >%s< in >/etc/services<. Choose again." "$INP"
                  else
                     done=true
                  fi
                  if [ $INP = $SGE_QMASTER_PORT ]; then
                     done=false
                  fi
               fi
               if [ $done = false ]; then
                  $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> to continue >> "
               fi
            fi
         else
            done=true
            service_available=true
         fi
      done

      if [ $service_available = false ]; then
         SGE_EXECD_PORT=$INP
         export SGE_EXECD_PORT
         $INFOTEXT "\nUsing port >%s<. No service >sge_execd< available.\n" $SGE_EXECD_PORT
         $INFOTEXT -log "Using port >%s<. No service >sge_execd< available." $SGE_EXECD_PORT
         $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> to continue >> "
         $CLEAR
      else
         unset SGE_EXECD_PORT
         $INFOTEXT "\nService >sge_execd< is now available.\n"
         $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> to continue >> "
         $CLEAR
      fi
   else
      $INFOTEXT "\nUsing the service\n\n" \
                "   sge_execd\n\n" \
                "for communication with Grid Engine.\n"
      $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> to continue >> "
      $CLEAR
   fi

   export SGE_EXECD_PORT
}

