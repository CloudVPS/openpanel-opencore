// --------------------------------------------------------------------------
// OpenPanel - The Open Source Control Panel
// Copyright (c) 2006-2008 PanelSix
//
// This software and its source code are subject to version 2 of the
// GNU General Public License. Please be aware that use of the OpenPanel
// and PanelSix trademarks and the IconBase.com iconset may be subject
// to additional restrictions. For more information on these restrictions
// and for a copy of version 2 of the GNU General Public License, please
// visit the Legal and Privacy Information section of the OpenPanel
// website on http://www.openpanel.com/
// --------------------------------------------------------------------------

#include <grace/statstring.h>
#include <grace/value.h>
#include <grace/dictionary.h>
#include "session.h"

//  -------------------------------------------------------------------------
/// Template class for callback handling inside class rpchandler.
//  -------------------------------------------------------------------------
template <class base> class rpccmdlist
{
public:
	typedef value *(base::*kmethod)(const value &, coresession &);
	
	rpccmdlist (base *par)
	{
		parent = par;
	}
	
	~rpccmdlist (void)
	{
	}
	
	void setcmd (const statstring &s, kmethod i)
	{
		if (! methods.exists (s))
		{
			kmethod *n = new kmethod;
			(*n) = i;
			methods.set (s, n);
		}
		methods[s] = i;
	}
	
	bool exists (const statstring &cmd) { return methods.exists (cmd); }
	
	value *call (const statstring &cmd, const value &args, coresession &s)
	{
		if (! methods.exists (cmd)) return NULL;
		kmethod foo = methods[cmd];
		return (parent->*foo)(args, s);
	}

protected:	
	dictionary<kmethod> methods;
	base *parent;
};

//  -------------------------------------------------------------------------
/// Command handler for rpc requests.
//  -------------------------------------------------------------------------
class rpchandler
{
public:
					 rpchandler (sessiondb *s);
					~rpchandler (void);

	value			*handle (const value &v, uid_t uid, const string &origin);

	value			*call (const statstring &c, const value &v, coresession &cs);
	value			*getlanguages (const value &v);
	value			*bind (const value &v, uid_t uid, const string &origin);

	value			*ping (const value &v, coresession &cs);
	value			*createobject (const value &v, coresession &cs);
	value			*deleteobject (const value &v, coresession &cs);
	value			*updateobject (const value &v, coresession &cs);
	value			*chown (const value &v, coresession &cs);
	value			*classinfo (const value &v, coresession &cs);
	value			*classxml (const value &v, coresession &cs);
	value			*callmethod (const value &v, coresession &cs);
	value			*getreferences (const value &v, coresession &cs);
	value			*getrecord (const value &v, coresession &cs);
	value			*getrecords (const value &v, coresession &cs);
	value			*getparent (const value &v, coresession &cs);
	value			*getworld (const value &v, coresession &cs);
	value			*listparamsformethod (const value &v, coresession &cs);
	value			*listmodules (const value &v, coresession &cs);
	value			*listclasses (const value &v, coresession &cs);
	value			*getmenu (const value &v, coresession &cs);
	value			*listtemplates (const value &v, coresession &cs);
	value			*applytemplate (const value &v, coresession &cs);
	value			*listmenus (const value &v, coresession &cs);
	
	void			 copysessionerror (coresession &cs, value &into);
	void			 seterror (int errcode, value &into);
	
protected:
	rpccmdlist<rpchandler>	 handler;
	sessiondb				&sdb;
};
