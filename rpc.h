// This file is part of OpenPanel - The Open Source Control Panel
// The Grace library is free software: you can redistribute it and/or modify it 
// under the terms of the GNU General Public License as published by the Free 
// Software Foundation, either version 3 of the License.
//
// Please note that use of the OpenPanel trademark may be subject to additional 
// restrictions. For more information, please visit the Legal Information 
// section of the OpenPanel website on http://www.openpanel.com/

#include <grace/statstring.h>
#include <grace/value.h>
#include <grace/dictionary.h>
#include <grace/daemon.h>
#include "session.h"

//  -------------------------------------------------------------------------
/// Template class for callback handling inside class RPCHandler.
//  -------------------------------------------------------------------------
template <class base> class rpccmdlist
{
public:
	typedef value *(base::*kmethod)(const value &, CoreSession &);
	
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
	
	value *call (const statstring &cmd, const value &args, CoreSession &s)
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
class RPCHandler
{
public:
					 RPCHandler (SessionDB *s);
					~RPCHandler (void);

	value			*handle (const value &v, uid_t uid, const string &origin);

	value			*call (const statstring &c, const value &v, CoreSession &cs);
	value			*getLanguages (const value &v);
	value			*bind (const value &v, uid_t uid, const string &origin);

	value			*ping (const value &v, CoreSession &cs);
	value			*createObject (const value &v, CoreSession &cs);
	value			*deleteObject (const value &v, CoreSession &cs);
	value			*updateObject (const value &v, CoreSession &cs);
	value			*chown (const value &v, CoreSession &cs);
	value			*classInfo (const value &v, CoreSession &cs);
	value			*classXML (const value &v, CoreSession &cs);
	value			*callMethod (const value &v, CoreSession &cs);
	value			*getRecord (const value &v, CoreSession &cs);
	value			*getRecords (const value &v, CoreSession &cs);
	value			*queryRecords (const value &v, CoreSession &cs);
	value			*getParent (const value &v, CoreSession &cs);
	value			*getWorld (const value &v, CoreSession &cs);
	value			*listParamsForMethod (const value &v, CoreSession &cs);
	value			*listModules (const value &v, CoreSession &cs);
	value			*listClasses (const value &v, CoreSession &cs);
	
	void			 copySessionError (CoreSession &cs, value &into);
	void			 setError (int errcode, value &into);
	
protected:
	rpccmdlist<RPCHandler>	 handler;
	SessionDB				&sdb;
};
