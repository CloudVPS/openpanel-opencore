// This file is part of OpenPanel - The Open Source Control Panel
// OpenPanel is free software: you can redistribute it and/or modify it 
// under the terms of the GNU General Public License as published by the Free 
// Software Foundation, using version 3 of the License.
//
// Please note that use of the OpenPanel trademark may be subject to additional 
// restrictions. For more information, please visit the Legal Information 
// section of the OpenPanel website on http://www.openpanel.com/

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
// CONSTRUCTOR SessionDB
// ==========================================================================
SessionDB::SessionDB (ModuleDB &pmdb)
	: mdb (pmdb)
{
	first = last = NULL;
	lck.o = 0;
}

// ==========================================================================
// DESTRUCTOR SessionDB
// ==========================================================================
SessionDB::~SessionDB (void)
{
}

// ==========================================================================
// METHOD ::saveToDisk
// ==========================================================================
void SessionDB::saveToDisk (const string &path)
{
	value serialized;
	
	sharedsection (lck)
	{
		CoreSession *crsr = first;
		while (crsr)
		{
			crsr->spinlock.lockr();
			
			timestamp t = crsr->heartbeat;
			
			serialized[crsr->id] =
				$("meta",crsr->meta)->
				$("errors",crsr->errors)->
				$("quotamap",crsr->quotamap)->
				$("heartbeat",t);
			
			crsr->spinlock.unlock();
			
			crsr = crsr->next;
		}
	}
	
	serialized.savexml (path);
}

// ==========================================================================
// METHOD ::loadFromDisk
// ==========================================================================
void SessionDB::loadFromDisk (const string &path)
{
	value serialized;
	
	serialized.loadxml (path);
	foreach (s, serialized)
	{
		createFromSerialized (s);
	}
}

// ==========================================================================
// METHOD SessionDB::get
// ==========================================================================
CoreSession *SessionDB::get (const statstring &id)
{
	CoreSession *res;
	
	sharedsection (lck)
	{
		res = find (id);
		if (res) res->inuse++;
	}
	
	if (res) log::write (log::debug, "Session", "SDB Open <%S>" %format (id));
	return res;
}

// ==========================================================================
// METHOD SessionDB::create
// ==========================================================================
CoreSession *SessionDB::create (const value &meta)
{
	CoreSession *s;
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
			log::write (log::error, "Session", "Software bug in session "
					    "database, exception caught");
		}
	}
	log::write (log::debug, "Session", "SDB Create <%S>" %format (id));
	
	return s;
}

// ==========================================================================
// METHOD ::createFromSerialized
// ==========================================================================
CoreSession *SessionDB::createFromSerialized (const value &ser)
{
	CoreSession *s;
	statstring id = ser.id();
	
	exclusivesection (lck)
	{
		try
		{
			lck = 1;
			s = demand (id);
			s->meta = ser["meta"];
			s->errors = ser["errors"];
			s->quotamap = ser["quotamap"];
			
			timestamp t = ser["heartbeat"];
			s->heartbeat = t.unixtime();
		}
		catch (...)
		{
			log::write (log::error, "Session", "Software bug in session "
					    "database, exception caught");
		}
	}
	log::write (log::debug, "Session", "SDB Recreate <%S>" %format (id));
	return s;
}

// ==========================================================================
// METHOD SessionDB::release
// ==========================================================================
void SessionDB::release (CoreSession *s)
{
	sharedsection (lck)
	{
		s->heartbeat = kernel.time.now();
		if (s->inuse > 0) s->inuse--;
		log::write (log::debug, "Session", "SDB Release <%S> "
				    "inuse <%i>" %format (s->id, s->inuse));
	}
}

// ==========================================================================
// METHOD SessionDB::remove
// ==========================================================================
void SessionDB::remove (CoreSession *s)
{
	if (! first) return;
	
	exclusivesection (lck)
	{
		try
		{
			lck = 1;
			CoreSession *crsr = first;
			CoreSession *parent = NULL;
			
			log::write (log::debug, "Session", "SDB Remove <%S>" %format (s->id));
						
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
			log::write (log::error, "Session", "Software bug in session "
					    "database, exception caught.");
		}
	}
}

// ==========================================================================
// METHOD SessionDB::find
// ==========================================================================
CoreSession *SessionDB::find (const statstring &key, bool noreport)
{
	if (! first)
	{
		log::write (log::debug, "Session", "Find on empty list");
		return NULL;
	}
	
	if (! key)
	{
		log::write (log::warning, "Session", "Find on empty key");
	}
	
	CoreSession *crsr = first;
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
		log::write (log::warning, "Session", "Session <%S> not "
				    "found" %format (key));
	}
	return NULL;
}

// ==========================================================================
// METHOD SessionDB::demand
// ==========================================================================
CoreSession *SessionDB::demand (const statstring &key)
{
	CoreSession *res;
	
	res = find (key, true);
	if (res)
	{
		res->heartbeat = kernel.time.now();
		res->inuse++;
	}
	else
	{
		res = new CoreSession (key, mdb);
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
// METHOD SessionDB::link
// ==========================================================================
void SessionDB::link (CoreSession *cs)
{
	assert (lck.o == 1);
	CoreSession *crsr = first;
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
// METHOD SessionDB::exists
// ==========================================================================
bool SessionDB::exists (const statstring &id)
{
	CoreSession *s;

	sharedsection (lck)
	{
		s = find (id);
	}
	
	if (s == NULL) return false;
	return true;
}

// ==========================================================================
// METHOD SessionDB::list
// ==========================================================================
value *SessionDB::list (void)
{
	returnclass (value) res retain;
	
	sharedsection (lck)
	{
		CoreSession *c = first;
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
// METHOD SessionDB::expire
// ==========================================================================
int SessionDB::expire (void)
{
	int result = 0;
	time_t cutoff = kernel.time.now() - 600;
	exclusivesection (lck)
	{
		try
		{
			lck = 1;
			CoreSession *s, *ns;
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
					log::write (log::debug, "Expire", "Passing <%S> inuse=<%i> "
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
			log::write (log::error, "Session", "Software bug in session "
					    "database expiration, exception caught.");
		}
	}
	
	return result;
}

// ==========================================================================
// CONSTRUCTOR CoreSession
// ==========================================================================
CoreSession::CoreSession (const statstring &myid, class ModuleDB &pmdb)
	: id (myid), mdb (pmdb)
{
	next = prev = higher = lower = NULL;
	heartbeat = kernel.time.now();
	inuse = 0;
	if (! db.init ())
	{
		log::write (log::error, "Session", "Error initializing the sqlite3 "
				    "database");
		throw (sqliteInitException());
	}
}

// ==========================================================================
// DESTRUCTOR CoreSession
// ==========================================================================
CoreSession::~CoreSession (void)
{
	db.deinit();
}

// ==========================================================================
/// METHOD CoreSession::login
// ==========================================================================
bool CoreSession::login (const string &user, const string &pass, bool superuser)
{
	bool res;
	
	if (superuser && user[0]=='!')
	{
        db.enableGodMode();
        log::write (log::info, "Session", "Login special <%S> in godmode"
				    %format (user, meta["origin"]));
        return true;
    }
    
	res = db.login (user, pass);
	if (! res)
	{
		setError (ERR_DBMANAGER_LOGINFAIL);
		log::write (log::error, "Session", "Failed login user "
				    "<%S> (%S)" %format (user, db.getLastError()));
		
	}
	else
	{
		log::write (log::info, "Session", "Login user=<%S> origin=<%S>"
				    %format (user, meta["origin"]));
		
		meta["user"] = user;
	}
	
	return res;
}

// ==========================================================================
/// METHOD CoreSession::getCredentials
// ==========================================================================
void CoreSession::getCredentials (value &creds)
{
    db.getCredentials (creds);
}

// ==========================================================================
/// METHOD CoreSession::setCredentials
// ==========================================================================
void CoreSession::setCredentials (const value &creds)
{
    db.setCredentials (creds);
}

// ==========================================================================
/// METHOD CoreSession::userLogin
// ==========================================================================
bool CoreSession::userLogin (const string &user)
{
	bool res;
	
	res = db.userLogin (user);
	if (! res)
	{
		log::write (log::error, "Session", "Failed login "
				    "user <%S> (%S)" %format (user, db.getLastError()));
	}
	else
	{
		log::write (log::info, "Session", "Login user=<%S> origin=<%S>"
				    %format (user, meta["origin"]));
				   
		meta["user"] = user;
	}
	
	// FIXME: is this ok?
	setError (db.getLastErrorCode(), db.getLastError());
	return res;
}

bool CoreSession::isAdmin(void) const
{
	bool isadmin = meta["user"] == "openadmin";
	return isadmin;
}


// ==========================================================================
// METHOD CoreSession::createObject
// ==========================================================================
string *CoreSession::createObject (const statstring &parentid,
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
	
	DEBUG.storeFile ("Session", "create-param", withparam, "createObject");

	// Catch internal classes.
	if (mdb.isInternalClass (ofclass))
	{
		InternalClass &cl = mdb.getInternalClass (ofclass);
		string *r = cl.createObject (this, parentid, withparam, withid);
		if (! r) setError (ERR_ICLASS, cl.error());
		return r;
	}

	// Check for the class.
	if (! mdb.classExists (ofclass))
	{
		log::write (log::error, "Session", "Create request for class <%S> "
				    "which does not exist" %format (ofclass));
		setError (ERR_SESSION_CLASS_UNKNOWN);
		return NULL;
	}
	
	// Resolve to a CoreClass reference.
	CoreClass &cl = mdb.getClass (ofclass);
	
	// Normalize singleton instances
	if (cl.singleton) withid = cl.singleton;
	
	// Make sure the indexing makes sense.
	if ( (! cl.manualindex) && (withid) )
	{
		log::write (log::error, "Session", "Create request with manual id "
				    "on class <%S> with autoindex" %format (ofclass));
		setError (ERR_SESSION_INDEX);
		return NULL;
	}
	
	if (cl.manualindex && (! withid))
	{
		log::write (log::error, "Session", "Create request with no required "
				    "manual id on class <%S>" %format (ofclass));
		setError (ERR_SESSION_NOINDEX);
		return NULL;
	}
	
	if (cl.parentrealm)
	{
		value vparent;
		string pmid;
		
		if (! db.fetchObject (vparent, parentid, /* formodule */ false))
		{
			log::write (log::error, "Session", "Lookup failed for "
					    "fetchObject on parentid=<%S>" %format (parentid));
			setError (ERR_SESSION_PARENTREALM);
			return NULL;
		}
		
		pmid = vparent[0]["metaid"];
		if (! pmid)
		{
			log::write (log::error, "Session", "Error getting metaid "
					    "from resolved parent object: %J" %format (vparent));
			setError (ERR_SESSION_PARENTREALM, "Error finding metaid");
			return NULL;
		}
		
		if ((! cl.hasprototype) && (pmid.strstr ("$prototype$") >= 0))
		{
			log::write (log::error, "Session", "Blocking attempt to "
					    "create new prototype records under"
					    " id=%S" %format (pmid));
			setError (ERR_SESSION_CREATEPROTO);
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
	
	DEBUG.storeFile ("Session", "normalize-pre", withparam, "createObject");
	
	if (! cl.normalize (withparam, err))
	{
		log::write (log::error, "Session", "Input data validation "
				    "error: %s " %format (err));
		setError (ERR_SESSION_VALIDATION, err);
		return NULL;
	}

	DEBUG.storeFile ("Session", "normalize-post", withparam, "createObject");
	
	// Handle any module-bound crypting voodoo on the fields.
	if (! handleCrypts (parentid, ofclass, withid, withparam))
	{
		log::write (log::error, "Session", "Create failed due to crypt error");
		// handleCrypts already sets the error
		return NULL;
	}

	value ctx;
	value parm;

	if (mdb.classIsDynamic (ofclass))
	{
		uuid = strutil::uuid();
		
		ctx = $("OpenCORE:Context", ofclass) ->
			  $("OpenCORE:Session",
					$("sessionid", id.sval()) ->
					$("classid", ofclass) ->
					$("objectid", withid ? withid.sval() : uuid)
			   )->
			  $(ofclass,withparam);
		
		if (cl.requires && parentid)
		{
			value par;
			if (mdb.classIsDynamic (cl.requires))
			{
				ctx << syncDynamicObjects (nokey, cl.requires, 0, -1, parentid);
			}
			else
			{
				statstring puuid;
				puuid = db.findObject (nokey, ofclass, parentid, nokey);
				if (! puuid) puuid = db.findObject (nokey, ofclass, nokey, parentid);
				
				value pv;
				if (db.fetchObject (pv, puuid, false))
				{
					ctx << pv;
				}
			}
		}
		
		DEBUG.storeFile ("Session","dynamic-ctx", ctx, "createObject");
	}
	else
	{
		// create the object in the database, it will be marked as wanted.
		uuid = db.createObject(parentid, withparam, ofclass, withid, false, immediate);
		if (! uuid)
		{
			log::write (log::error, "Session", "Database error: %s"
						%format (db.getLastError()));
			setError (db.getLastErrorCode(), db.getLastError());
			return NULL;
		}
		
		if (owner)
		{
			if (! chown (uuid, owner))
			{
				db.reportFailure (uuid);
				setError (ERR_SESSION_CHOWN);
				return NULL;
			}
		}

		if (immediate)
		{
			return new (memory::retainable::onstack) string (uuid);
		}

		// Get the parameters for the module action.
		if (! db.fetchObject (parm, uuid, true /* formodule */))
		{
			log::write (log::critical, "Session", "Database failure getting object-"
					   "related data for '%S': %s" %format (uuid,
						db.getLastError()));
					   
			ALERT->alert ("Session error on object-related data\n"
						  "uuid=<%S> error=<%s>" %format (uuid,
						  db.getLastError()));
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
	}
	
	// Perform the moduleaction.
	string moderr;
	corestatus_t res = mdb.createObject (ofclass, withid, ctx, moderr);

	// Handle the result.
	switch (res)
	{
		case status_ok:
			// Report success to the database.
			if (! db.reportSuccess (uuid))
			{
				// Failed. Not sure if this is what we want.
				setError (db.getLastErrorCode(), db.getLastError());
				log::write (log::error, "session ", "Database failure on marking "
						    "record: %s" %format (db.getLastError()));
				
				(void) mdb.deleteObject (ofclass, uuid, parm, moderr);
				return NULL;
			}
			break;
		
		case status_failed:
			setError (ERR_MDB_ACTION_FAILED, moderr);
			
			if (! db.reportFailure (uuid))
			{
				string err = db.getLastError();
				log::write (log::error, "session ", "Database failure on "
						    "marking delete %s" %format (err));
			}
			return NULL;
			
		case status_postponed:
			break;
	}

	if (ofclass && mdb.getClass (ofclass).cascades)
	{
		handleCascade (parentid, ofclass, uuid);
	}
	
	return new (memory::retainable::onstack) string (uuid);
}

// ==========================================================================
// METHOD CoreSession::handleCrypts
// ==========================================================================
bool CoreSession::handleCrypts (const statstring &parentid,
								const statstring &ofclass, 
								const statstring &withid,
								value &param)
{
	// Make sure we have a class.
	if (! mdb.classExists (ofclass))
	{
		log::write (log::critical, "Session", "Request for crypt of class "
				    "<%S> which doesn't seem to exist" %format (ofclass));
		ALERT->alert ("Session error on crypt for nonexistant class "
					  "name=<%S>" %format (ofclass));
		return false;
	}
	
	// Get a reference to the CoreClass object.
	CoreClass &C = mdb.getClass (ofclass);
	
	log::write (log::debug, "Session", "Handlecrypt class=<%S>"
					%format (ofclass));
	
	foreach (opt, C.param)
	{
		// Something with a crypt-attribute
		if ((opt("enabled") == true) &&
		    (opt.attribexists ("crypt")))
		{
			if (opt("type") != "password")
			{
				log::write (log::warning, "Session", "%S::%S has a "
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
						if (mdb.makeCrypt (ofclass, opt.id(),
								param[opt.id()].sval(), crypted) != status_ok)
						{
							log::write (log::error, "Session", "Could not "
									    "get crypt-result from module");
							setError (ERR_SESSION_CRYPT);
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
						log::write (log::error, "Session", "Unknown crypt-"
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
				uuid = db.findObject (parentid, ofclass, withid, nokey);
				if (! uuid)
				{
					uuid = db.findObject (parentid, ofclass, nokey, withid);
				}
				if (! uuid)
				{
					// Completely failed to find it, something fishy's
					// going on.
					log::write (log::error, "Session", "Object submitted "
							    "with externally crypted field, no data "
							    "for the field and no pre-existing "
							    "object, member=<%S::%S>"
							    %format (ofclass, opt.id()));
							   
					setError (ERR_SESSION_CRYPT_ORIG);
					return false;
				}
				
				// Get the object from the database and copy the old
				// crypted version.
				object.clear();
				// handle errors?
				db.fetchObject (object, uuid, false /*formodule*/);
				param[opt.id()] = object[0][opt.id()];
			}
		}
	}
	return true;
}

// ==========================================================================
// METHOD CoreSession::updateObject
// ==========================================================================
bool CoreSession::updateObject (const statstring &parentid,
							    const statstring &ofclass,
							    const statstring &withid,
							    const value &withparam_,
							    bool immediate)
{
	value withparam = withparam_;
	string uuid;
	string nuuid;
	string err;
	
	DEBUG.storeFile ("Session", "param", withparam, "updateObject");
	
	// Catch internal classes.
	if (mdb.isInternalClass (ofclass))
	{
		InternalClass &cl = mdb.getInternalClass (ofclass);
		bool r = cl.updateObject (this, parentid, withid, withparam);
		if (! r) setError (ERR_ICLASS, cl.error());
		return r;
	}

	// Complain if the class does not exist.
	if (! mdb.classExists (ofclass))
	{
		log::write (log::error, "Session", "Could not update object, class "
				    "<%S> does not exist" %format (ofclass));
		setError (ERR_SESSION_CLASS_UNKNOWN);
		return false;
	}
	
	// Handle any cryptable fields.
	if (! handleCrypts (parentid, ofclass, withid, withparam))
	{
		// Crypting failed. Bummer.
		log::write (log::error, "Session", "Update failed due to crypt error");
		setError (ERR_SESSION_CRYPT);
		return false;
	}
	
	
	CoreClass &cl = mdb.getClass (ofclass);
	value oldobject;
	value parm;
	
	if (mdb.classIsDynamic (ofclass))
	{
		value t = syncDynamicObjects (parentid, ofclass, 0, -1, withid);
		oldobject = t;
		DEBUG.storeFile ("Session", "oldobject", oldobject, "updateObject");
	}
	else
	{
		// Get the uuid of the original object.
		uuid = db.findObject (parentid, ofclass, withid, nokey);
		if (! uuid) uuid = db.findObject (parentid, ofclass, nokey, withid);
		
		// Object not found?
		if (! uuid)
		{
			// Report the problem.
			log::write (log::error, "Session", "Update of object with id/metaid <%S> "
						"which could not be found in the database: %s"
						%format (withid, db.getLastError()));
					   
			setError (ERR_SESSION_OBJECT_NOT_FOUND);
			return false;
		}
	
		if (! db.fetchObject (oldobject, uuid, false))
		{
			log::write (log::error, "Session", "Object disappeared while trying "
						"to update class=<%S> uuid=<%S>" %format (ofclass, uuid));
			
			setError (ERR_SESSION_OBJECT_NOT_FOUND);
			return false;
		}
	}
	
	// Fill in missing parameters from the old object.
	foreach (par, cl.param)
	{
		if ((! par.attribexists ("enabled")) || (par("enabled") == false))
		{
			withparam[par.id()] = oldobject[ofclass][par.id()];
		}
		else if (! withparam.exists (par.id()))
		{
			withparam[par.id()] = oldobject[ofclass][par.id()];
		}
	}
	
	if (! cl.normalize (withparam, err))
	{
		log::write (log::error, "Session", "Input data validation error: "
				    "%S" %format (err));
		setError (ERR_SESSION_VALIDATION, err);
		return false;
	}

	value ctx;

	if (mdb.classIsDynamic (ofclass))
	{
		ctx = $("OpenCORE:Context", ofclass) ->
			  $("OpenCORE:Session",
					$("sessionid", id.sval()) ->
					$("classid", ofclass) ->
					$("objectid", withid ? withid.sval() : uuid)
			   )->
			  $(ofclass,withparam);
			  
		if (cl.requires && parentid)
		{
			value par;
			if (mdb.classIsDynamic (cl.requires))
			{
				ctx <<  syncDynamicObjects (nokey, cl.requires, 0, -1, parentid);
			}
			else
			{
				statstring puuid;
				puuid = db.findObject (nokey, ofclass, parentid, nokey);
				if (! puuid) puuid = db.findObject (nokey, ofclass, nokey, parentid);
				
				value pv;
				if (db.fetchObject (pv, puuid, false))
				{
					ctx << pv;
				}
			}
		}
	}
	else
	{
		bool updatesucceeded;
		
		updatesucceeded = db.updateObject (withparam, uuid, immediate);
		if (! updatesucceeded) // FIXME: get rid of bool var?
		{
			setError (db.getLastErrorCode(), db.getLastError());
			log::write (log::error, "session ", "Database failure on updating "
						"object: %s" %format (db.getLastError()));
			return false;
		}
		
		if (immediate)
		{
			return true;
		}

		nuuid = uuid;
		
		// Get the parameters for the module action.
		if (! db.fetchObject (parm, nuuid, true /* formodule */))
		{
			log::write (log::critical, "Session", "Database failure getting object-"
						"related data: %s" %format (db.getLastError()));
			ALERT->alert ("Session error on object-related data\n"
						  "uuid=<%S> error=<%S>" %format (nuuid,
						  db.getLastError()));
						  
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
	
	}

	DEBUG.storeFile ("Session", "update-ctx", ctx, "updateObject");

	// Perform the moduleaction.
	string moderr;
	corestatus_t res = mdb.updateObject (ofclass, withid, ctx, moderr);

	switch (res)
	{
		case status_ok:
			if (mdb.isInternalClass (ofclass)) break;
			if (! db.reportSuccess (nuuid))
			{
				setError (db.getLastErrorCode(), db.getLastError());
				log::write (log::error, "Session", "Database failure on marking "
						    "record: %s" %format (db.getLastError()));
				
				if (! db.fetchObject (parm, uuid, true))
				{
					log::write (log::critical, "Session", "Database failure "
							   "getting object-related data, cannot roll "
							   "back to previous: %s" %format(db.getLastError()));
					
					ALERT->alert ("Session error, database failure "
								 "getting object-related data\n"
								 "uuid=<%S> error=<%S>" %format (nuuid,
								 db.getLastError()));
								 
					return false;
				}
				ctx << parm;
				(void) mdb.updateObject (ofclass, nuuid, ctx, moderr);
				return false;
			}
			break;
		
		case status_failed:
			setError (ERR_MDB_ACTION_FAILED, moderr);
			if (mdb.isInternalClass (ofclass)) return false;
			if (! db.reportFailure (uuid))
			{
				log::write (log::error, "Session", "Database failure on "
						    "rolling back reality objects: %s"
						    %format (db.getLastError()));
			}
			return false;
			
		case status_postponed:
			break;
	}
	
	if (ofclass && mdb.getClass (ofclass).cascades)
	{
		handleCascade (parentid, ofclass, nuuid);
	}

	return true;
}

// ==========================================================================
// METHOD CoreSession::deleteObject
// ==========================================================================
bool CoreSession::deleteObject (const statstring &parentid,
							    const statstring &ofclass,
							    const statstring &withid,
							    bool immediate)
{
	// Catch internal classes.
	if (mdb.isInternalClass (ofclass))
	{
		InternalClass &cl = mdb.getInternalClass (ofclass);
		bool r = cl.deleteObject (this, parentid, withid);
		if (! r) setError (ERR_ICLASS, cl.error());
		return r;
	}
	
	if (mdb.classIsDynamic (ofclass))
	{
		value ctx;
		
		ctx = $("OpenCORE:Context", ofclass) ->
			  $("OpenCORE:Session",
					$("sessionid", id.sval()) ->
					$("classid", ofclass) ->
					$("objectid", withid)
			   );
		
		CoreClass &cl = mdb.getClass (ofclass);
		if (cl.requires && parentid)
		{
			ctx << syncDynamicObjects (nokey, cl.requires, 0, -1, parentid);
		}
		
		string moderr;
		mdb.deleteObject (ofclass, withid, ctx, moderr);
		return true;
	}

	string uuidt; // uuid of top of tree-to-be-deleted
	uuidt = db.findObject (parentid, ofclass, withid, nokey);
	if (! uuidt) uuidt = db.findObject (parentid, ofclass, nokey, withid);
	
	if (! uuidt)
	{
		log::write (log::error, "Session", "Error deleting object '%S': not "
				    "found: %s" %format (uuidt, db.getLastError()));
				   
		setError (db.getLastErrorCode(), db.getLastError());
		return false;
	}

	if (!db.isDeleteable (uuidt))
	{
		log::write (log::error, "Session", "Error deleting object '%S': "
				    "%s" %format (uuidt, db.getLastError()));
		setError (db.getLastErrorCode(), db.getLastError());
		return false;
	}

	value uuidlist;
	
	if (!db.listObjectTree(uuidlist, uuidt))
	{
		log::write (log::error, "Session", "Error deleting object '%S':"
					"recursive listing failed: %s" %format (uuidt,
					db.getLastError()));
				   
		setError (db.getLastErrorCode(), db.getLastError());
		return false;
	}
	
	// Inspect the first object for $prototype$ mess.
	// FIXME: be smarter about this.
	bool firstobject = true;
	
	foreach (uuid, uuidlist)
	{	
		bool deletesucceeded = db.deleteObject (uuid, immediate, true);
		if (! deletesucceeded) // FIXME: get rid of bool var?
		{
			log::write (log::error, "Session", "Error deleting object '%S': %s"
					    %format (uuid, db.getLastError()));
			
			setError (db.getLastErrorCode(), db.getLastError());
			return false;
		}
		
		// Skip this part for immediate (database-only) requests.
		if (immediate) continue;

		value parm;
		value ctx;

		// Get the parameters for the module action.
		db.fetchObject (parm, uuid, true /* formodule */);
		
		if (firstobject)
		{
			if (parm[0]["metaid"].sval().strstr("$prototype$") >= 0)
			{
				log::write (log::error, "Session", "Denied delete of "
						    "prototype object");
				setError (ERR_SESSION_NOTALLOWED);
				return false;
			}
		}

		DEBUG.storeFile ("Session", "fetched", parm, "deleteObject");

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
		res = mdb.deleteObject (parm[0].id().sval(), withid, ctx, moderr);
		
		switch (res)
		{
			case status_failed:
				if (firstobject)
				{
					db.reportFailure (uuid);
					setError (ERR_MDB_ACTION_FAILED);
					return false;
				}
    			ALERT->alert ("Module failed to delete %s in recursive delete: %s"
							%format (uuid, moderr));
				// fall through

			case status_ok:
				if (! db.reportSuccess (uuid))
				{
					setError (db.getLastErrorCode(), db.getLastError());
					
					log::write (log::error, "session ", "Database failure "
							    "on marking record: %s"
							   		%format (db.getLastError()));
			
					db.fetchObject (parm, uuid, true);
					ctx << parm;
					
					(void) mdb.createObject (ofclass, withid, ctx, moderr);
				}
				break;

			case status_postponed:
				break;
		}
		
		if (firstobject) firstobject = false;
	}
	
	if (ofclass && mdb.getClass (ofclass).cascades)
	{
		handleCascade (parentid, ofclass, uuidt);
	}

	return true;
}

// ==========================================================================
// METHOD CoreSession::getClassInfo
// ==========================================================================
value *CoreSession::getClassInfo (const string &forclass)
{
	log::write (log::debug, "Session", "getClassInfo <%S>" %format(forclass));
	
	// The class named "ROOT" is a virtual concept within ModuleDB to keep
	// track of classes that don't have parent objects.
	if ( (forclass != "ROOT") && (! mdb.classExists (forclass)) )
	{
		log::write (log::error, "Session", "Info for class <%S> requested "
				    "where no such class exists" %format (forclass));
		
		setError (ERR_SESSION_CLASS_UNKNOWN);
		return NULL;
	}
	
	// Squeeze the information out of the ModuleDB.
	returnclass (value) res retain;
	res = mdb.getClassInfo (forclass, isAdmin() );
	if (! res) return &res;
	DEBUG.storeFile ("Session", "res", res, "classInfo");
	return &res;
}

// ==========================================================================
// METHOD CoreSession::getUserQuota
// ==========================================================================
value *CoreSession::getUserQuota (const statstring &useruuid)
{
	returnclass (value) res retain;
	
	log::write (log::debug, "Session", "getUserQuota <%S>" %format (useruuid));
	
	value cl = mdb.listClasses ();
	foreach (c, cl)
	{
        int usage = 0;
        int quota = db.getUserQuota (c.id(), useruuid, &usage);
        
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
// METHOD CoreSession::setUserQuota
// ==========================================================================
bool CoreSession::setUserQuota (const statstring &ofclass,
								 int count,
							     const statstring &useruuid)
{
	log::write (log::debug, "Session", "setUserQuota <%S,%S,%i>"
			    %format (ofclass, useruuid, count));
			   
	if (db.setUserQuota (ofclass, count, useruuid)) return true;
	setError (db.getLastErrorCode(), db.getLastError());
	return false;
}

// ==========================================================================
// METHOD CoreSession::chown
// ==========================================================================
bool CoreSession::chown(const statstring &ofobject,
						const statstring &user)
{
	log::write (log::debug, "Session", "chown <%S,%S>"
			    %format (ofobject, user));

	statstring uuid;
	uuid = db.findObject (nokey, "User", nokey, user);
	if (! uuid) uuid = user;
	
	if (db.chown (ofobject, uuid)) return true;
	setError (db.getLastErrorCode(), db.getLastError());
	return false;
}

// ==========================================================================
// METHOD CoreSession::callMethod
// ==========================================================================
value *CoreSession::callMethod (const statstring &parentid,
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
	if (withid && (! mdb.classIsDynamic (ofclass)))
	{
		uuid = db.findObject (parentid, ofclass, withid, nokey);
		if (! uuid) uuid = db.findObject (parentid, ofclass, nokey, withid);
		
		// The id was not a valid metaid/uuid, let's moan and exit.
		if (! uuid)
		{
			log::write (log::error, "Session", "Callmethod: object not found "
					    "class=<%S> id=<%S>" %format (ofclass, withid));
			setError (ERR_SESSION_OBJECT_NOT_FOUND);
			return &res;
		}
		obj.clear();
		db.fetchObject (obj, uuid, true /* formodule */);
	}
	
	argv["obj"] = obj;
	argv["argv"] = withparam;
	
	string moderr;
	st = mdb.callMethod (parentid, ofclass, withid, method, argv, res, moderr);
	
	// Report an error if this went wrong.
	if (st != status_ok)
	{
		setError (ERR_MDB_ACTION_FAILED, moderr);
		log::write (log::error, "Session", "Callmethod: Error from ModuleDB "
				    "<%S::%S> id=<%S>" %format (ofclass, method, withid));
	}
	
	return &res;
}

// ==========================================================================
// METHOD CoreSession::setError
// ==========================================================================
void CoreSession::setError (unsigned int code, const string &msg)
{
	errors = $("code", code) ->
			 $("message", msg);
}

// ==========================================================================
// METHOD CoreSession::setError
// ==========================================================================
void CoreSession::setError (unsigned int code)
{
    string mid = "0x%04x" %format (code);
	
	errors = $("code", code) ->
			 $("message", CORE->rsrc["errors"]["en"][mid]);
}

// ==========================================================================
// METHOD CoreSession::findParent
// ==========================================================================
statstring *CoreSession::findParent (const statstring &parentid)
{
	returnclass (statstring) res retain;
	res = db.findParent (parentid);
	return &res;
}

// ==========================================================================
// METHOD CoreSession::getQuotaUUID
// ==========================================================================
statstring *CoreSession::getQuotaUUID (const statstring &userid,
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

$exception (listObjectsInnerException, "");

// ==========================================================================
// METHOD CoreSession::listDynamicObjects
// ==========================================================================
value *CoreSession::syncDynamicObjects (const statstring &parentid,
										const statstring &ofclass,
										int offset, int count,
										const statstring &withid)
{
	returnclass (value) res retain;

	static lock<value> dynamicuuids;
	string err;
	statstring rparentid;
	
	value parentobj;
	
	if (parentid)
	{
		if (db.fetchObject (parentobj, parentid, /* formodule */ false))
		{
			rparentid = parentobj[0]["metaid"];
		}
		else
		{
			sharedsection (dynamicuuids)
			{
				if (dynamicuuids.exists (parentid))
				{
					rparentid = dynamicuuids[parentid];
				}
				else
				{
					rparentid = parentid;
				}
			}
		}
	}
	
	CORE->log (log::debug, "Session", "syncDynamicObjects rparentid=<%S>"
			   %format (rparentid));
	
	if (withid)
	{
		value tres;
		tres = mdb.listDynamicObjects (parentid, rparentid, ofclass, err);
		
		DEBUG.storeFile ("Session", "singleres", tres, "syncDynamicObjects");
		
		if (tres[ofclass].exists (withid))
		{
			res[ofclass] = tres[ofclass][withid];
		}
		else
		{
			foreach (obj, tres[ofclass])
			{
				if ((obj["metaid"] == withid) || (obj["uuid"] == withid))
				{
					res[ofclass] = obj;
					break;
				}
			}
		}
	}
	else
	{
		res = mdb.listDynamicObjects (parentid, rparentid, ofclass, err);
	}
	
	DEBUG.storeFile ("Session","mdbres", res, "syncDynamicObjects");
	
	if (err.strlen())
	{
		setError (ERR_MDB_ACTION_FAILED, err);
		return false;
	}

	foreach (cl, res)
	{
		if (cl("type") != "class") continue;
		statstring classid = cl.id();
		foreach (obj, cl)
		{
			if (obj("type") != "object") continue;
			if (obj["metaid"] != obj["uuid"])
			{
				exclusivesection (dynamicuuids)
				{
					dynamicuuids[obj["uuid"]] = obj["metaid"];
				}
			}
		}
	}
	
	return &res;
}

// ==========================================================================
// METHOD CoreSession::listMeta
// ==========================================================================
value *CoreSession::listMeta (const statstring &parentid,
							  const statstring &ofclass,
							  int offset, int count)
{
	returnclass (value) res retain;
	
	CoreClass &metaclass = mdb.getClass (ofclass);
	log::write (log::debug, "Session", "Getrecords metabase(%S)" %format (ofclass));
	try
	{
		res[ofclass].type ("class");
		const value &dlist = mdb.getMetaClassChildren (ofclass);
		DEBUG.storeFile ("Session", "dlist",dlist,"listMeta");
		
		foreach (dclass, dlist)
		{
			if (! mdb.classExists (dclass)) continue;
			
			CoreClass &realclass = mdb.getClass (dclass);
			value tres;
			if (! db.listObjects (tres, parentid, $(dclass), false))
			{
				throw (listObjectsInnerException());
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
					
					if (row.exists (p.id()) && row[p.id()].sval())
					{
						into[pid] = row[p.id()];
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
		setError (db.getLastErrorCode(), db.getLastError());
	}
	
	return &res;
}

// ==========================================================================
// METHOD CoreSession::listObjects
// ==========================================================================
value *CoreSession::listObjects (const statstring &parentid,
							     const statstring &ofclass,
							     int offset, int count)
{
	returnclass (value) res retain;
	
	bool wasdynamic = false;
	string err;
	
	log::write (log::debug, "Session", "Listobjects class=<%S> "
			    "parentid=<%S>" %format (ofclass, parentid));

	// Catch internal classes.
	if (mdb.isInternalClass (ofclass))
	{
		log::write (log::debug, "Session", "Listobjects for internal class");
		InternalClass &cl = mdb.getInternalClass (ofclass);
		res = cl.listObjects (this, parentid);
		if (! res.count()) setError (ERR_ICLASS, cl.error());
		return &res;
	}

	if (! mdb.classExists (ofclass))
	{
		res.clear();
		setError (ERR_SESSION_CLASS_UNKNOWN, ofclass);
		return &res;
	}
	
	// Is the class a metaclass?
	if (ofclass && mdb.classIsMetaBase (ofclass))
	{
		res = listMeta (parentid, ofclass, offset, count);
		return &res;
	}

	// Is the class dynamic? NB: dynamic classes can not be derived
	// from a metaclass, ktxbai.
	if (mdb.classIsDynamic (ofclass))
	{
		wasdynamic = true;
		log::write (log::debug, "Session", "Class is dynamic");
		
		res = syncDynamicObjects (parentid, ofclass, offset, count);
		return &res;
	}
	
	// Get the list out of the database. Either pure, or through or
	// just-in-time-insertion super-secret techniques above.
	if (! db.listObjects (res, parentid, $(ofclass), false /* not formodule */,
						   count, offset))
	{
		res.clear();
		setError (db.getLastErrorCode(), db.getLastError());
	}
	
	DEBUG.storeFile ("Session", "res", res, "listObjects");

	return &res;
}

// ==========================================================================
// METHOD CoreSession::applyFieldWhiteList
// ==========================================================================
bool CoreSession::applyFieldWhiteList (value &objs, value &whitel)
{	
	log::write (log::debug, "Session", "applyFieldWhiteList <(objs),(whitel)>");
	return db.applyFieldWhiteList (objs, whitel);
}	

// ==========================================================================
// METHOD CoreSession::getObject
// ==========================================================================
value *CoreSession::getObject (const statstring &parentid,
							   const statstring &ofclass,
							   const statstring &withid)
{
	string uuid;
	returnclass (value) res retain; 
	
	// Catch internal classes.
	if (mdb.isInternalClass (ofclass))
	{
		InternalClass &cl = mdb.getInternalClass (ofclass);
		res = cl.getObject (this, parentid, withid);
		if (! res.count()) setError (ERR_ICLASS, cl.error());
		return &res;
	}

	// Is the class dynamic? NB: dynamic classes can not be derived
	// from a metaclass, ktxbai.
	if (mdb.classIsDynamic (ofclass))
	{
		log::write (log::debug, "Session", "Class is dynamic");
		res = syncDynamicObjects (parentid, ofclass, 0, -1, withid);
		return &res;
	}
	
	// Find the object, either by uuid or by metaid.
	uuid = db.findObject (parentid, ofclass, withid, nokey);
	if (! uuid) uuid = db.findObject (parentid, ofclass, nokey, withid);

	// Not found under any method.
	if (! uuid)
	{
		log::write (log::error, "Session", "Could not resolve object at "
				    "parentid=<%S> class=<%S> key=<%S>"
				    %format (parentid, ofclass, withid));

		setError (ERR_SESSION_OBJECT_NOT_FOUND);
		return &res;
	}
	
	if (! db.fetchObject (res, uuid, false /* formodule */))
	{
		log::write (log::critical, "Session", "Database failure getting object-"
				    "related data for '%s': %S" %format (uuid,
				    db.getLastError()));
				   
		ALERT->alert ("Session error on object-related data\n"
					  "uuid=<%S> error=<%S>" %format (uuid,
					  db.getLastError()));
				   
		setError (db.getLastErrorCode(), db.getLastError());
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

	DEBUG.storeFile ("Session","res", res, "getObject");
	return &res; 
}

// ==========================================================================
// METHOD CoreSession::getClass
// ==========================================================================
statstring *CoreSession::getClass (const statstring &parentid)
{
	return db.classNameFromUUID (parentid);
}

// ==========================================================================
// METHOD SessionExpireThread::run
// ==========================================================================
void SessionExpireThread::run (void)
{
	log::write (log::info, "expire", "Thread started");
	
	while (true)
	{
		// Wait either 60 seconds or until the next event.
		value ev = waitevent (60000);
		if (ev)
		{
			// A 'die' event is a shutdown request.
			if (ev.type() == "shutdown")
			{
				log::write (log::info, "expire", "Thread shutting down");
				shutdownCondition.broadcast();
				return;
			}
			else // Unknown event.
			{
				log::write (log::warning, "expire", "Received unrecognized "
						    "event: %S" %format (ev.type()));
			}
		}
		else // Not an event, so it was a timeout, do our thing.
		{
			int cnt = sdb->expire ();
			if (cnt)
			{
				log::write (log::info, "expire", "Cleaned %i sessions"
							%format(cnt));
			}
		}
	}
}

// ==========================================================================
// METHOD CoreSession::getModuleForClass
// ==========================================================================
CoreModule *CoreSession::getModuleForClass (const statstring &cl)
{
	return mdb.getModuleForClass (cl);
}

// ==========================================================================
// METHOD CoreSession::classExists
// ==========================================================================
bool CoreSession::classExists (const statstring &cl)
{
	return mdb.classExists (cl);
}

// ==========================================================================
// METHOD CoreSession::getModuleByName
// ==========================================================================
CoreModule *CoreSession::getModuleByName (const statstring &mname)
{
	return mdb.getModuleByName (mname);
}

// ==========================================================================
// METHOD CoreSession::getLanguages
// ==========================================================================
value *CoreSession::getLanguages (void)
{
	return mdb.getLanguages ();
}

// ==========================================================================
// METHOD CoreSession::listModules
// ==========================================================================
value *CoreSession::listModules (void)
{
	return mdb.listModules ();
}

// ==========================================================================
// METHOD CoreSession::listClasses
// ==========================================================================
value *CoreSession::listClasses (void)
{
	return mdb.listClasses ();
}

// ==========================================================================
// METHOD CoreSession::getWorld
// ==========================================================================
value *CoreSession::getWorld (void)
{
	returnclass (value) res retain;
	bool isadmin = isAdmin();
	
	foreach (cl, mdb.classlist)
	{
		if ((! isadmin) && mdb.isAdminClass (cl.id())) continue;
		res[cl.id()] = mdb.getClassInfo (cl.id(), isadmin);
	}
	return &res;
}

// ==========================================================================
// METHOD CoreSession::listParamsForMethod
// ==========================================================================
value *CoreSession::listParamsForMethod (const statstring &parentid,
										 const statstring &ofclass,
										 const statstring &withid,
										 const statstring &methodname)
{
	return mdb.listParamsForMethod (parentid, ofclass, withid, methodname);
}

// ==========================================================================
// METHOD CoreSession::mlockr
// ==========================================================================
void CoreSession::mlockr (void)
{
	spinlock.lockr ();
}

// ==========================================================================
// METHOD CoreSession::mlockw
// ==========================================================================
void CoreSession::mlockw (const string &plocker)
{
	spinlock.lockw ();
	locker = plocker;
}

// ==========================================================================
// METHOD CoreSession::munlock
// ==========================================================================
void CoreSession::munlock (void)
{
	if (locker.strlen()) locker.crop ();
	spinlock.unlock ();
}

// ==========================================================================
// METHOD CoreSession::handleCascade
// ==========================================================================
void CoreSession::handleCascade (const statstring &parentid,
								 const statstring &ofclass,
								 const string &withid)
{
	log::write (log::info, "session ", "Handling cascades for <%S>"
			    %format (ofclass));
			   
	CoreClass &theclass = mdb.getClass (ofclass);
	if (! theclass.requires) return;
	
	value uuidlist;
	value classmatch;
	if (! db.listObjectTree (uuidlist, parentid)) return;
	if (! uuidlist.count()) return;
	
	value classlist = mdb.getClasses (theclass.requires);
	
	foreach (crsr, classlist)
	{
		if (crsr.sval() != ofclass.sval())
			classmatch[crsr.sval()] = true;
	}
	
	foreach (uuid, uuidlist)
	{
		value tenv;
		if (db.fetchObject (tenv, uuid, true))
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
				
				DEBUG.storeFile ("Session", "data", tenv, "handleCascade");
				
				string moderr;
				mdb.updateObject (objectclass, objectid, tenv, moderr);
			}
		}
	}
}

// ==========================================================================
// METHOD CoreSession::resolveMetaID
// ==========================================================================
statstring *CoreSession::resolveMetaID (const statstring &uuid)
{
	returnclass (statstring) res retain;
	value v;
	
	if (db.fetchObject (v, uuid, /* formodule */ false))
	{
		res = v[0]["metaid"];
	}
	
	return &res;
}
