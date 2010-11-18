// This file is part of the OpenPanel API
// OpenPanel is free software: you can redistribute it and/or modify it 
// under the terms of the GNU Lesser General Public License as published by the
// Free Software Foundation, using version 3 of the License.
//
// Please note that use of the OpenPanel trademark may be subject to additional 
// restrictions. For more information, please visit the Legal Information 
// section of the OpenPanel website on http://www.openpanel.com/

#include <grace-coreapi/module.h>

class Storpel : public CoreClass
{
public:
				 Storpel (void) : CoreClass ("Storpel")
				 {
				 	alias ("StorpelUser");
				 }
				 
				~Storpel (void)
				 {
				 }
				 
	bool		 create (const value &env)
				 {
				 	return update (env);
				 }
				 
	bool		 update (const value &env)
				 {
				 	file f;
				 	string fnam = "%s.conf" %format (id);
				 	f.openwrite (fnam);
				 	f.writeln ("Model %s" %format (param["model"]));
				 	
				 	value aliases = listaliases (env);
				 	foreach (alias, aliases)
				 	{
				 		f.writeln ("Alias %s" %format (alias));
				 	}
				 	
				 	foreach (usr, param["StorpelUser"])
				 	{
				 		f.writeln ("%s:%s" %format (usr.id(), usr["password"]));
				 	}
				 	
				 	f.close ();
				 	if (! authd.installfile (fnam, "/etc/storpels"))
				 	{
				 		seterror (CoreModule::E_AUTHD, authd.error);
				 		return false;
				 	}
				 	
				 	return true;
				 }
				 
	bool		 delete (const value &env)
				 {
				 	string rmpath = "/etc/storpels/%s.conf" %format (id);
				 	if (! authd.deletefile (rmpath))
				 	{
				 		seterror (CoreModule::E_AUTHD, authd.error);
				 		return false;
				 	}
				 	
				 	string fn = "%s.conf" %format (id);
				 	fs.rm (fn);
				 	return true;
				 }
};


class StorpelModule : public CoreModule
{
public:
				 StorpelModule (void)
				 	: CoreModule ("Storpel.module")
				 {
				 }
				 
				~StorpelModule (void)
				 {
				 }
				 
	Storpel		 storpel;
	StorpelUser	 storpeluser;
};

IMPLEMENT ("Storpel.module");
