// --------------------------------------------------------------------------
// OpenPanel - The Open Source Control Panel
// Copyright (c) 2006-2007 PanelSix
//
// This software and its source code are subject to version 2 of the
// GNU General Public License. Please be aware that use of the OpenPanel
// and PanelSix trademarks and the IconBase.com iconset may be subject
// to additional restrictions. For more information on these restrictions
// and for a copy of version 2 of the GNU General Public License, please
// visit the Legal and Privacy Information section of the OpenPanel
// website on http://www.openpanel.com/
// --------------------------------------------------------------------------

#include <grace/md5.h>
#include <assert.h>
#include "session.h"
#include "moduledb.h"
#include "error.h"
#include "opencore.h"
#include "debug.h"
#include "alerts.h"

const char *MD5SALT_VALID = "./abcdefghijklmnopqrstuvwxyz"
							"ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

// ==========================================================================
// CONSTRUCTOR sessiondb
// ==========================================================================
sessiondb::sessiondb (moduledb &pmdb)
	: mdb (pmdb)
{
	first = last = NULL;
	lck.o = 0;
}

// ==========================================================================
// DESTRUCTOR sessiondb
// ==========================================================================
sessiondb::~sessiondb (void)
{
}

// ==========================================================================
// METHOD sessiondb::get
// ==========================================================================
coresession *sessiondb::get (const statstring &id)
{
	coresession *res;
	
	sharedsection (lck)
	{
		lck = 0;
		res = find (id);
		if (res) res->inuse++;
	}
	
	if (res) CORE->log (log::debug, "session", "SDB Open <%S>" %format (id));
	return res;
}

// ==========================================================================
// METHOD sessiondb::create
// ==========================================================================
coresession *sessiondb::create (const value &meta)
{
	coresession *s;
	statstring id = strutil::uuid();
	
	exclusivesection (lck)
	{
		try
		{
			lck = 1;
			s = demand (id);
			s->meta = meta;
		}
		catch (...)
		{
			CORE->log (log::error, "session", "Software bug in session "
					   "database, exception caught");
		}
	}
	CORE->log (log::debug, "session", "SDB Create <%S>" %format (id));
	
	return s;
}

// ==========================================================================
// METHOD sessiondb::release
// ==========================================================================
void sessiondb::release (coresession *s)
{
	sharedsection (lck)
	{
		lck = 0;
		s->heartbeat = kernel.time.now();
		if (s->inuse > 0) s->inuse--;
		CORE->log (log::debug, "session", "SDB Release <%S> "
				   "inuse <%i>" %format (s->id, s->inuse));
	}
}

// ==========================================================================
// METHOD sessiondb::remove
// ==========================================================================
void sessiondb::remove (coresession *s)
{
	if (! first) return;
	
	exclusivesection (lck)
	{
		try
		{
			lck = 1;
			coresession *crsr = first;
			coresession *parent = NULL;
			
			CORE->log (log::debug, "session", "SDB Remove <%S>" %format (s->id));
						
			while (crsr)
			{
				if (crsr->id == s->id)
				{
					if (parent)
					{
						if (parent->lower == s) parent->lower = NULL;
						else if (parent->higher == s) parent->higher = NULL;
						else
						{
							breaksection throw (sessionUnlinkException());
						}
					}
					
					if (s->prev)
					{
						if (s->next)
						{
							s->prev->next = s->next;
							s->next->prev = s->prev;
						}
						else
						{
							s->prev->next = NULL;
							last = s->prev;
						}
					}
					else
					{
						if (s->next)
						{
							s->next->prev = NULL;
							first = s->next;
						}
						else
						{
							first = last = NULL;
							s->higher = s->lower = NULL;
						}
					}
					
					crsr = first;
					while (crsr)
					{
						crsr->lower = crsr->higher = NULL;
						if (crsr != first) link (crsr);
						crsr = crsr->next;
					}
					
					delete s;
					breaksection return;
				}
				
				parent = crsr;
				
				if (crsr->id.key() <= s->id.key()) crsr = crsr->higher;
				else crsr = crsr->lower;
			}
		}
		catch (...)
		{
			CORE->log (log::error, "session", "Software bug in session "
					   "database, exception caught.");
		}
	}
}

// ==========================================================================
// METHOD sessiondb::find
// ==========================================================================
coresession *sessiondb::find (const statstring &key, bool noreport)
{
	if (! first)
	{
		CORE->log (log::debug, "session", "Find on empty list");
		return NULL;
	}
	
	if (! key)
	{
		CORE->log (log::warning, "session", "Find on empty key");
	}
	
	coresession *crsr = first;
	while (crsr)
	{
		assert (crsr->lower != crsr);
		assert (crsr->higher != crsr);
		
		if (crsr->id == key) return crsr;
		if (crsr->id.key() <= key.key()) crsr = crsr->higher;
		else crsr = crsr->lower;
	}
	
	if (! noreport)
	{
		CORE->log (log::warning, "session", "Session <%S> not "
				   "found" %format (key));
	}
	return NULL;
}

// ==========================================================================
// METHOD sessiondb::demand
// ==========================================================================
coresession *sessiondb::demand (const statstring &key)
{
	coresession *res;
	
	res = find (key, true);
	if (res)
	{
		res->heartbeat = kernel.time.now();
		res->inuse++;
	}
	else
	{
		res = new coresession (key, mdb);
		if (! res)
		{
			return NULL;
		}
		if (! last)
		{
			first = last = res;
			res->prev = res->next = NULL;
			res->higher = res->lower = NULL;
			return res;
		}
		
		res->prev = last;
		last->next = res;
		last = res;
		res->inuse = 1;
	
		link (res);
	}
	return res;
}

// ==========================================================================
// METHOD sessiondb::link
// ==========================================================================
void sessiondb::link (coresession *cs)
{
	assert (lck.o == 1);
	coresession *crsr = first;
	while (crsr)
	{
		assert (crsr != cs);
		if (crsr->id.key() <= cs->id.key())
		{
			if (crsr->higher) crsr = crsr->higher;
			else
			{
				crsr->higher = cs;
				return;
			}
		}
		else
		{
			if (crsr->lower) crsr = crsr->lower;
			else
			{
				crsr->lower = cs;
				return;
			}
		}
	}
	return;
}

// ==========================================================================
// METHOD sessiondb::exists
// ==========================================================================
bool sessiondb::exists (const statstring &id)
{
	coresession *s;

	sharedsection (lck)
	{
		lck = 0;
		s = find (id);
	}
	
	if (s == NULL) return false;
	return true;
}

// ==========================================================================
// METHOD sessiondb::list
// ==========================================================================
value *sessiondb::list (void)
{
	returnclass (value) res retain;
	
	sharedsection (lck)
	{
		lck = 0;
		coresession *c = first;
		while (c)
		{
			res[c->id] = $("heartbeat", (unsigned int) c->heartbeat) ->
						 $("inuse", c->inuse) ->
						 $merge (c->meta);

			c = c->next;
		}
	}
	return &res;
}

// ==========================================================================
// METHOD sessiondb::expire
// ==========================================================================
int sessiondb::expire (void)
{
	int result = 0;
	time_t cutoff = kernel.time.now() - 600;
	exclusivesection (lck)
	{
		try
		{
			lck = 1;
			coresession *s, *ns;
			s = first;
			while (s)
			{
				ns = s->next;
				if ((! s->inuse) && (s->heartbeat < cutoff))
				{
					result++;
					breaksection remove (s);
				}
				else
				{
					CORE->log (log::debug, "expire", "Passing <%S> inuse=<%i> "
							   "heartbeat=%i cutoff=%i" %format (s->id,
							   		s->inuse,
							   		(int) s->heartbeat,
							   		(int) cutoff));
				}
					
				s = ns;
			}
		}
		catch (...)
		{
			CORE->log (log::error, "session", "Software bug in session "
					   "database expiration, exception caught.");
		}
	}
	
	return result;
}

// ==========================================================================
// CONSTRUCTOR coresession
// ==========================================================================
coresession::coresession (const statstring &myid, class moduledb &pmdb)
	: id (myid), mdb (pmdb)
{
	next = prev = higher = lower = NULL;
	heartbeat = kernel.time.now();
	inuse = 0;
	if (! db.init ())
	{
		CORE->log (log::error, "session", "Error initializing the sqlite3 "
				   "database");
		throw (sqliteInitException());
	}
}

// ==========================================================================
// DESTRUCTOR coresession
// ==========================================================================
coresession::~coresession (void)
{
	db.deinit();
}

// ==========================================================================
/// METHOD coresession::login
// ==========================================================================
bool coresession::login (const string &user, const string &pass, bool superuser)
{
	bool res;
	
	if (superuser && user[0]=='!')
	{
        db.iamgod();
        CORE->log (log::info, "session", "Login special <%S> in godmode"
				   %format (user, meta["origin"]));
        return true;
    }

	res = db.login (user, pass);
	if (! res)
	{
		if (! user) return false;

		seterror (ERR_DBMANAGER_LOGINFAIL);
		CORE->log (log::error, "session", "Failed login user "
				   "<%S> (%S)" %format (user, db.getlasterror()));
		
	}
	else
	{
		CORE->log (log::info, "session", "Login user=<%S> origin=<%S>"
				   %format (user, meta["origin"]));
		
		meta["user"] = user;
	}
	
	return res;
}

// ==========================================================================
/// METHOD coresession::getcredentials
// ==========================================================================
void coresession::getcredentials (value &creds)
{
    db.getcredentials (creds);
}

// ==========================================================================
/// METHOD coresession::setcredentials
// ==========================================================================
void coresession::setcredentials (const value &creds)
{
    db.setcredentials (creds);
}



// ==========================================================================
/// METHOD coresession::userlogin
// ==========================================================================
bool coresession::userlogin (const string &user)
{
	bool res;
	
	res = db.userlogin (user);
	if (! res)
	{
		CORE->log (log::error, "session", "Failed login "
				   "user <%S> (%S)" %format (user, db.getlasterror()));
	}
	else
	{
		CORE->log (log::info, "session", "Login user=<%S> origin=<%S>"
				   %format (user, meta["origin"]));
				   
		meta["user"] = user;
	}
	
	// FIXME: is this ok?
	seterror (db.getlasterrorcode(), db.getlasterror());
	return res;
}

// ==========================================================================
// METHOD coresession::createobject
// ==========================================================================
string *coresession::createobject (const statstring &parentid,
								   const statstring &ofclass,
								   const value &_withparam,
								   const statstring &_withid,
								   bool immediate)
{
	string uuid;
	string err; // normalization error text.
	value nparam; // normalized parameters.
	value withparam = _withparam;
	statstring withid = _withid;
	statstring owner;
	if (withparam.exists ("owner"))
	{
		if (withparam["owner"] != "Select ...")
		{
			owner = withparam["owner"];
		}
		withparam.rmval ("owner");
	}
	
	if (withid) withparam["id"] = withid;
	
	DEBUG.storefile ("session", "create-param", withparam);

	// Catch internal classes.
	if (mdb.isinternalclass (ofclass))
	{
		internalclass &cl = mdb.geticlass (ofclass);
		string *r = cl.createobject (this, parentid, withparam, withid);
		if (! r) seterror (ERR_ICLASS, cl.error());
		return r;
	}

	// Check for the class.
	if (! mdb.classexists (ofclass))
	{
		CORE->log (log::error, "session", "Create request for class <%S> "
				   "which does not exist" %format (ofclass));
		seterror (ERR_SESSION_CLASS_UNKNOWN);
		return NULL;
	}
	
	// Resolve to a coreclass reference.
	coreclass &cl = mdb.getclass (ofclass);
	
	// Normalize singleton instances
	if (cl.singleton) withid = cl.singleton;
	
	// Make sure the indexing makes sense.
	if ( (! cl.manualindex) && (withid) )
	{
		CORE->log (log::error, "session", "Create request with manual id "
				   "on class <%S> with autoindex" %format (ofclass));
		seterror (ERR_SESSION_INDEX);
		return NULL;
	}
	
	if (cl.manualindex && (! withid))
	{
		CORE->log (log::error, "session", "Create request with no required "
				   "manual id on class <%S>" %format (ofclass));
		seterror (ERR_SESSION_NOINDEX);
		return NULL;
	}
	
	if (cl.parentrealm)
	{
		value vparent;
		string pmid;
		
		if (! db.fetchobject (vparent, parentid, /* formodule */ false))
		{
			CORE->log (log::error, "session", "Lookup failed for "
					   "fetchobject on parentid=<%S>" %format (parentid));
			seterror (ERR_SESSION_PARENTREALM);
			return NULL;
		}
		
		pmid = vparent[0]["metaid"];
		if (! pmid)
		{
			CORE->log (log::error, "session", "Error getting metaid "
					   "from resolved parent object: %J" %format (vparent));
			seterror (ERR_SESSION_PARENTREALM, "Error finding metaid");
			return NULL;
		}
		
		if (cl.parentrealm == "domainsuffix")
		{
			if (! withid) withid = pmid;
			else if (withid != pmid)
			{
				if (withid.strlen() > pmid.strlen())
				{
					string theirsuffix = withid.sval().right (pmid.strlen ());
					if (theirsuffix != pmid)
					{
						withid = "%s.%s" %format (withid, pmid);
					}
					else
					{
						string t = withid.sval().copyuntil (pmid);
						if (t[-1] != '.')
						{
							withid = "%s.%s" %format (withid, pmid);
						}
					}
				}
				else
				{
					withid = "%s.%s" %format (withid, pmid);
				}
			}
		}
		else if (cl.parentrealm == "emailsuffix")
		{
			if (withid.sval().strchr ('@') < 0)
			{
				withid = "%s@%s" %format (withid, pmid);
			}
		}
	}
	
	DEBUG.storefile ("session", "normalize-pre", withparam, "createobject");
	
	if (! cl.normalize (withparam, err))
	{
		CORE->log (log::error, "session", "Input data validation "
				   "error: %S " %format (err));
		seterror (ERR_SESSION_VALIDATION, err);
		return NULL;
	}

	DEBUG.storefile ("session", "normalize-post", withparam, "createobject");
	
	// Handle any module-bound crypting voodoo on the fields.
	if (! handlecrypts (parentid, ofclass, withid, withparam))
	{
		CORE->log (log::error, "session", "Create failed due to crypt error");
		// handlecrypts already sets the error
		return NULL;
	}

	// create the object in the database, it will be marked as wanted.
	uuid = db.createobject(parentid, withparam, ofclass, withid, false, immediate);
	if (! uuid)
	{
		CORE->log (log::error, "session", "Database error: %s"
				   %format (db.getlasterror()));
		seterror (db.getlasterrorcode(), db.getlasterror());
		return NULL;
	}
	
	if (owner)
	{
		if (! chown (uuid, owner))
		{
			db.reportfailure (uuid);
			seterror (ERR_SESSION_CHOWN);
			return NULL;
		}
	}

	if (immediate)
	{
	    return new (memory::retainable::onstack) string (uuid);
	}

	value parm;
	value ctx;
	
	// Get the parameters for the module action.
	if (! db.fetchobject (parm, uuid, true /* formodule */))
	{
		CORE->log (log::critical, "session", "Database failure getting object-"
				   "related data for <%S>: %S" %format (uuid,
				   db.getlasterror()));
				   
		ALERT->alert ("Session error on object-related data\n"
					  "uuid=<%S> error=<%S>" %format (uuid,
					  db.getlasterror()));
		return NULL;
	}

	// Bring them into a larger context.
	ctx = $("OpenCORE:Context", parm[0].id()) ->
		  $("OpenCORE:Session",
		  		$("sessionid", id.sval()) ->
		  		$("classid", ofclass) ->
		  		$("objectid", withid ? withid.sval() : uuid)
		   );
	
	// Merge the parameters
	ctx << parm;
	
	// Perform the moduleaction.
	string moderr;
	corestatus_t res = mdb.createobject (ofclass, withid, ctx, moderr);

	// Handle the result.
	switch (res)
	{
		case status_ok:
			// Report success to the database.
			if (! db.reportsuccess (uuid))
			{
				// Failed. Not sure if this is what we want.
				seterror (db.getlasterrorcode(), db.getlasterror());
				CORE->log (log::error, "session ", "Database failure on marking "
						   "record: %s" %format (db.getlasterror()));
				
				(void) mdb.deleteobject (ofclass, uuid, parm, moderr);
				return NULL;
			}
			break;
		
		case status_failed:
			seterror (ERR_MDB_ACTION_FAILED, moderr);
			
			if (! db.reportfailure (uuid))
			{
				string err = db.getlasterror();
				CORE->log (log::error, "session ", "Database failure on "
						   "marking delete %s" %format (err));
			}
			return NULL;
			
		case status_postponed:
			break;
	}

	if (ofclass && mdb.getclass (ofclass).cascades)
	{
		handlecascade (parentid, ofclass, uuid);
	}
	
	return new (memory::retainable::onstack) string (uuid);
}

// ==========================================================================
// METHOD coresession::handlecrypts
// ==========================================================================
bool coresession::handlecrypts (const statstring &parentid,
								const statstring &ofclass, 
								const statstring &withid,
								value &param)
{
	// Make sure we have a class.
	if (! mdb.classexists (ofclass))
	{
		CORE->log (log::critical, "session", "Request for crypt of class "
				   "<%S> which doesn't seem to exist" %format (ofclass));
		ALERT->alert ("Session error on crypt for nonexistant class "
					  "name=<%S>" %format (ofclass));
		return false;
	}
	
	// Get a reference to the coreclass object.
	coreclass &C = mdb.getclass (ofclass);
	
	CORE->log (log::debug, "session", "Handlecrypt class=<%S>"
			   %format (ofclass));
	
	foreach (opt, C.param)
	{
		// Something with a crypt-attribute
		if ((opt("enabled") == true) &&
		    (opt.attribexists ("crypt")))
		{
			if (opt("type") != "password")
			{
				CORE->log (log::warning, "session", "%S::%S has a "
						   "crypt-attribute but is not marked as a "
						   "password field" %format (ofclass, opt.id()));
			}
			// Is it currently filled in?
			if (param[opt.id()].sval().strlen() != 0)
			{
                const string precryptheader="pre-crypted password follows\n";
			    if(param[opt.id()].sval().strncmp(precryptheader,precryptheader.strlen()) == 0)
			    {
                    param[opt.id()] = param[opt.id()].sval().mid(param[opt.id()].sval().strchr('\n')+1);
                    break;
			    }
				// Yes, it will need crypting.
				string crypted;
				string salt;
				md5checksum csum;
				
				// Check the crypt type
				caseselector (C.param[opt.id()]("crypt"))
				{
					// extern type: ask the module
					incaseof ("extern") :
						if (mdb.makecrypt (ofclass, opt.id(),
								param[opt.id()].sval(), crypted) != status_ok)
						{
							CORE->log (log::error, "session", "Could not "
									   "get crypt-result from module");
							seterror (ERR_SESSION_CRYPT);
							return false;
						}
						break;
					
					// built-in md5-type
					incaseof ("md5") :
						for (int i=0; i<8; ++i)
							salt.strcat (MD5SALT_VALID[rand() & 63]);
						
						crypted = csum.md5pw (param[opt.id()].cval(),
											  salt.str());
						break;
					
					defaultcase:
						CORE->log (log::error, "session", "Unknown crypt-"
								   "type: %S"
								   %format (C.param[opt.id()]("crypt")));
						return false;
				}
				param[opt.id()] = crypted;
			}
			else // No, it's not filled in.
			{
				string uuid;
				value object;
				
				// Find the previous version.
				uuid = db.findobject (parentid, ofclass, withid, nokey);
				if (! uuid)
				{
					uuid = db.findobject (parentid, ofclass, nokey, withid);
				}
				if (! uuid)
				{
					// Completely failed to find it, something fishy's
					// going on.
					CORE->log (log::error, "session", "Object submitted "
							   "with externally crypted field, no data "
							   "for the field and no pre-existing "
							   "object, member=<%S::%S>"
							   %format (ofclass, opt.id()));
							   
					seterror (ERR_SESSION_CRYPT_ORIG);
					return false;
				}
				
				// Get the object from the database and copy the old
				// crypted version.
				object.clear();
				// handle errors?
				db.fetchobject (object, uuid, false /*formodule*/);
				param[opt.id()] = object[0][opt.id()];
			}
		}
	}
	return true;
}

// ==========================================================================
// METHOD coresession::updateobject
// ==========================================================================
bool coresession::updateobject (const statstring &parentid,
							    const statstring &ofclass,
							    const statstring &withid,
							    const value &withparam_,
							    bool immediate)
{
	value withparam = withparam_;
	string uuid;
	string nuuid;
	
	DEBUG.storefile ("session", "param", withparam, "updateobject");
	
	// Catch internal classes.
	if (mdb.isinternalclass (ofclass))
	{
		internalclass &cl = mdb.geticlass (ofclass);
		bool r = cl.updateobject (this, parentid, withid, withparam);
		if (! r) seterror (ERR_ICLASS, cl.error());
		return r;
	}

	// Complain if the class does not exist.
	if (! mdb.classexists (ofclass))
	{
		CORE->log (log::error, "session", "Could not update object, class "
				   "<%S> does not exist" %format (ofclass));
		seterror (ERR_SESSION_CLASS_UNKNOWN);
		return false;
	}
	
	// Handle any cryptable fields.
	if (! handlecrypts (parentid, ofclass, withid, withparam))
	{
		// Crypting failed. Bummer.
		CORE->log (log::error, "session", "Update failed due to crypt error");
		seterror (ERR_SESSION_CRYPT);
		return false;
	}
	
	// Get the uuid of the original object.
	uuid = db.findobject (parentid, ofclass, withid, nokey);
	if (! uuid) uuid = db.findobject (parentid, ofclass, nokey, withid);
	
	// Object not found?
	if (! uuid)
	{
		// Report the problem.
		CORE->log (log::error, "session", "Update of object with id/metaid <%S> "
				   "which could not be found in the database: %s"
				   %format (withid, db.getlasterror()));
				   
		seterror (ERR_SESSION_OBJECT_NOT_FOUND);
		return false;
	}
	
	coreclass &cl = mdb.getclass (ofclass);
	value oldobject;
	
	if (! db.fetchobject (oldobject, uuid, false))
	{
		CORE->log (log::error, "session", "Object disappeared while trying "
				   "to update class=<%S> uuid=<%S>" %format (ofclass, uuid));
		
		seterror (ERR_SESSION_OBJECT_NOT_FOUND);
		return false;
	}
	
	// Fill in missing parameters from the old object.
	foreach (par, cl.param)
	{
		if ((! par.attribexists ("enabled")) || (par("enabled") == false))
		{
			withparam[par.id()] = oldobject[ofclass][par.id()];
		}
		else if (par.attribexists ("required") && par("required") &&
				 (! withparam.exists (par.id())))
		{
			withparam[par.id()] = oldobject[ofclass][par.id()];
		}
	}

	bool updatesucceeded;
	
	updatesucceeded = db.updateobject (withparam, uuid, immediate);
	if (! updatesucceeded) // FIXME: get rid of bool var?
	{
		seterror (db.getlasterrorcode(), db.getlasterror());
		CORE->log (log::error, "session ", "Database failure on updating "
				   "object: %s" %format (db.getlasterror()));
		return false;
	}
	
	if (immediate)
    {
		return true;
    }

	nuuid = uuid;
		
	value parm;
	value ctx;
	
	// Get the parameters for the module action.
	if (! db.fetchobject (parm, nuuid, true /* formodule */))
	{
		CORE->log (log::critical, "session", "Database failure getting object-"
				   "related data: %s" %format (db.getlasterror()));
		ALERT->alert ("Session error on object-related data\n"
					  "uuid=<%S> error=<%S>" %format (nuuid,
					  db.getlasterror()));
					  
		return false;
	}

	ctx = $("OpenCORE:Context", parm[0].id()) ->
		  $("OpenCORE:Session",
		  		$("sessionid", id.sval()) ->
		  		$("classid", ofclass) ->
		  		$("objectid", withid ? withid.sval() : uuid)
		   );

	// Merge the parameters
	ctx << parm;

	// Perform the moduleaction.
	string moderr;
	corestatus_t res = mdb.updateobject (ofclass, withid, ctx, moderr);

	switch (res)
	{
		case status_ok:
			if (! db.reportsuccess (nuuid))
			{
				seterror (db.getlasterrorcode(), db.getlasterror());
				CORE->log (log::error, "session ", "Database failure on marking "
						   "record: %s", db.getlasterror().cval());
				
				if (! db.fetchobject (parm, uuid, true))
				{
					CORE->log (log::critical, "session", "Database failure "
							   "getting object-related data, cannot roll "
							   "back to previous: %s", db.getlasterror().str());
					
					ALERT->alert ("Session error, database failure "
								 "getting object-related data\n"
								 "uuid=<%S> error=<%S>" %format (nuuid,
								 db.getlasterror()));
								 
					return false;
				}
				ctx << parm;
				(void) mdb.updateobject (ofclass, nuuid, ctx, moderr);
				return false;
			}
			break;
		
		case status_failed:
			seterror (ERR_MDB_ACTION_FAILED, moderr);
			if (! db.reportfailure (uuid))
			{
				CORE->log (log::error, "session ", "Database failure on "
						   "rolling back reality objects: %s",
						   db.getlasterror().cval());
			}
			return false;
			
		case status_postponed:
			break;
	}
	
	if (ofclass && mdb.getclass (ofclass).cascades)
	{
		handlecascade (parentid, ofclass, nuuid);
	}

	return true;
}

// ==========================================================================
// METHOD coresession::deleteobject
// ==========================================================================
bool coresession::deleteobject (const statstring &parentid,
							    const statstring &ofclass,
							    const statstring &withid,
							    bool immediate)
{
	// Catch internal classes.
	if (mdb.isinternalclass (ofclass))
	{
		internalclass &cl = mdb.geticlass (ofclass);
		bool r = cl.deleteobject (this, parentid, withid);
		if (! r) seterror (ERR_ICLASS, cl.error());
		return r;
	}

	string uuidt; // uuid of top of tree-to-be-deleted
	uuidt = db.findobject (parentid, ofclass, withid, nokey);
	if (! uuidt) uuidt = db.findobject (parentid, ofclass, nokey, withid);
	
	if (! uuidt)
	{
		CORE->log (log::error, "session", "Error deleting object <%S>: not "
				   "found: %s", uuidt.str(), db.getlasterror().str());
				   
		seterror (db.getlasterrorcode(), db.getlasterror());
		return false;
	}

	value uuidlist;
	
	if(!db.listrecursively(uuidlist, uuidt))
	{
		CORE->log (log::error, "session", "Error deleting object <%S>:"
					"recursive listing failed: %s" %format (uuidt,
					db.getlasterror()));
				   
		seterror (db.getlasterrorcode(), db.getlasterror());
		return false;
	}
	
	// Inspect the first object for $prototype$ mess.
	// FIXME: be smarter about this.
	bool firstobject = true;
	
	foreach(uuid, uuidlist)
	{	
		bool deletesucceeded = db.deleteobject (uuid, immediate, true);
		if (! deletesucceeded) // FIXME: get rid of bool var?
		{
			CORE->log (log::error, "session", "Error deleting object <%S>: %s",
					   uuid.str(), db.getlasterror().str());
			
			seterror (db.getlasterrorcode(), db.getlasterror());
			return false;
		}
		
		// Skip this part for immediate (database-only) requests.
		if (immediate) continue;

		value parm;
		value ctx;

		// Get the parameters for the module action.
		db.fetchobject (parm, uuid, true /* formodule */);
		
		if (firstobject)
		{
			firstobject = false;
			foreach (obj, parm)
			{
				if (obj["metaid"].sval().strstr("$prototype$") >= 0)
				{
					CORE->log (log::error, "session", "Denied delete of "
							   "prototype object");
					seterror (ERR_SESSION_NOTALLOWED);
					return false;
				}
			}
		}

		DEBUG.storefile ("session", "fetched", parm, "deleteobject");

		ctx = $("OpenCORE:Context", parm[0].id()) ->
			  $("OpenCORE:Session",
					$("sessionid", id.sval()) ->
					$("classid", ofclass) ->
					$("objectid", withid ? withid.sval() : uuid.sval())
			   );

		// Merge the parameters
		ctx << parm;

		string moderr;
		corestatus_t res;
		res = mdb.deleteobject (parm[0].id().sval(), withid, ctx, moderr);
		
		switch (res)
		{
			case status_failed:
    			ALERT->alert ("Module failed to delete %s in recursive delete: %s"
							%format (uuid, moderr));
				// fall through

			case status_ok:
				if (! db.reportsuccess (uuid))
				{
					seterror (db.getlasterrorcode(), db.getlasterror());
					
					CORE->log (log::error, "session ", "Database failure "
							   "on marking record: %s"
							   		%format (db.getlasterror()));
			
					db.fetchobject (parm, uuid, true);
					ctx << parm;
					
					(void) mdb.createobject (ofclass, withid, ctx, moderr);
				}
				break;

			case status_postponed:
				break;
		}
	}
	
	if (ofclass && mdb.getclass (ofclass).cascades)
	{
		handlecascade (parentid, ofclass, uuidt);
	}

	return true;
}

void csbreakme (void) {}

// ==========================================================================
// METHOD coresession::getclassinfo
// ==========================================================================
value *coresession::getclassinfo (const string &forclass)
{
	CORE->log (log::debug, "session", "getclassinfo <%S>", forclass.str());
	
	// The class named "ROOT" is a virtual concept within moduledb to keep
	// track of classes that don't have parent objects.
	if ( (forclass != "ROOT") && (! mdb.classexists (forclass)) )
	{
		CORE->log (log::error, "session", "Info for class <%S> requested "
				   "where no such class exists", forclass.str());
		
		seterror (ERR_SESSION_CLASS_UNKNOWN);
		return NULL;
	}
	
	// Squeeze the information out of the moduledb.
	returnclass (value) res retain;
	res = mdb.getclassinfo (forclass, meta["user"] == "openadmin");
	if (! res) return &res;
	DEBUG.storefile ("session", "res", res, "classinfo");
	return &res;
}

// ==========================================================================
// METHOD coresession::getuserquota
// ==========================================================================
value *coresession::getuserquota (const statstring &useruuid)
{
	returnclass (value) res retain;
	
	CORE->log (log::debug, "session", "getuserquota <%S>" %format (useruuid));
	
	value cl = mdb.listclasses ();
	foreach (c, cl)
	{
        int usage = 0;
        int quota = db.getuserquota (c.id(), useruuid, &usage);
        
        res["objects"][c.id()] = 
			$("units","Objects") ->
			$("quota", quota) ->
			$("usage", usage);
	}
	
	res["consumables"] = $("bandwidth",
								$("units","MB") ->
								$("quota", 1000) ->
								$("usage", 250)) ->
						 $("diskspace",
						 		$("units","MB") ->
						 		$("quota", 50) ->
						 		$("usage", 14));
	
	return &res;
}

// ==========================================================================
// METHOD coresession::setuserquota
// ==========================================================================
bool coresession::setuserquota (const statstring &ofclass,
								 int count,
							     const statstring &useruuid)
{
	CORE->log (log::debug, "session", "setuserquota <%S,%S,%i>"
			   %format (ofclass, useruuid, count));
			   
	if (db.setuserquota (ofclass, count, useruuid)) return true;
	seterror (db.getlasterrorcode(), db.getlasterror());
	return false;
}

// ==========================================================================
// METHOD coresession::chown
// ==========================================================================
bool coresession::chown(const statstring &ofobject,
						const statstring &user)
{
	CORE->log (log::debug, "session", "chown <%S,%S>"
			   %format (ofobject, user));

	statstring uuid;
	uuid = db.findobject (nokey, "User", nokey, user);
	if (! uuid) uuid = user;
	
	if (db.chown (ofobject, uuid)) return true;
	seterror (db.getlasterrorcode(), db.getlasterror());
	return false;
}

// ==========================================================================
// METHOD coresession::callmethod
// ==========================================================================
value *coresession::callmethod (const statstring &parentid,
								const statstring &ofclass,
								const statstring &withid,
								const statstring &method,
								const value &withparam)
{
	string uuid;
	value obj;
	value argv;
	corestatus_t st;
	
	returnclass (value) res retain;
	
	// If an id is given, the method wants the object's members and
	// what not.
	if (withid)
	{
		uuid = db.findobject (parentid, ofclass, withid, nokey);
		if (! uuid) uuid = db.findobject (parentid, ofclass, nokey, withid);
		
		// The id was not a valid metaid/uuid, let's moan and exit.
		if (! uuid)
		{
			CORE->log (log::error, "session", "Callmethod: object not found "
					   "class=<%S> id=<%S>" %format (ofclass, withid));
			seterror (ERR_SESSION_OBJECT_NOT_FOUND);
			return &res;
		}
		obj.clear();
		db.fetchobject (obj, uuid, true /* formodule */);
	}
	
	argv["obj"] = obj;
	argv["argv"] = withparam;
	
	string moderr;
	st = mdb.callmethod (parentid, ofclass, withid, method, argv, res, moderr);
	
	// Report an error if this went wrong.
	if (st != status_ok)
	{
		seterror (ERR_MDB_ACTION_FAILED, moderr);
		CORE->log (log::error, "session", "Callmethod: Error from moduledb "
				   "<%S::%S> id=<%S>" %format (ofclass, method, withid));
	}
	
	return &res;
}

// ==========================================================================
// METHOD coresession::seterror
// ==========================================================================
void coresession::seterror (unsigned int code, const string &msg)
{
	errors = $("code", code) ->
			 $("message", msg);
}

// ==========================================================================
// METHOD coresession::seterror
// ==========================================================================
void coresession::seterror (unsigned int code)
{
    string mid = "0x%04x" %format (code);
	
	errors = $("code", code) ->
			 $("message", CORE->rsrc["errors"]["en"][mid]);
}

// ==========================================================================
// METHOD coresession::upcontext
// ==========================================================================
statstring *coresession::upcontext (const statstring &parentid)
{
	returnclass (statstring) res retain;
	res = db.findparent (parentid);
	return &res;
}

// ==========================================================================
// METHOD coresession::bindcontext
// ==========================================================================
statstring *coresession::bindcontext (const statstring &parentid,
									  const statstring &bindclass,
									  const statstring &bindid)
{
	returnclass (statstring) res retain;
	res = db.findobject (parentid, bindclass, bindid, nokey);
	if (! res) res = db.findobject (parentid, bindclass, nokey, bindid);
	return &res;
}

// ==========================================================================
// METHOD coresession::getquotauuid
// ==========================================================================
statstring *coresession::getquotauuid (const statstring &userid,
									   const statstring &name)
{
	returnclass (statstring) res retain;
	
	if (quotamap["metaid"][userid].exists (name))
	{
		res = quotamap["metaid"][userid][name];
	}
	else
	{
		res = strutil::uuid ();
		quotamap["metaid"][userid][name] = res;
		quotamap["uuid"][res] = name;
	}
	
	return &res;
}

$exception (listobjectsInnerException, "");

// ==========================================================================
// METHOD coresession::listdynamicobjects
// ==========================================================================
bool coresession::syncdynamicobjects (const statstring &parentid,
									  const statstring &ofclass,
									  int offset, int count)
{
	value curdb;
	value olddb;
	string err;
	
	curdb = mdb.listdynamicobjects (parentid, ofclass, err, count, offset);
	if (err.strlen())
	{
		seterror (ERR_MDB_ACTION_FAILED, err);
		return false;
	}

	if (! db.listobjects (olddb, parentid, $(ofclass), false, count, offset))
	{
		CORE->log (log::error, "session", "Error listing cached "
				   "dynamic objects: %s" %format (db.getlasterror()));
		// FIXME: Pass db.lasterror upwards if this ever makes sense
		olddb.clear();
	}
	
	DEBUG.storefile ("session","old", olddb, "syncdynamicobjects");
	DEBUG.storefile ("session","new", curdb, "syncdynamicobjects");
	
	value skipfields = $("uuid",true) ->
					   $("parentid",true) ->
					   $("ownerid",true) ->
					   $("id",true) ->
					   $("metaid",true);
	
	// Find any nodes currently in the database that we need to
	// update or delete.
	foreach (oldnode, olddb[0])
	{
		// Is this oldie absent in the new list?
		if (! curdb[0].exists (oldnode.id()))
		{
			CORE->log (log::debug, "session", "Removing cached node "
					   "id=<%S>" %format (oldnode.id()));
			// Yeah, kick its butt.
			string uuid = oldnode["uuid"];
			db.deleteobject (uuid);
			db.reportsuccess (uuid);
		}
		else
		{
			// No, so let's consider this an update.
			// We'll need a temporary object to get rid of the
			// extra uuid/id/metaid fields.
			CORE->log (log::debug, "session", "Updating cached node "
					   "id=<%S>" %format (oldnode.id()));
			
			bool changed = false;
			
			foreach (field, curdb[0][oldnode.id()])
			{
				if (skipfields.exists (field.id())) continue;
				
				if (oldnode[field.id()] != field)
				{
					changed = true;
					CORE->log (log::debug, "session", "Cached node "
							   "field <%S> changed" %format (field.id()));
					break;
				}
				else
				{
					CORE->log (log::debug, "session", "Cached <%S> \"%S\" == "
							   "\"%S\"" %format (field.id(),
							   oldnode[field.id()], field));
				}
			}
			
			if (changed)
			{
				value repnode = curdb[0][oldnode.id()];
				repnode.rmval ("uuid");
				repnode.rmval ("id");
				repnode.rmval ("metaid");
				string uuid = oldnode["uuid"];
				db.updateobject (repnode, uuid);
				db.reportsuccess (uuid);
			}
		}
	}
	
	// Now find any new objects.
	foreach (curnode, curdb[0])
	{
		// Never heard of this geezer before?
		if (! olddb[0].exists (curnode.id()))
		{
			// Great let's introduce him to the database then.
			CORE->log (log::debug, "session", "Creating cached node "
					   "id=<%S>" %format (curnode.id()));
					   
			string uuid = db.createobject (parentid, curnode,
										   ofclass, curnode.id(), false, true);
			if (! uuid)
			{
				CORE->log (log::error, "session", "Error creating "
						   "database cache of dynamic list: %s"
						   %format (db.getlasterror()));
				seterror (db.getlasterrorcode(), db.getlasterror());
			}
		}
	}
	
	return true;
}

// ==========================================================================
// METHOD coresession::listmeta
// ==========================================================================
value *coresession::listmeta (const statstring &parentid,
							  const statstring &ofclass,
							  int offset, int count)
{
	returnclass (value) res retain;
	
	coreclass &metaclass = mdb.getclass (ofclass);
	CORE->log (log::info, "session", "Getrecords metabase(%S)" %format (ofclass));
	try
	{
		res[ofclass].type ("class");
		const value &dlist = mdb.getmetaclasschildren (ofclass);
		
		foreach (dclass, dlist)
		{
			coreclass &realclass = mdb.getclass (dclass);
			value tres;
			if (! db.listobjects (tres, parentid, $(dclass), false))
			{
				throw (listobjectsInnerException());
			}
			
			foreach (row, tres[0])
			{
				value &into = res[ofclass][row.id()];
				into["class"] = dclass;
				into["type"] = realclass.shortname;
				into["id"] = row.id();
				into["uuid"] = row["uuid"];
				
				// Match up metaclass param in this class' parameters.
				foreach (p, metaclass.param)
				{
					statstring pid = p.id();
					bool isnick;
					if (p.attribexists ("nick"))
					{
						pid = p("nick");
					}
					
					if (row.exists (p.id()) && row[p.id()].sval())
					{
						value &rp = realclass.param[p.id()];
						
						// Resolve ref.
						if (rp("type") == "ref")
						{
							statstring rlabel = rp("ref").sval().copyafter ('/');
							
							value rlist;
							db.fetchobject (rlist, row[p.id()]);
							into[pid] = rlist[0][rlabel];
						}
						else
						{
							into[pid] = row[p.id()];
						}
					}
				}
				
				into["description"] =
					strutil::valueparse (realclass.metadescription, row);
			}
		}
	}
	catch (exception e)
	{
		res.clear();
		seterror (db.getlasterrorcode(), db.getlasterror());
	}
	
	return &res;
}

// ==========================================================================
// METHOD coresession::listobjects
// ==========================================================================
value *coresession::listobjects (const statstring &parentid,
							     const statstring &ofclass,
							     int offset, int count)
{
	returnclass (value) res retain;
	bool wasdynamic = false;
	string err;
	
	CORE->log (log::debug, "session", "Listobjects class=<%S> "
			   "parentid=<%S>" %format (ofclass, parentid));

	// Catch internal classes.
	if (mdb.isinternalclass (ofclass))
	{
		CORE->log (log::info, "session", "Listobjects for internal class");
		internalclass &cl = mdb.geticlass (ofclass);
		res = cl.listobjects (this, parentid);
		if (! res.count()) seterror (ERR_ICLASS, cl.error());
		return &res;
	}

	if (! mdb.classexists (ofclass))
	{
		res.clear();
		seterror (ERR_SESSION_CLASS_UNKNOWN, ofclass);
		return &res;
	}
	
	// Is the class a metaclass?
	if (ofclass && mdb.classismetabase (ofclass))
	{
		res = listmeta (parentid, ofclass, offset, count);
		return &res;
	}

	// Is the class dynamic? NB: dynamic classes can not be derived
	// from a metaclass, ktxbai.
	if (mdb.isdynamic (ofclass))
	{
		wasdynamic = true;
		CORE->log (log::debug, "session", "Class is dynamic");
		
		if (! syncdynamicobjects (parentid, ofclass, offset, count))
			return &res;
	}
	
	// Get the list out of the database. Either pure, or through or
	// just-in-time-insertion super-secret techniques above.
	if (! db.listobjects (res, parentid, $(ofclass), false /* not formodule */,
						   count, offset))
	{
		res.clear();
		seterror (db.getlasterrorcode(), db.getlasterror());
	}
	
	DEBUG.storefile ("session", "res", res, "listobjects");

	return &res;
}

// ==========================================================================
// METHOD coresession::getreferences
// ==========================================================================
value *coresession::getreferences (const statstring &parentid,
								   const statstring &refclass,
								   const statstring &reflabel)
{
	returnclass (value) res retain;
	
	if (! mdb.classexists (refclass))
	{
		CORE->log (log::error, "session", "Attempt to get references to "
				   "class <%S> which does not exist" %format (refclass));
		return &res;
	}
	
	value list = listobjects (parentid, refclass);
	foreach (obj, list[refclass])
	{
		if (! obj.exists (reflabel))
		{
			CORE->log (log::warning, "session", "Reference to class "
					   "<%S> field <%S> has no value for object "
					   "with id <%S>" %format (refclass, reflabel, obj.id()));
		}
		else
		{
			if (obj[reflabel].sval())
				res[obj["id"]] = obj[reflabel];
		}
	}
	
	return &res;
}

// ==========================================================================
// METHOD coresession::fieldwhitel
// ==========================================================================
bool coresession::fieldwhitel (value &objs, value &whitel)
{	
	CORE->log (log::debug, "session", "fieldwhitel <(objs),(whitel)>");

	return db.fieldwhitel (objs, whitel);
}	

// ==========================================================================
// METHOD coresession::getobject
// ==========================================================================
value *coresession::getobject (const statstring &parentid,
							   const statstring &ofclass,
							   const statstring &withid)
{
	string uuid;
	returnclass (value) res retain; 
	
	// Catch internal classes.
	if (mdb.isinternalclass (ofclass))
	{
		internalclass &cl = mdb.geticlass (ofclass);
		res = cl.getobject (this, parentid, withid);
		if (! res.count()) seterror (ERR_ICLASS, cl.error());
		return &res;
	}

	// Find the object, either by uuid or by metaid.
	uuid = db.findobject (parentid, ofclass, withid, nokey);
	if (! uuid) uuid = db.findobject (parentid, ofclass, nokey, withid);

	// Not found under any method.
	if (! uuid)
	{
		CORE->log (log::error, "session", "Could not resolve object at "
				   "parentid=<%S> class=<%S> key=<%S>"
				   %format (parentid, ofclass, withid));

		seterror (ERR_SESSION_OBJECT_NOT_FOUND);
		return &res;
	}
	
	// Is the class dynamic? NB: dynamic classes can not be derived
	// from a metaclass, ktxbai.
	if (mdb.isdynamic (ofclass))
	{
		CORE->log (log::debug, "session", "Class is dynamic");
		
		if (! syncdynamicobjects (parentid, ofclass, 0, -1))
			return &res;
	}
	
	if (! db.fetchobject (res, uuid, false /* formodule */))
	{
		CORE->log (log::critical, "session", "Database failure getting object-"
				   "related data for <%s>: %S" %format (uuid,
				   db.getlasterror()));
				   
		ALERT->alert ("Session error on object-related data\n"
					  "uuid=<%S> error=<%S>" %format (uuid,
					  db.getlasterror()));
				   
		seterror (db.getlasterrorcode(), db.getlasterror());
		return NULL;
	}
		
	foreach (field, res[0])
	{
		if (field.attribexists ("type"))
		{
			if (field("type") == "password") field = "";
		}
	}
	res[0]["class"] = ofclass;

	DEBUG.storefile ("session","res", res, "getobject");
	return &res; 
}

// ==========================================================================
// METHOD coresession::getclass
// ==========================================================================
statstring *coresession::getclass (const statstring &parentid)
{
	return db.findclassname (parentid);
}

// ==========================================================================
// METHOD sessionexpire::run
// ==========================================================================
void sessionexpire::run (void)
{
	CORE->log (log::info, "expire", "Thread started");
	
	while (true)
	{
		// Wait either 60 seconds or until the next event.
		value ev = waitevent (60000);
		if (ev)
		{
			// A 'die' event is a shutdown request.
			if (ev.type() == "shutdown")
			{
				CORE->log (log::info, "expire", "Thread shutting down");
				shutdownCondition.broadcast();
				return;
			}
			else // Unknown event.
			{
				CORE->log (log::warning, "expire", "Received unrecognized "
						   "event: %S" %format (ev.type()));
			}
		}
		else // Not an event, so it was a timeout, do our thing.
		{
			int cnt = sdb->expire ();
			if (cnt)
			{
				CORE->log (log::info, "expire", "Cleaned %i sessions", cnt);
			}
		}
	}
}

// ==========================================================================
// METHOD coresession::getmoduleforclass
// ==========================================================================
coremodule *coresession::getmoduleforclass (const statstring &cl)
{
	return mdb.getmoduleforclass (cl);
}

// ==========================================================================
// METHOD coresession::classexists
// ==========================================================================
bool coresession::classexists (const statstring &cl)
{
	return mdb.classexists (cl);
}

// ==========================================================================
// METHOD coresession::getmodulebyname
// ==========================================================================
coremodule *coresession::getmodulebyname (const statstring &mname)
{
	return mdb.getmodulebyname (mname);
}

// ==========================================================================
// METHOD coresession::getlanguages
// ==========================================================================
value *coresession::getlanguages (void)
{
	return mdb.getlanguages ();
}

// ==========================================================================
// METHOD coresession::listmodules
// ==========================================================================
value *coresession::listmodules (void)
{
	return mdb.listmodules ();
}

// ==========================================================================
// METHOD coresession::listclasses
// ==========================================================================
value *coresession::listclasses (void)
{
	return mdb.listclasses ();
}

// ==========================================================================
// METHOD coresession::getworld
// ==========================================================================
value *coresession::getworld (void)
{
	returnclass (value) res retain;
	bool isadmin = meta["user"] == "openadmin";
	
	foreach (cl, mdb.classlist)
	{
		if ((! isadmin) && mdb.isadminclass (cl.id())) continue;
		res[cl.id()] = mdb.getclassinfo (cl.id(), isadmin);
	}
	return &res;
}

// ==========================================================================
// METHOD coresession::listparamsformethod
// ==========================================================================
value *coresession::listparamsformethod (const statstring &parentid,
										 const statstring &ofclass,
										 const statstring &withid,
										 const statstring &methodname)
{
	return mdb.listparamsformethod (parentid, ofclass, withid, methodname);
}

// ==========================================================================
// METHOD coresession::mlockr
// ==========================================================================
void coresession::mlockr (void)
{
	spinlock.lockr ();
}

// ==========================================================================
// METHOD coresession::mlockw
// ==========================================================================
void coresession::mlockw (const string &plocker)
{
	spinlock.lockw ();
	locker = plocker;
}

// ==========================================================================
// METHOD coresession::munlock
// ==========================================================================
void coresession::munlock (void)
{
	if (locker.strlen()) locker.crop ();
	spinlock.unlock ();
}

// ==========================================================================
// METHOD coresession::handlecascade
// ==========================================================================
void coresession::handlecascade (const statstring &parentid,
								 const statstring &ofclass,
								 const string &withid)
{
	CORE->log (log::info, "session ", "Handling cascades for <%S>"
			   %format (ofclass));
			   
	coreclass &theclass = mdb.getclass (ofclass);
	if (! theclass.requires) return;
	
	value uuidlist;
	value classmatch;
	if (! db.listrecursively (uuidlist, parentid)) return;
	if (! uuidlist.count()) return;
	
	value classlist = mdb.getclasses (theclass.requires);
	
	foreach (crsr, classlist)
	{
		if (crsr.sval() != ofclass.sval())
			classmatch[crsr.sval()] = true;
	}
	
	foreach (uuid, uuidlist)
	{
		value tenv;
		if (db.fetchobject (tenv, uuid, true))
		{
			statstring objectclass = tenv[0].id();
			if (classmatch.exists (objectclass))
			{
				statstring objectid = tenv[0]["id"].sval();
				tenv << $("OpenCORE:Context", tenv[0].id()) ->
						$("OpenCORE:Session",
							$("sessionid", id.sval()) ->
							$("classid", objectclass.sval()) ->
							$("objectid", objectid.sval())
						 );
				
				DEBUG.storefile ("session", "data", tenv, "handlecascade");
				
				string moderr;
				mdb.updateobject (objectclass, objectid, tenv, moderr);
			}
		}
	}
}
