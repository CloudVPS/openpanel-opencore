#include <grace/value.h>
#include "rpc.h"
#include "error.h"
#include "opencore.h"
#include "debug.h"

// ==========================================================================
// CONSTRUCTOR rpchandler
// ==========================================================================
rpchandler::rpchandler (sessiondb *s) : handler (this), sdb (*s)
{
	#define AddCommand(foo) handler.setcmd ( #foo, &rpchandler:: foo )
	#define BindCommand(foo,bar) handler.setcmd (#foo, &rpchandler:: bar )
	
	BindCommand (create, createObject);
	BindCommand (delete, deleteObject);
	BindCommand (update, updateObject);
	AddCommand  (ping);
	AddCommand  (chown);
	AddCommand  (classinfo);
	AddCommand  (classxml);
	AddCommand  (callmethod);
	AddCommand  (getrecord);
	AddCommand  (getrecords);
	AddCommand  (getparent);
	AddCommand  (getworld);
	AddCommand  (listparamsformethod);
	AddCommand  (listmodules);
	AddCommand  (listclasses);
	
	#undef AddCommand
	#undef BindCommand
}

#define RPCRETURN(resname) \
		value *__rpc_res = $("header", $("session_id", cs.id) -> \
									   $("errorid", ERR_OK) -> \
									   $("error", "OK")); \
		value &resname = *__rpc_res;
						
// ==========================================================================
// DESTRUCTOR rpchandler
// ==========================================================================
rpchandler::~rpchandler (void)
{
}

// ==========================================================================
// METHOD rpchandler::handle
// ==========================================================================
value *rpchandler::handle (const value &v, uid_t uid, const string &origin)
{
	DEBUG.newSession ();
	DEBUG.storeFile ("rpc","in", v);
	statstring cmd = v["header"]["command"];
	statstring sessid = v["header"]["session_id"];
	
	caseselector (cmd)
	{
		incaseof ("bind") : return bind (v, uid, origin);
		incaseof ("getlanguages") : return getlanguages (v);
		defaultcase : break;
	}
	
	if (! sessid)
	{
		return $("header", $("errorid", ERR_SESSION_INVALID) ->
						   $("error", "No session provided"));
	}
	
	coresession *cs = sdb.get (sessid);
	if (! cs)
	{
		return $("header", $("errorid", ERR_SESSION_INVALID) ->
						   $("error", "Unknown session"));
	}
	
	if (cmd == "logout")
	{
		sdb.remove (cs);
		return $("header", $("errorid",ERR_OK) -> $("error","OK"));
	}
	
	value *res = call (cmd, v, *cs);
	sdb.release (cs);
	
	DEBUG.storeFile ("rpc","out", *res);
	return res;
}

// ==========================================================================
// METHOD rpchandler::call
// ==========================================================================
value *rpchandler::call (const statstring &cmd, const value &v,
						 coresession &cs)
{
	if (! handler.exists (cmd))
	{
		// Return an error if we couldn't hand off the call.
		return $("header",
					$("session_id", cs.id) ->
					$("errorid", ERR_RPC_INVALIDCMD) ->
					$("error", "Invalid command"));
	}
	
	return handler.call (cmd, v, cs);
}

// ==========================================================================
// METHOD rpchandler::bind
// ==========================================================================
value *rpchandler::bind (const value &v, uid_t uid, const string &origin)
{
	const value &vbody = v["body"];
	coresession *cs = NULL;
	value meta = $("origin", origin);
	
	// Create a session object
	try
	{
		cs = sdb.create (meta);
	}
	catch (exception e)
	{
		CORE->log (log::error, "rpc", "Exception caught while trying to "
				   "bind session: %S" %format (e.description));
				   
		return $("header",
					$("errorid", ERR_UNKNOWN) ->
					$("error", "Error binding session"));
	}
	
	// /body/password is the canonical place to look for a password,
	// but the old protocol used /body/data/id for some stupid reason.
	string password = vbody.exists ("password") ? vbody["password"]
												: vbody["data"]["id"];
	
	// Try a login with provided credentials
	if (cs->login (vbody["id"], password, (uid==0)))
	{
		string sid = cs->id;
		sdb.release (cs);
		CORE->log (log::debug, "rpc", "Login: success");
		
		return $("header",
					$("session_id", sid) ->
					$("errorid", ERR_OK) ->
					$("error", "Login succeeded")
				);
	}
	
	// Ok, find out if we can use pre-validation for the uid
	value pw = core.userdb.getpwuid (uid);
	if (pw)
	{
		string username = pw["username"];
		
		CORE->log (log::info, "rpc", "Login with pre-validated user %u "
				   "(%s)" %format ((unsigned int) uid, username));
		
		if (username == "root") username = "openadmin"; // FIXME: HAX
		
		if (cs->userlogin (username))
		{
			string sid = cs->id;
			sdb.release (cs);
			DEBUG.newSession ();
			CORE->log (log::debug, "rpc", "Login: success");
			
			return $("header",
						$("session_id", sid) ->
						$("errorid", ERR_OK) ->
						$("error", "Login succeeded")
					);
		}
	}
	
	// It definitely didn't happen. Propagate the coresession error.
	returnclass (value) res retain;
	copysessionerror (*cs, res);
	sdb.remove (cs);
	return &res;
}

// ==========================================================================
// METHOD rpchandler::getlanguages
// ==========================================================================
value *rpchandler::getlanguages (const value &v)
{
	coresession *cs = NULL;
	value meta;
	
	// Create a session object
	try
	{
		cs = sdb.create (meta);
	}
	catch (exception e)
	{
		CORE->log (log::error, "rpc", "Exception caught while trying to "
				   "get languages: %S" %format (e.description));
				   
		return $("header",
					$("errorid", ERR_UNKNOWN) ->
					$("error", "Error binding session"));
	}

	returnclass (value) res retain;
	res = $("header",
				$("errorid", ERR_OK) ->
				$("error", "OK")
		   ) ->
		  $("body",
		  		$("data", cs->getlanguages ())
		   );
	
	sdb.remove (cs);
	return &res;
}

// ==========================================================================
// METHOD rpchandler::ping
// ==========================================================================
value *rpchandler::ping (const value &v, coresession &cs)
{
	RPCRETURN (res);
	return &res;
}

// ==========================================================================
// METHOD rpchandler::createObject
// ==========================================================================
value *rpchandler::createObject (const value &v, coresession &cs)
{
	RPCRETURN (res);
	const value &vbody = v["body"];
	statstring in_class = vbody["classid"];
	statstring in_parent = vbody["parentid"];
	const value &in_data = vbody["data"];
	statstring in_id = vbody["objectid"];
    statstring in_immediate = vbody["immediate"];
	string objid;

	cs.mlockw ("createObject");

		objid = cs.createObject (in_parent, in_class, in_data, in_id, in_immediate);
		
		if (! objid)
		{
			copysessionerror (cs, res);
		}
		
	cs.munlock ();
	res["body"]["data"]["objid"] = objid;
	return &res;
}

// ==========================================================================
// METHOD rpchandler::deleteObject
// ==========================================================================
value *rpchandler::deleteObject (const value &v, coresession &cs)
{
	RPCRETURN (res);
	statstring in_class = v["body"]["classid"];
	statstring in_id = v["body"]["objectid"];
	statstring in_parent = v["body"]["parentid"];
	statstring in_immediate = v["body"]["immediate"];	
	
	cs.mlockw ("deleteObject");
	
		if (! cs.deleteObject (in_parent, in_class, in_id, in_immediate))
		{
			copysessionerror (cs, res);
		}
	
	cs.munlock ();
	return &res;
}

// ==========================================================================
// METHOD rpchandler::updateObject
// ==========================================================================
value *rpchandler::updateObject (const value &v, coresession &cs)
{
	RPCRETURN (res);
	const value &vbody = v["body"];
	statstring in_class = vbody["classid"].sval();
	statstring in_id = vbody["objectid"].sval();
	statstring in_parent = vbody["parentid"].sval();
	statstring in_immediate = vbody["immediate"];	
	const value &in_data = vbody["data"];

	cs.mlockw ("updateObject");
	
		if (! cs.updateObject (in_parent, in_class, in_id, in_data, in_immediate))
		{
			copysessionerror (cs, res);
		}
		
	cs.munlock ();
	return &res;
}

// ==========================================================================
// METHOD rpchandler::chown
// ==========================================================================
value *rpchandler::chown (const value &v, coresession &cs)
{
	RPCRETURN (res);
	statstring in_object = v["body"]["objectid"];
	statstring in_user = v["body"]["userid"];
	
	cs.mlockw ("chown");
	
		if (! cs.chown (in_object, in_user))
		{
			copysessionerror (cs, res);
		}
	
	cs.munlock ();
	return &res;
}

// ==========================================================================
// METHOD rpchandler::classinfo
// ==========================================================================
value *rpchandler::classinfo (const value &v, coresession &cs)
{
	RPCRETURN (res);
	statstring in_classid = v["body"]["classid"];
	value tval;
	
	cs.mlockr ();
	
		tval = cs.getclassinfo (in_classid);
		if (! tval)
		{
			copysessionerror (cs, res);
		}
		else
		{
			res["body"]["data"] = tval;
		}
		
	cs.munlock ();
	return &res;
}

// ==========================================================================
// METHOD rpchandler::classxml
// ==========================================================================
value *rpchandler::classxml (const value &v, coresession &cs)
{
	RPCRETURN (res);
	statstring in_classid = v["body"]["classid"];
	coremodule *m = cs.getmoduleforclass (in_classid);
	
	if (! m)
	{
		copysessionerror (cs, res);
	}
	else
	{
		res["body"]["data"] = m->meta.toxml ();
	}
	return &res;
}

// ==========================================================================
// METHOD rpchandler::callmethod
// ==========================================================================
value *rpchandler::callmethod (const value &v, coresession &cs)
{
	RPCRETURN (res);
	const value &vbody = v["body"];
	statstring in_method = vbody["method"];
	statstring in_class = vbody["classid"];
	statstring in_parentid = vbody["parentid"];
	statstring in_id = vbody["objectid"];
	value argv = vbody["argv"];
	value tval;
	
	cs.mlockw ("callmethod");
		tval = cs.callmethod (in_parentid, in_class, in_id, in_method, argv);
	cs.munlock ();
	
	res["body"]["data"] = tval;
	return &res;
}

// ==========================================================================
// METHOD rpchandler::getrecord
// ==========================================================================
value *rpchandler::getrecord (const value &v, coresession &cs)
{
	RPCRETURN (res);
	const value &vbody = v["body"];
	value &dres = res["body"]["data"];
	statstring in_parentid = vbody["parentid"];
	statstring in_class = vbody["classid"];
	statstring in_objectid = vbody["objectid"];
	
	cs.mlockr ();
	
		dres["object"] = cs.getobject (in_parentid, in_class, in_objectid);
		if (! dres["object"])
		{
			copysessionerror (cs, res);
		}
		
	cs.munlock ();
	return &res;
}

// ==========================================================================
// METHOD rpchandler::getrecords
// ==========================================================================
value *rpchandler::getrecords (const value &v, coresession &cs)
{
	RPCRETURN (res);
	const value &vbody = v["body"];
	statstring in_parentid = vbody["parentid"];
	statstring in_class = vbody["classid"];
	int offset = 0;
	int count = -1;
	value &dres = res["body"]["data"];
	
	if (vbody.exists ("offset"))
	{
		offset = vbody["offset"];
		count = vbody["count"];
	}
	
	cs.mlockr ();
	
		dres = cs.listObjects (in_parentid, in_class, offset, count);
		dres["info"]["total"] = dres[0].count ();
		if (vbody.exists ("whitelist"))
		{
			value in_whitel = vbody["whitelist"];
			cs.applyFieldWhiteLabel (dres, in_whitel);
		}
	
	cs.munlock ();
	return &res;
}

// ==========================================================================
// METHOD rpchandler::getparent
// ==========================================================================
value *rpchandler::getparent (const value &v, coresession &cs)
{
	RPCRETURN (res);
	statstring in_objectid = v["body"]["objectid"];
	statstring resid;
	
	cs.mlockr ();
		resid = cs.upcontext (in_objectid);
	cs.munlock ();
	
	res["body"]["data"]["newparent"] = resid;
	return &res;	
}

// ==========================================================================
// METHOD rpchandler::getworld
// ==========================================================================
value *rpchandler::getworld (const value &v, coresession &cs)
{
	RPCRETURN (res);
	cs.mlockr ();
	
	res["body"]["data"]["body"] = $("classes", cs.getworld()) ->
								  $("modules", cs.listmodules());
	
	cs.munlock ();
	return &res;
}

// ==========================================================================
// METHOD rpchandler::listparamsformethod
// ==========================================================================
value *rpchandler::listparamsformethod (const value &v, coresession &cs)
{
	RPCRETURN (res);
	const value &vbody = v["body"];
	statstring in_method = vbody["method"];
	statstring in_class = vbody["classid"];
	statstring in_id = vbody["id"];
	statstring in_parentid = vbody["parentid"];
	
	cs.mlockr ();
	
		res["body"]["data"] =
			cs.listparamsformethod (in_parentid, in_class, in_id, in_method);
			
	cs.munlock ();
	return &res;
}

// ==========================================================================
// METHOD rpchandler::listmodules
// ==========================================================================
value *rpchandler::listmodules (const value &v, coresession &cs)
{
	RPCRETURN (res);
	res["body"]["data"]["modules"] = cs.listmodules ();
	return &res;
}

// ==========================================================================
// METHOD rpchandler::listclasses
// ==========================================================================
value *rpchandler::listclasses (const value &v, coresession &cs)
{
	RPCRETURN (res);
	res["body"]["data"]["classes"] = cs.listclasses ();
	return &res;
}

// ==========================================================================
// METHOD rpchandler::copysessionerror
// ==========================================================================
void rpchandler::copysessionerror (coresession &cs, value &into)
{
	string txt = "(%[code]04i) %[message]s" %format (cs.error());
	into["header"] = $("errorid", cs.error()["code"]) ->
					 $("error", txt);
}

// ==========================================================================
// METHOD rpchandler::seterror
// ==========================================================================
void rpchandler::seterror (int errcode, value &into)
{
	statstring mid = "%04x" %format (errcode);
	into["header"] = $("errorid", errcode) ->
					 $("error", CORE->rsrc["errors"]["en"][mid]);
}
