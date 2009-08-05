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
