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
 *   and/or Swiss Center for Scientific Computing
 * 
 *   Copyright: 2002 by Sun Microsystems, Inc.
 *   Copyright: 2002 by Swiss Center for Scientific Computing
 * 
 *   All Rights Reserved.
 * 
 ************************************************************************/
/*___INFO__MARK_END__*/
package com.sun.grid.jam.app;

import java.awt.event.*;
import java.awt.*;
import javax.swing.*;
import net.jini.core.entry.Entry;
import net.jini.admin.*;
import net.jini.discovery.*;
import net.jini.lookup.*;
import net.jini.lookup.entry.*;
import java.io.*;
import java.rmi.*;
import java.rmi.server.*;
import java.util.Date;

import com.sun.grid.jam.util.JAMAdmin;
import com.sun.grid.jam.util.JAMAdminProxy;
import com.sun.grid.jam.queue.QueueInterface;
import com.sun.grid.jam.admin.UserProperties;
import com.sun.grid.jam.job.entry.JobUserKey;
import com.sun.grid.jam.tools.JAMServiceUILauncher;

/**
 * Proxy for UserDefined Application.
 *
 * @version 1.5, 09/22/00
 *
 * @see UserAppAgent
 * @see com.sun.grid.jam.queue.QueueInterface
 *
 * @author Eric Sharakan
 */
public class UserDefinedApp
  extends AppProxy
{
  public UserDefinedApp()
  {
    appAgent = new UserAppAgent();
  }

  /**
   * Implement superclass's abstract submit method.  Wrapper around
   * QueueInterface's submit method.
   */
  public void submit(QueueInterface queue)
    throws RemoteException, IOException, ClassNotFoundException
  {
    // Need to fill in the script field of appParams
    try {
      appParams.setScriptContentsFromFile();
    } catch (IOException ioe) {
      System.err.println("Error reading script contents");
      System.err.println(ioe);
    }
    // Now do the submission
    String[] groups = { System.getProperty("user.name") };
    UserProperties userProps = new UserProperties("full name",
						  groups[0]);
    Entry jobEntry [] = { new Name(appParams.getName()),
			  new Name(groups[0]) ,
			  new JobUserKey(appParams.getName(),
					 userProps,
					 "localhost",
					 new Date())};
    queue.submit(appParams, jobEntry, userProps);
    JAMServiceUILauncher uiLauncher = new
      JAMServiceUILauncher(groups, locators,
			   jobEntry,
			   "com.sun.grid.jam.job.JobInterface");
  }
}


