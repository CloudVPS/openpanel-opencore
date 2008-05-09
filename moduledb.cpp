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

#include "moduledb.h"
#include "status.h"
#include "error.h"
#include "opencore.h"
#include "paths.h"
#include "debug.h"
#include "alerts.h"

void breakme (void) {}

$exception (moduleInitException, "Error initializing module");

// cache layout:
// modules {
//   modulename = #version
// }
// classes {
//   classname = #version
// }
// livedb_serials {
//   modulename = #serial
// }

// ==========================================================================
// CONSTRUCTOR moduledb
// ==========================================================================
moduledb::moduledb (void)
{
	first = last = NULL;
	
	internalclasses.set ("OpenCORE:Quota", new quotaclass);
	internalclasses.set ("OpenCORE:ActiveSession", new sessionlistclass);
	internalclasses.set ("OpenCORE:ErrorLog", new errorlogclass);
	internalclasses.set ("OpenCORE:System", new coresystemclass);
	internalclasses.set ("OpenCORE:ClassList", new classlistclass);
}

// ==========================================================================
// METHOD moduledb::init
//
// TODO: detect double class registration
// ==========================================================================
void moduledb::init (const value &reloadmods)
{
	value cache;
	
	dbmanager db;
	if (! db.init ())
	{
		CORE->log (log::critical, "moduledb", "Could not init database");
		return;
	}
	
	// Indicate to the database that we're bypassing the authorization layer
	// for this primary exercise.
	db.iamgod();

	// Load the module.cache file, we will use this to track module versions
	// between opencore runs.
	string cachepath = PATH_CACHES;
	cachepath.strcat ("/module.cache");
	
	// Make sure the module cache exists at all, if not, we will create
	// an empty cache.
	if (! fs.exists (cachepath))
	{
		CORE->log (log::info, "moduledb", "No module cache found, assuming "
				   "new install");
				   
		cache["modules"]; // Initialize cache's tree structure.
		
		// If we can't write, consider that a dealbreaker.
		if (! cache.saveshox (cachepath))
		{
			CORE->log (log::critical, "moduledb", "Can not write to module "
					   "cache at <%s>, bailing.", cachepath.str());
			CORE->delayedexiterror ("Error saving module.cache");
			sleep (2);
			exit (1);
		}
	}
	else
	{
		cache.loadshox (cachepath);
		if (! cache.count())
		{
			CORE->log (log::warning, "moduledb", "Loaded module cache "
					   "seems to be empty");
		}
	}
	
	foreach (mod, reloadmods)
	{
		cache.rmval (mod);
	}
	
	// Read the module directory
	value mdir;
	mdir = fs.ls (PATH_MODULES);
	
	try
	{
		// Go over the directory entries
		foreach (filename, mdir)
		{
			string mname = filename.id();
			if (mname.globcmp ("*.module"))
			{
				try
				{
					loadmodule (mname, cache, db);
				}
				catch (exception e)
				{
					CORE->log (log::error, "moduledb", "Error loading "
							   "<%s>: %s" %format (mname, e.description));
					
					// Re-throw on user.module, without that one
					// there's little use starting up at all.
					if (mname == "User.module") throw (e);
				}
			}
		}
	}
	catch (exception e)
	{
		cache.saveshox (cachepath);
		CORE->delayedexiterror ("Error loading modules");
		exit (1);
	}
	
	// Save the cache.
	cache.saveshox (cachepath);
}

// ==========================================================================
// METHOD moduledb::loadmodule
// ==========================================================================
void moduledb::loadmodule (const string &mname, value &cache, dbmanager &db)
{
	static value history;
	
	// Prevent double registration through dependencies
	if (history.exists (mname)) return;
	history[mname] = true;
	
	string path = PATH_MODULES "/%s" %format (mname);
	CORE->log (log::info, "moduledb", "Loading <%s>" %format (path));
	
	// Make sure the module exists at all.
	if (! fs.exists (path))
	{
		CORE->log (log::critical, "moduledb", "Unable to load <%s>",
				   path.str());
		
		cache["modules"].rmval (mname.str());
		throw (moduleInitException("Module does not exist"));
	}
	
	// Create a new coremodule object
	coremodule *m = new coremodule (path, mname, this);
	
	// Run the module's verify script. Refrain from loading it if the
	// verify failed.
	if (! m->verify ())
	{
		delete m;	
		throw (moduleInitException ("Verify failed"));
	}
	
	// Merge languages.
	languages << m->languages;
	
	loaddependencies (mname, cache, db, m);
	
	// Link the module to the linked list.
	m->next = NULL;
	if (last)
	{
		m->prev = last;
		last->next = m;
		last = m;
	}
	else
	{
		m->prev = NULL;
		first = last = m;
	}
	
	bool firsttime = checkcache (mname, cache, m);
	registerquotas (m);
	registerclasses (mname, cache, db, m);
	makestagingdir (mname);
	
	handlegetconfig (mname, cache, db, m);
}

// ==========================================================================
// METHOD moduledb::checkcache
// ==========================================================================
bool moduledb::checkcache (const string &mname, value &cache,
						   coremodule *m)
{
	if (cache["modules"].exists (mname))
	{
		// Apperently it is, let's see if we are dealing with
		// a new version or some such nonsense.
		int cachedver = cache["modules"][mname].ival();
		int newver = m->meta["version"];
		
		if (cachedver > newver)
		{
			CORE->log (log::critical, "moduledb", "Version downgrade "
					   "detected on module <%s> current version = %i, "
					   "old version = %i" %format (mname,newver,cachedver));
					   
			throw (moduleInitException());
		}
		else if (cachedver < newver)
		{
			CORE->log (log::info, "moduledb", "New version of module "
					   "<%s> detected", mname.str());
		
			if (! m->updateok (cachedver))
			{
				CORE->log (log::critical, "moduledb", "Module <%s> "
						   "cannot deal with objects created by"
						   "version %i" %format (mname, cachedver));
				
				throw (moduleInitException());
			}
		}
		else
		{
			CORE->log (log::info, "moduledb", "Using cached version");
		}
		
		cache["modules"][mname] = m->meta["version"];
		byname[mname] = m;
	
		return false;
	}

	cache["modules"][mname] = m->meta["version"];
	byname[mname] = m;
	
	CORE->log (log::info, "moduledb", "No cache entry for "
			   "module <%s>", mname.str());
			   
	DEBUG.storefile ("moduledb", "miss", cache["modules"], "checkcache");
	return true;
}

// ==========================================================================
// METHOD moduledb::loaddependencies
// ==========================================================================
void moduledb::loaddependencies (const string &mname, value &cache,
								 dbmanager &db, coremodule *m)
{
	// Does the module require another module?
	if (m->meta.exists ("requires"))
	{
		// Yes, recurse.
		CORE->log (log::info, "moduledb", "Loading required module <%S>",
				   m->meta["requires"].cval());
		
		try
		{
			loadmodule (m->meta["requires"], cache, db);
		}
		catch (exception e)
		{
			CORE->log (log::error, "moduledb", "Error loading required "
					   "module <%S>, disabling depending module <%S>"
					   %format (m->meta["requires"], mname));
			
			delete m;
			throw (moduleInitException ("Dependency failed"));
		}
	}
	
}

// ==========================================================================
// METHOD moduledb::registerquotas
// ==========================================================================
void moduledb::registerquotas (coremodule *m)
{
	// Store the current version in the cache.
	if (m->meta.exists ("quotas"))
	{
		foreach (q, m->meta["quotas"])
		{
			byquota[q.id()] = m;
		}
	}
}

// ==========================================================================
// METHOD moduledb::makestagingdir
// ==========================================================================
void moduledb::makestagingdir (const string &mname)
{
	// Check for the staging directory
	string tmp;
	string stagingDir;

	tmp = mname;
	tmp = tmp.cutat (".module");
	stagingDir = PATH_CONF "/staging/%s" %format (tmp);
	
	if (! fs.exists (stagingDir))
	{
		// No? Create it.
		fs.mkdir (stagingDir);
		fs.chown (stagingDir, "opencore");
		fs.chgrp (stagingDir, "opencore");
	}
		
}

// ==========================================================================
// METHOD moduledb::registerclasses
// ==========================================================================
void moduledb::registerclasses (const string &mname, value &cache,
								dbmanager &db, coremodule *m)
{
	value childrendeps;
	
	/// Iterate over the coreclass objects
	foreach (classobj,m->classes)
	{
		if (byclass.exists (classobj.name))
		{
			CORE->log (log::critical, "moduledb", "Class <%S> "
					   "already exists in module <%S>",
					   classobj.name.str(), mname.str());
			cache["modules"].rmval (mname.str());
			throw (moduleInitException());
		}
		if (byclassuuid.exists (classobj.uuid))
		{
			CORE->log (log::critical, "moduledb", "Class with "
					   "uuid <%S> already registered."
					   %format (classobj.uuid));
		}
		byclass[classobj.name] = m;
		byclassuuid[classobj.uuid] = m;
		classlist[classobj.name] = mname;
		
		// Does this class require another class?
		if (classobj.requires)
		{
			CORE->log (log::info, "moduledb", "Registered class <%s> "
					   "requires <%s>", classobj.name.str(),
					   classobj.requires.str());
		
			// Yes, add this class to the byparent list.
			byparent[classobj.requires].newval() = classobj.name;
		}
		else
		{
			CORE->log (log::info, "moduledb", "Registered class <%S> "
					   "at root", classobj.name.str());
			
			// No requirements, parent this class to the virtual
			// parent class 'ROOT'.
			byparent["ROOT"].newval() = classobj.name;
		}
		
		if (classobj.childrendep)
		{
			childrendeps[classobj.name] = classobj.childrendep;
		}
		
		// Get registration data.
		value regdata = classobj.getregistration ();
		regdata("modulename") = mname;
		
		DEBUG.storefile ("moduledb", "regdata", regdata, "registerclasses");
		
		// Send it to the database manager.
		if (! db.registerclass (regdata))
		{
			// Failed, croak.
			CORE->log (log::critical, "moduledb", "Could not register "
					   "class <%S> with dbmanager: %s",
					   classobj.name.str(), db.getlasterror().cval());
			sleep (2);
			cache["modules"].rmval (mname.str());
			throw (moduleInitException());
		}
	}

	// If we recorde a "childrendep" in a parent class, set the
	// 'cascades' flag for this child class so we'll know we have
	// to cascade any updates to it.
	foreach (cdep, childrendeps)
	{
		if (classexists (cdep.sval()))
		{
			CORE->log (log::info, "moduledb", "Setting cascade for "
					   "class <%S>", cdep.cval());
			getclass (cdep.sval()).cascades = true;
		}
	}
}

// ==========================================================================
// METHOD moduledb::handlegetconfig
// ==========================================================================
void moduledb::handlegetconfig (const string &mname, value &cache,
							    dbmanager &db, coremodule *m)
{
	value current; //< Return from the module call.
	value out; //< Translated for dbmanager flattening.
	value uuids; //< Local-to-global id mapping for flattening.
	value skipparents; // parent-uuids that already existed
	
	CORE->log (log::info, "moduledb", "Getconfig for <%s>" %format (mname));
	
	// First let's get the currentconfig tree from the module.
	if (m->meta["implementation"]["getconfig"].bval())
	{
		current = m->getcurrentconfig();
		
		// Go over the type="object" nodes inside
		foreach (node, current)
		{
			// excluse the result node
			if (node.id() != "OpenCORE:Result")
			{
				// flatten into the 'out' array.
				parsetree (node, out, -1);
			}
		}
		
		DEBUG.storefile ("moduledb","getconfig-tree", out, "handlenewmodule");
		
		// Iterate over the out array
		foreach (obj, out)
		{
			statstring oclass = obj["class"].sval();
			statstring metaid;
			statstring localid = obj["id"].sval();
			statstring parentid;
			statstring uuid;
			
			if (obj.exists ("metaid")) metaid = obj["metaid"];
			
			// The parsetree() method will set a parent node
			// pointing to the index of the parent. Check if
			// this node has one.
			if (obj.exists ("parent"))
			{
				// In that case we already created the parent
				// and stored its resulting uuid in the
				// uuids[] array.
				if (uuids.exists (obj["parent"].sval()))
				{
					// so now we have the parent as a uuid
					parentid = uuids[obj["parent"].sval()];
				}
				
				if (skipparents.exists (parentid)) continue;
			}
			
			foreach (memb, obj["members"])
			{
				if (memb.sval().strncmp ("$REF$", 5) == 0)
				{
					value ll = strutil::split (memb, '$');
					string refclass = ll[2];
					string reflabel = ll[3];
					string refid = ll[4];
					
					string uuid =
						db.findobject ("", refclass, "", refid);
						
					DEBUG.storefile ("moduledb", "magicref",
									 uuid, "handlenewmodule");
						
					memb = uuid;
				}
			}
			
			// Hammer the dbmanager.
			uuid = db.createobject (parentid, obj["members"],
									oclass, metaid, false, true);
			
			if ((! uuid) && (db.getlasterrorcode() != ERR_DBMANAGER_EXISTS))
			{
				CORE->log (log::critical, "moduledb", "Could not "
						   "commit initial <%S> object in database: "
						   "%s" %format (oclass, db.getlasterror()));
						   
				cache["modules"].rmval (mname.str());
				throw (moduleInitException());
			}
			else if (! uuid)
			{
				uuid = db.findobject (parentid, oclass, nokey, metaid);
				skipparents[uuid] = true;
				continue;
			}
			
			value vdebug = $("parentid", parentid) ->
						   $("data", obj["members"]) ->
						   $("class", oclass) ->
						   $("metaid", metaid);
			
			DEBUG.storefile ("moduledb","dbman-args", vdebug, "handlenewmodule");
			
			// Henny penny the sky is falling!
			if (! uuid)
			{
				CORE->log (log::error, "moduledb",
						  "Error importing external data for "
						  "class <%S> with metaid <%S>",
						  oclass.str(), metaid.str());
				break;
			}
			else // AOK
			{
				// Keep the uuid around for posterity.
				uuids[localid] = uuid.sval();
			}
		}
		
		DEBUG.storefile ("moduledb","getconfig-uuids", uuids, "handlenewmodule");
	}
}

// ==========================================================================
// METHOD moduledb::parsetree
// ==========================================================================
void moduledb::parsetree (const value &tree, value &into, int parent)
{
	if (! into.count()) DEBUG.storefile ("moduledb", "in", tree, "parsetree");
	foreach (instance, tree)
	{
		int pos = into.count();
		into.newval() =
			$("class", tree.id()) ->
			$("id", pos);

		if (parent>=0) into[pos]["parent"] = parent;

		if (instance.id())
			into[pos]["metaid"] = instance.id().sval();
			
		foreach (member, instance)
		{
			if ((! member.attribexists ("type"))||(member("type") != "class"))
			{
				into[pos]["members"][member.id()] = member;
			}
			else
			{
				parsetree (member, into, pos);
			}
		}
	}
}

// ==========================================================================
// METHOD moduledb::createobject
// ==========================================================================
corestatus_t moduledb::createobject (const statstring &ofclass,
									 const statstring &withid,
									 const value &parm,
									 string &err)
{
	coremodule *m;
	corestatus_t res = status_failed;
	
	DEBUG.storefile ("moduledb", "parm", parm, "createobject");
	
	if (! byclass.exists (ofclass))
	{
		// FIXME: can we somehow get a better error to the session?
		err = "Class does not exist";
		return status_failed;
	}
	
	m = byclass[ofclass];
	if (! m)
	{
		err = "Module for class not found";
		CORE->log (log::critical, "moduledb", "Unexpected error trying to "
				   "resolve class <%S>", ofclass.str());
		ALERT->alert ("Error resolving <%S> in moduledb" %format (ofclass));
		return status_failed;
	}
	
	value outp;
	value returnp;

	outp = parm; // Picking up the proper parameters for the module
	             // shifted to the dbmanager.
	
	outp["OpenCORE:Command"] = "create";
	outp["OpenCORE:Session"]["classid"] = ofclass;
	
	if (! outp["OpenCORE:Session"].exists ("objectid"))
	{
		outp["OpenCORE:Session"]["objectid"] = withid;
	}

	res = m->action ("create", ofclass, outp, returnp);
	
	// FIXME: is it useful to keep returnp?
	// FIXME: report errors here or upstream?
	
	DEBUG.storefile ("moduledb", "returnp", returnp, "createobject");
	
	if (res != status_ok)
	{
		err.printf ("%i:%s", returnp["OpenCORE:Result"]["error"].ival(),
					returnp["OpenCORE:Result"]["message"].cval());
	}
	
	return res;
}

// ==========================================================================
// METHOD moduledb::makecrypt
// ==========================================================================
corestatus_t moduledb::makecrypt (const statstring &ofclass,
								  const statstring &fieldid,
								  const string &plaintext,
								  string &crypted)
{
	coremodule *m;
	corestatus_t res = status_failed;
	
	if (! byclass.exists (ofclass)) return status_failed;
	m = byclass[ofclass];
	if (! m)
	{
		CORE->log (log::critical, "moduledb", "Unexpected error trying to "
				   "resolve class <%S>", ofclass.str());
		ALERT->alert ("Unexpected error trying to resolve class <%S> in "
					 "moduledb" %format (ofclass));
		return status_failed;
	}

	value returnp;
	value ctx = $("OpenCore:Command", "crypt") ->
				$("OpenCore:Session", $("classid", ofclass)) ->
				$(fieldid, plaintext);

	res = m->action ("crypt", ofclass, ctx, returnp);
	
	if (res != status_ok) return res;
	
	crypted = returnp[fieldid].sval();
	return res;
}

// ==========================================================================
// METHOD moduledb::callmethod
// ==========================================================================
corestatus_t moduledb::callmethod (const statstring &parentid,
								   const statstring &ofclass,
								   const statstring &withid,
								   const statstring &method,
								   const value &param,
								   value &returnp,
								   string &err)
{
	corestatus_t res;
	coremodule *m;
	
	if (! byclass.exists (ofclass))
	{
		CORE->log (log::error, "moduledb", "Callmethod <%S:%S>: class "
				   "not found", ofclass.str(), method.str());
		
		return status_failed;
	}
	
	m = byclass[ofclass];
	if (! m)
	{
		CORE->log (log::critical, "moduledb", "Unexpected error trying to "
				   "resolve class <%S>", ofclass.str());
		ALERT->alert ("Unexpected error trying to resolve class <%S> in "
					 "moduledb" %format (ofclass));
		return status_failed;
	}
	
	value ctx =
		$("OpenCORE:Command", "callmethod") ->
		$("OpenCORE:Session",
			$("classid", ofclass) ->
		  	$("id", withid) ->
		  	$("parentid", parentid) ->
		  	$("method", method)) ->
		$merge(param);

	DEBUG.storefile ("moduledb","ctx", ctx, "callmethod");
	
	res = m->action ("callmethod", ofclass, ctx, returnp);

	if (res != status_ok)
	{
		err.printf ("%i:%s", returnp["OpenCORE:Result"]["error"].ival(),
					returnp["OpenCORE:Result"]["message"].cval());
	}
	
	return res;
}
									
// ==========================================================================
// METHOD moduledb::setspecialphysicalquota
// ==========================================================================
corestatus_t moduledb::setspecialphysicalquota
                                    (const statstring &tag,
									 const value &quota,
									 string &err)
{
	coremodule *m;
	corestatus_t res = status_failed;
	
	DEBUG.storefile ("moduledb", "quota", quota, "setspecialphysicalquota");

    string ofclass = tag;
    ofclass = ofclass.left(ofclass.strchr('/'));
    
	if (! byclass.exists (ofclass))
	{
		// FIXME: inadequate error reporting
		return status_failed;
	}
	
	m = byclass[ofclass];
	if (! m)
	{
		CORE->log (log::critical, "moduledb", "Unexpected error trying to "
				   "resolve class <%S>, specialquota tag <%S>", ofclass.str(), tag.str());
		ALERT->alert ("Unexpected error trying to resolve class <%S>, specialquota tag <%S> in "
					 "moduledb" %format (ofclass, tag));
		return status_failed;
	}
	
	value outp;
	value returnp;

	// FIXME: include class and tag
    outp << $("OpenCORE:Command", "setspecialphysicalquota") ->
            $("quota", quota);
    	
	res = m->action ("setspecialphysicalquota", tag, outp, returnp);
	
	if (res != status_ok)
	{
		err = "%[error]i:%[message]s" %format (returnp["OpenCORE:Result"]);
	}
	
	return res;
}

// ==========================================================================
// METHOD moduledb::updateobject
// ==========================================================================
corestatus_t moduledb::updateobject (const statstring &ofclass,
									 const statstring &withid,
									 const value &parm,
									 string &err)
{
	coremodule *m;
	corestatus_t res = status_failed;
	
	DEBUG.storefile ("moduledb", "parm", parm, "updateobject");

	if (! byclass.exists (ofclass))
	{
		// FIXME: inadequate error reporting
		return status_failed;
	}
	
	m = byclass[ofclass];
	if (! m)
	{
		CORE->log (log::critical, "moduledb", "Unexpected error trying to "
				   "resolve class <%S>", ofclass.str());
		ALERT->alert ("Unexpected error trying to resolve class <%S> in "
					 "moduledb" %format (ofclass));
		return status_failed;
	}
	
	value outp;
	value returnp;
	
	outp = parm; // Picking up the proper parameters for the module
	             // shifted to the dbmanager.
	             
	outp << $("OpenCORE:Command", "update") ->
			$("OpenCORE:Session",
				$("classid", ofclass) ->
				$("objectid", withid)
			 );
	
	res = m->action ("update", ofclass, outp, returnp);
	
	if (res != status_ok)
	{
		err = "%[error]i:%[message]s" %format (returnp["OpenCORE:Result"]);
	}
	
	return res;
}

// ==========================================================================
// METHOD moduledb::isdynamic
// ==========================================================================
bool moduledb::isdynamic (const statstring &forclass)
{
	if (! classexists (forclass)) return false;
	return getclass (forclass).dynamic;
}

// ==========================================================================
// METHOD moduledb::listdynamicobjects
// ==========================================================================
value *moduledb::listdynamicobjects (const statstring &parentid,
									 const statstring &ofclass,
									 string &err, int count, int offset)
{
	if (! classexists (ofclass)) return NULL;
	if (getclass (ofclass).dynamic == false) return NULL;
	coremodule *m = byclass[ofclass];
	
	err = "";

	value returnp;
	value outp = $("OpenCORE:Command", "listobjects") ->
				 $("OpenCORE:Session",
				 		$("parentid", parentid) ->
				 		$("classid", ofclass) ->
				 		$("count", count) ->
				 		$("offset", offset)
				  );
	
	corestatus_t rez = status_failed;
	rez = m->action ("listobjects", ofclass, outp, returnp);
	
	if (rez != status_ok)
	{
		CORE->log (log::error, "moduledb", "Error listing dynamic objects "
				   "for class <%S>", ofclass.str());
				   
		err = "%[error]i:%[message]s" %format (returnp["OpenCORE:Result"]);
		return NULL;
	}

	returnclass (value) res retain;
	res = returnp["objects"];
	DEBUG.storefile ("moduledb","res", res, "listdynamicobjects");
	return &res;
}

// ==========================================================================
// METHOD moduledb::listparamsformethod
// ==========================================================================
value *moduledb::listparamsformethod (const statstring &parentid,
									  const statstring &ofclass,
									  const statstring &withid,
									  const statstring &methodname)
{
	// Complain if it's an unknown class.
	if (! classexists (ofclass))
	{
		CORE->log (log::error, "moduledb", "Listparamsformethod called "
				   "for class=<%S>, class not found" %format (ofclass));
		
		return NULL;
	}
	
	// Get a reference to the class and look for the method.
	coreclass &cl = getclass (ofclass);
	if (! cl.methods.exists (methodname))
	{
		CORE->log (log::error, "moduledb", "Listparamsformethod called "
				   "for class=<%S> methodname=<%S>, method not found"
				   %format (ofclass, methodname));
		
		return NULL;
	}

	coremodule *m = byclass[ofclass];
	
	// Determine if the method is dynamic.
	if (cl.methods[methodname]("args") == "dynamic")
	{
		value returnp;
		value outp = $("OpenCORE:Command", "listparamsformethod") ->
					 $("OpenCORE:Session",
					 		$("parentid", parentid) ->
					 		$("classid", ofclass) ->
					 		$("objectid", withid) ->
					 		$("methodname", methodname)
					  );
		
		corestatus_t rez = status_failed;
		rez = m->action ("listparamsformethod", ofclass, outp, returnp);
		
		if (rez != status_ok)
		{
			CORE->log (log::error, "moduledb", "Error getting list of "
					   "parameters for a call to class=<%S> method=<%S> "
					   "parentid=<%S> objectid=<%S>" %format (ofclass,
					   methodname, parentid, withid));
			return NULL;
		}
		
		returnclass (value) res retain;
		// We need to make a fanciful translation here actually,
		// but first let's try to liberate the data and get it back
		// to the caller.
		res = returnp;
		return &res;
	}

	CORE->log (log::error, "moduledb", "Listparamsformethod called "
			   "for class=<%S> method=<%S>, not dynamic" %format (ofclass,
			   methodname));
	return NULL;
}

// ==========================================================================
// METHOD moduledb::synchronizeclass
// ==========================================================================
unsigned int moduledb::synchronizeclass (const statstring &ofclass,
								 unsigned int withserial)
{
	if (! classexists (ofclass))
	{
		CORE->log (log::error, "moduledb", "Got synchronization request "
				   "for class <%S> which does not exist" %format (ofclass));
		return withserial;
	}
	
	value returnp;
	coremodule *m = byclass[ofclass];
	
	value outp = $("OpenCORE:Command", "sync") ->
				 $("OpenCORE:Session",
				 		$("classid", ofclass) ->
				 		$("serial", withserial)
				  );
	
	corestatus_t rez = status_failed;
	rez = m->action ("sync", ofclass, outp, returnp);
	
	if (rez != status_ok)
	{
		CORE->log (log::error, "moduledb", "Error syncing class "
				  "<%S>" %format (ofclass));
		return withserial;
	}

	dbmanager db;
	db.iamgod ();
	
	foreach (update, returnp["objects"])
	{
		// Insert the object in context; figure out how.
	}
	
	return withserial;
}

// ==========================================================================
// METHOD moduledb::deleteobject
// ==========================================================================
corestatus_t moduledb::deleteobject (const statstring &ofclass,
									 const statstring &withid,
									 const value &parm,
									 string &err)
{
	coremodule *m;
	corestatus_t res = status_failed;

	DEBUG.storefile ("moduledb", "parm", parm, "deleteobject");

	if (! byclass.exists (ofclass))
	{
		// FIXME: better error handling.
		return status_failed;
	}
	
	m = byclass[ofclass];
	if (! m)
	{
		CORE->log (log::critical, "moduledb", "Unexpected error trying to "
				   "resolve class <%S>" %format (ofclass));
		ALERT->alert ("Unexpected error trying to resolve class <%S> in "
					 "moduledb" %format (ofclass));
		return status_failed;
	}
	
	value outp;
	value returnp;
	
	outp = parm; // Picking up the proper parameters for the module
	             // shifted to the dbmanager.
	             
	outp << $("OpenCORE:Command", "delete") ->
			$("OpenCORE:Session",
				$("classid", ofclass) ->
				$("objectid", withid)
			 );
			 
	res = m->action ("delete", ofclass, outp, returnp);
	
	if (res != status_ok)
	{
		err = "%[error]i:%[message]s" %format (returnp["OpenCORE:Result"]);
	}
	
	return res;
}

// ==========================================================================
// METHOD moduledb::getmeta
// ==========================================================================
value *moduledb::getmeta (const statstring &forclass)
{
	returnclass (value) res retain;
	
	if (! byclass.exists (forclass)) return &res;
	res = byclass[forclass]->meta;
	return &res;
}

// ==========================================================================
// METHOD moduledb::getcurrentconfig
// ==========================================================================
value *moduledb::getcurrentconfig (const statstring &forprimaryclass)
{
	coremodule *m;
	value *res;
	
	m = byclass[forprimaryclass];
	
	if (!m) return NULL;
	if (m->primaryclass != forprimaryclass) return NULL;
	
	res = m->getcurrentconfig ();
	DEBUG.storefile ("moduledb", "res", *res, "getcurrentconfig");
	return res;
}

// ==========================================================================
// METHOD moduledb::normalize
// ==========================================================================
bool moduledb::normalize (const statstring &forclass, value &data, string &err)
{
	if (! classexists (forclass))
	{
		err = "Unknown class: %s" %format (forclass);
		return false;
	}
	
	coreclass &C = getclass (forclass);
	return C.normalize (data, err);
}

// ==========================================================================
// METHOD moduledb::isadminclass
// ==========================================================================
bool moduledb::isadminclass (const statstring &classname)
{
	coremodule *m = getmoduleforclass (classname);
	if (! m) return false;
	return m->classes[classname].capabilities.attribexists ("admin");
}

// ==========================================================================
// METHOD moduledb::getclassinfo
// ==========================================================================
value *moduledb::getclassinfo (const statstring &classname, bool foradmin)
{
	returnclass (value) res retain;
	
	// Try to resolve the class name to a module.
	coremodule *m = getmoduleforclass (classname);

	// If we found something, process it.
	if (m)
	{
		res = m->classes[classname].makeclassinfo ();
		if (! res)
		{
			CORE->log (log::warning, "moduledb", "Empty result data "
					   "on asking for classinfo on <%S>" %format (classname));
					   
			return &res;
		}
		
		// Resolve the class to a coreclass object
		coreclass &C = m->classes[classname];
		
		// Read the requirement
		statstring pclass = C.requires;
		
		if (pclass) // Anything required?
		{
			// Resolve requirement class to module.
			coremodule *mm = getmoduleforclass (pclass);
			if (mm)
			{
				coreclass &PC = mm->classes[pclass];
				value &pinfo = res["info"]["parent"];
				
				pinfo << $("id", PC.name) ->
						 $("uuid", PC.uuid) ->
						 $("name", PC.shortname) ->
						 $("description", PC.description) ->
						 $("indexing", PC.manualindex ? "manual" : "auto") ->
						 $("module", mm->name);
			}
			else if (pclass != "ROOT")
			{
				CORE->log (log::error, "moduledb", "Error resolving module "
						   "for class <%S>" %format (pclass));
			}
		}
	}
	
	// Look at any possible child classes.
	foreach (cclass, byparent[classname])
	{
		if (isadminclass (cclass) && (! foradmin)) continue;
		// Resolve the child class to a module.
		coremodule *mm = getmoduleforclass (cclass.sval());
		if (mm) // Module found?
		{
			// Resolve to coreclass, get data.
			coreclass &C = mm->classes[cclass.sval()];
			
			res["info"]["children"][C.uuid] = $("id", cclass.sval()) ->
											  $("shortname", C.shortname) ->
											  $("menuclass", C.menuclass) ->
											  $("description", C.description);
		}
		else if (cclass != "ROOT") // This shouldn't happen.
		{
			CORE->log (log::error, "moduledb", "Error resolving module for "
					   "class <%S>" %format (cclass));
		}
	}

	// Return the happy news.	
	return &res;
}

// ==========================================================================
// METHOD moduledb::listmodules
// ==========================================================================
value *moduledb::listmodules (void)
{
	returnclass (value) res retain;
	coremodule *c;
	
	sharedsection (modlock)
	{
		c = first;
		while (c)
		{
			res.newval() = c->name;
			c = c->next;
		}
	}
	
	return &res;
}

// ==========================================================================
// METHOD moduledb::listclasses
// ==========================================================================
value *moduledb::listclasses (void)
{
	returnclass (value) res retain;
	
	sharedsection (modlock)
	{
		foreach (cv, classlist)
		{
			coreclass &cl = getclass (cv.id());
			res[cv.id()] = $("shortname", cl.shortname) ->
						   $("description", cl.description) ->
						   $("module", cl.module.name) ->
						   $("uuid", cl.uuid) ->
						   $("version", cl.version);
		}
	}
	
	return &res;
}

// ==========================================================================
// METHOD moduledb::Registermetasubclass
// ==========================================================================
void moduledb::registermetasubclass (const statstring &derivedid,
									 const statstring &baseid)
{
	metachildren[baseid][derivedid] = derivedid;
}

// ==========================================================================
// METHOD getmetaclasschildren
// ==========================================================================
const value &moduledb::getmetaclasschildren (const statstring &baseid)
{
	return metachildren[baseid];
}
