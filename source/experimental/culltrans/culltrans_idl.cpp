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
// culltrans_idl.cpp
// write IDL file

#include <map>
#include <set>
#include <string>
#include <iostream>
#include <fstream>
#include "culltrans_repository.h"
#include "culltrans.h"

#ifdef HAVE_STD
using namespace std;
#endif

static const string indent("   ");

// writeHeader
static void writeHeader(ofstream& idl, map<string, List>::iterator& elem) {
   vector<Elem>::iterator it;
   map<string, List>::iterator list = lists.end();

   idl << "// " << elem->second.name << ".idl" << endl;
   idl << "// this file is automatically generated. DO NOT EDIT" << endl << endl;
   idl << "#ifndef _" << elem->second.name << "_idl" << endl;
   idl << "#define _" << elem->second.name << "_idl" << endl << endl;

   if(elem->second.interf) {
      idl << "// forward decls" << endl;
      idl << "module GE {" << endl;
      idl << "   interface " << elem->second.name << ";" << endl;
      idl << "   typedef sequence<" << elem->second.name << "> " << elem->second.name << "Seq;" << endl;
      idl << "};" << endl << endl;
   }

   idl << "#include \"basic_types.idl\"" << endl;
   for(it=elem->second.elems.begin(); it != elem->second.elems.end(); ++it) {
      if(it->listType == "ST_Type")
         continue;

cout << "it->listType:" << it->listType << endl;   
      list = lists.find(it->listType);
      if(list != lists.end())
         idl << "#include \"" << list->second.name << ".idl\"" << endl;
   }
}

// write elem
// writes out the functions for a single element
static bool writeElem(ofstream& idl, vector<Elem>::iterator& it, map<string, List>::iterator& elem) {
   string buffer = "";
   map<string, List>::iterator list = lists.end();

   idl << indent << indent;
cout << "it->type:" << it->type << endl;   
   if(it->type == lListT) {
cout << "it->listType:" << it->listType << endl;   
      list = lists.find(it->listType);
      if(list != lists.end())
         if(it->object)
            buffer += list->second.name + " ";
         else
            buffer += list->second.name + "Seq ";
      else {
         cerr << "Could not find " << it->listType << "." << endl;
         return false;
      }
   }
   // else if(it->type == lBoolT)
   //     buffer += "boolean ";
   else {
      cout << it->type << ":" << multiType2idlType[it->type] << endl; 
      buffer += multiType2idlType[it->type];
      buffer += ' ';
   }

   if(elem->second.interf) {
      idl << buffer << "get_" << it->name;
      idl << "() raises(ObjDestroyed, Authentication, Error) context(\"sge_auth\")";
   }
   else
      idl << buffer << it->name;

   idl << ";" << endl;
   if(!it->readonly && elem->second.interf) {
      idl << indent << indent << "sge_ulong set_" << it->name;
      idl << "(in " << buffer << "val) raises(ObjDestroyed, Authentication, Error) context(\"sge_auth\");" << endl;
   }

   return true;
}

// writeIDL
// writes out the IDL file for a given interface or struct
bool writeIDL(map<string, List>::iterator& elem) {
   cout << "Creating IDL file for " << elem->second.name << endl;
   
   // open output file
   vector<Elem>::iterator it;
   string file(elem->second.name);
   file += ".idl";
   ofstream idl(file.c_str());
   if(!idl) {
      cerr << "Could not open output file for " << elem->second.name << endl;
      return false;
   }

   // write header and #includes
   writeHeader(idl, elem);

   // write interface definition
   string buffer;
   idl << endl << "module GE {" << endl;
   idl << indent << (elem->second.interf?"interface ":"struct ") << elem->second.name;
   idl << (elem->second.interf?" : GE::Obj ":" ") << "{" << endl;
   for(it=elem->second.elems.begin(); it != elem->second.elems.end(); ++it)
      if(!writeElem(idl, it, elem))
         return false;
   idl << endl << elem->second.idl;
   idl << indent << "};" << endl;

   // make sequence typedef for interfaces
   if(!elem->second.interf)
      idl << indent << "typedef sequence<" << elem->second.name << "> " << elem->second.name << "Seq;" << endl;

   // write footer
   idl << "};" << endl << endl;
   idl << "#endif // _" << elem->second.name << "_idl" << endl;
   idl.close();
   
   return true;
}


