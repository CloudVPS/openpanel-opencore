#include <grace/value.h>
#include "rpc.h"
#include "error.h"
#include "opencore.h"
#include "debug.h"

// ==========================================================================
// CONSTRUCTOR RPCHandler
// ==========================================================================
RPCHandler::RPCHandler (SessionDB *s) : handler (this), sdb (*s)
{
	#define AddCommand(foo) handler.setcmd ( #foo, &RPCHandler:: foo )
	#define BindCommand(foo,bar) handler.setcmd (#foo, &RPCHandler:: bar )
	
	BindCommand (create, createObject);
	BindCommand (delete, deleteObject);
	BindCommand (update, updateObject);
	AddCommand  (ping);
	AddCommand  (chown);
	BindCommand (classinfo, classInfo);
	BindCommand (classxml, classXML);
	BindCommand (callmethod, callMethod);
	BindCommand (getrecord, getRecord);
	BindCommand (getrecords, getRecords);
	BindCommand (getparent, getParent);
	BindCommand (getworld, getWorld);
	BindCommand (listparamsformethod, listParamsForMethod);
	BindCommand (listmodules, listModules);
	BindCommand (listclasses, listClasses);
	
	#undef AddCommand
	#undef BindCommand
}

#define RPCRETURN(resname) \
		value *__rpc_res = $("header", $("session_id", cs.id) -> \
									   $("errorid", ERR_OK) -> \
									   $("error", "OK")); \
		value &resname = *__rpc_res;
						
// ==========================================================================
// DESTRUCTOR RPCHandler
// ==========================================================================
RPCHandler::~RPCHandler (void)
{
}

// ==========================================================================
// METHOD RPCHandler::handle
// ==========================================================================
value *RPCHandler::handle (const value &v, uid_t uid, const string &origin)
{
	DEBUG.newSession ();
	DEBUG.storeFile ("RPCHandler","in", v,"handle");
	statstring cmd = v["header"]["command"];
	statstring sessid = v["header"]["session_id"];
	
	caseselector (cmd)
	{
		incaseof ("bind") : return bind (v, uid, origin);
		incaseof ("getlanguages") : return getLanguages (v);
		defaultcase : break;
	}
	
	if (! sessid)
	{
		return $("header", $("errorid", ERR_SESSION_INVALID) ->
						   $("error", "No session provided"));
	}
	
	CoreSession *cs = sdb.get (sessid);
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
	
	DEBUG.storeFile ("RPCHandler","out", *res, "handle");
	return res;
}

// ==========================================================================
// METHOD RPCHandler::call
// ==========================================================================
value *RPCHandler::call (const statstring &cmd, const value &v,
						 CoreSession &cs)
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
// METHOD RPCHandler::bind
// ==========================================================================
value *RPCHandler::bind (const value &v, uid_t uid, const string &origin)
{
	const value &vbody = v["body"];
	CoreSession *cs = NULL;
	value meta = $("origin", origin);
	
	// Create a session object
	try
	{
		cs = sdb.create (meta);
	}
	catch (exception e)
	{
		log::write (log::error, "RPC", "Exception caught while trying to "
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
		log::write (log::debug, "RPC", "Login: success");
		
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
		
		log::write (log::info, "RPC", "Login with pre-validated user %u "
				    "(%s)" %format ((unsigned int) uid, username));
		
		if (username == "root") username = "openadmin"; // FIXME: HAX
		
		if (cs->userLogin (username))
		{
			string sid = cs->id;
			sdb.release (cs);
			DEBUG.newSession ();
			log::write (log::debug, "RPC", "Login: success");
			
			return $("header",
						$("session_id", sid) ->
						$("errorid", ERR_OK) ->
						$("error", "Login succeeded")
					);
		}
	}
	
	// It definitely didn't happen. Propagate the CoreSession error.
	returnclass (value) res retain;
	copySessionError (*cs, res);
	sdb.remove (cs);
	return &res;
}

// ==========================================================================
// METHOD RPCHandler::getLanguages
// ==========================================================================
value *RPCHandler::getLanguages (const value &v)
{
	CoreSession *cs = NULL;
	value meta;
	
	// Create a session object
	try
	{
		cs = sdb.create (meta);
	}
	catch (exception e)
	{
		log::write (log::error, "RPC", "Exception caught while trying to "
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
		  		$("data", cs->getLanguages ())
		   );
	
	sdb.remove (cs);
	return &res;
}

// ==========================================================================
// METHOD RPCHandler::ping
// ==========================================================================
value *RPCHandler::ping (const value &v, CoreSession &cs)
{
	RPCRETURN (res);
	return &res;
}

// ==========================================================================
// METHOD RPCHandler::createObject
// ==========================================================================
value *RPCHandler::createObject (const value &v, CoreSession &cs)
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

		objid = cs.createObject (in_parent, in_class, in_data,
								 in_id, in_immediate);
		
		if (! objid)
		{
			copySessionError (cs, res);
		}
		
	cs.munlock ();
	res["body"]["data"]["objid"] = objid;
	return &res;
}

// ==========================================================================
// METHOD RPCHandler::deleteObject
// ==========================================================================
value *RPCHandler::deleteObject (const value &v, CoreSession &cs)
{
	RPCRETURN (res);
	statstring in_class = v["body"]["classid"];
	statstring in_id = v["body"]["objectid"];
	statstring in_parent = v["body"]["parentid"];
	statstring in_immediate = v["body"]["immediate"];	
	
	cs.mlockw ("deleteObject");
	
		if (! cs.deleteObject (in_parent, in_class, in_id, in_immediate))
		{
			copySessionError (cs, res);
		}
	
	cs.munlock ();
	return &res;
}

// ==========================================================================
// METHOD RPCHandler::updateObject
// ==========================================================================
value *RPCHandler::updateObject (const value &v, CoreSession &cs)
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
			copySessionError (cs, res);
		}
		
	cs.munlock ();
	return &res;
}

// ==========================================================================
// METHOD RPCHandler::chown
// ==========================================================================
value *RPCHandler::chown (const value &v, CoreSession &cs)
{
	RPCRETURN (res);
	statstring in_object = v["body"]["objectid"];
	statstring in_user = v["body"]["userid"];
	
	cs.mlockw ("chown");
	
		if (! cs.chown (in_object, in_user))
		{
			copySessionError (cs, res);
		}
	
	cs.munlock ();
	return &res;
}

// ==========================================================================
// METHOD RPCHandler::classInfo
// ==========================================================================
value *RPCHandler::classInfo (const value &v, CoreSession &cs)
{
	RPCRETURN (res);
	statstring in_classid = v["body"]["classid"];
	value tval;
	
	cs.mlockr ();
	
		tval = cs.getClassInfo (in_classid);
		if (! tval)
		{
			copySessionError (cs, res);
		}
		else
		{
			res["body"]["data"] = tval;
		}
		
	cs.munlock ();
	return &res;
}

// ==========================================================================
// METHOD RPCHandler::classXML
// ==========================================================================
value *RPCHandler::classXML (const value &v, CoreSession &cs)
{
	RPCRETURN (res);
	statstring in_classid = v["body"]["classid"];
	CoreModule *m = cs.getModuleForClass (in_classid);
	
	if (! m)
	{
		copySessionError (cs, res);
	}
	else
	{
		res["body"]["data"] = m->meta.toxml ();
	}
	return &res;
}

// ==========================================================================
// METHOD RPCHandler::callmethod
// ==========================================================================
value *RPCHandler::callMethod (const value &v, CoreSession &cs)
{
	RPCRETURN (res);
	const value &vbody = v["body"];
	statstring in_method = vbody["method"];
	statstring in_class = vbody["classid"];
	statstring in_parentid = vbody["parentid"];
	statstring in_id = vbody["objectid"];
	value argv = vbody["argv"];
	value tval;
	
	cs.mlockw ("callMethod");
		tval = cs.callMethod (in_parentid, in_class, in_id, in_method, argv);
	cs.munlock ();
	
	res["body"]["data"] = tval;
	return &res;
}

// ==========================================================================
// METHOD RPCHandler::getRecord
// ==========================================================================
value *RPCHandler::getRecord (const value &v, CoreSession &cs)
{
	RPCRETURN (res);
	const value &vbody = v["body"];
	value &dres = res["body"]["data"];
	statstring in_parentid = vbody["parentid"];
	statstring in_class = vbody["classid"];
	statstring in_objectid = vbody["objectid"];
	
	cs.mlockr ();
	
		dres["object"] = cs.getObject (in_parentid, in_class, in_objectid);
		if (! dres["object"])
		{
			copySessionError (cs, res);
		}
		
	cs.munlock ();
	return &res;
}

// ==========================================================================
// METHOD RPCHandler::getRecords
// ==========================================================================
value *RPCHandler::getRecords (const value &v, CoreSession &cs)
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
			cs.applyFieldWhiteList (dres, in_whitel);
		}
	
	cs.munlock ();
	return &res;
}

// ==========================================================================
// METHOD RPCHandler::getParent
// ==========================================================================
value *RPCHandler::getParent (const value &v, CoreSession &cs)
{
	RPCRETURN (res);
	statstring in_objectid = v["body"]["objectid"];
	statstring resid;
	
	cs.mlockr ();
		resid = cs.findParent (in_objectid);
	cs.munlock ();
	
	res["body"]["data"]["newparent"] = resid;
	return &res;	
}

// ==========================================================================
// METHOD RPCHandler::getWorld
// ==========================================================================
value *RPCHandler::getWorld (const value &v, CoreSession &cs)
{
	RPCRETURN (res);
	cs.mlockr ();
	
	res["body"]["data"]["body"] = $("classes", cs.getWorld()) ->
								  $("modules", cs.listModules());
	
	cs.munlock ();
	return &res;
}

// ==========================================================================
// METHOD RPCHandler::listParamsForMethod
// ==========================================================================
value *RPCHandler::listParamsForMethod (const value &v, CoreSession &cs)
{
	RPCRETURN (res);
	const value &vbody = v["body"];
	statstring in_method = vbody["method"];
	statstring in_class = vbody["classid"];
	statstring in_id = vbody["id"];
	statstring in_parentid = vbody["parentid"];
	
	cs.mlockr ();
	
		res["body"]["data"] =
			cs.listParamsForMethod (in_parentid, in_class, in_id, in_method);
			
	cs.munlock ();
	return &res;
}

// ==========================================================================
// METHOD RPCHandler::listModules
// ==========================================================================
value *RPCHandler::listModules (const value &v, CoreSession &cs)
{
	RPCRETURN (res);
	res["body"]["data"]["modules"] = cs.listModules ();
	return &res;
}

// ==========================================================================
// METHOD RPCHandler::listClasses
// ==========================================================================
value *RPCHandler::listClasses (const value &v, CoreSession &cs)
{
	RPCRETURN (res);
	res["body"]["data"]["classes"] = cs.listClasses ();
	return &res;
}

// ==========================================================================
// METHOD RPCHandler::copySessionError
// ==========================================================================
void RPCHandler::copySessionError (CoreSession &cs, value &into)
{
	string txt = "(%[code]04i) %[message]s" %format (cs.error());
	into["header"] = $("errorid", cs.error()["code"]) ->
					 $("error", txt);
}

// ==========================================================================
// METHOD RPCHandler::setError
// ==========================================================================
void RPCHandler::setError (int errcode, value &into)
{
	statstring mid = "%04x" %format (errcode);
	into["header"] = $("errorid", errcode) ->
					 $("error", CORE->rsrc["errors"]["en"][mid]);
}
