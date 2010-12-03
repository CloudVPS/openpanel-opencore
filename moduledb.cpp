// This file is part of OpenPanel - The Open Source Control Panel
// OpenPanel is free software: you can redistribute it and/or modify it 
// under the terms of the GNU General Public License as published by the Free 
// Software Foundation, using version 3 of the License.
//
// Please note that use of the OpenPanel trademark may be subject to additional 
// restrictions. For more information, please visit the Legal Information 
// section of the OpenPanel website on http://www.openpanel.com/

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
// CONSTRUCTOR ModuleDB
// ==========================================================================
ModuleDB::ModuleDB (void)
{
	first = last = NULL;
	
	InternalClasses.set ("OpenCORE:Quota", new QuotaClass);
	InternalClasses.set ("OpenCORE:ActiveSession", new SessionListClass);
	InternalClasses.set ("OpenCORE:ErrorLog", new ErrorLogClass);
	InternalClasses.set ("OpenCORE:System", new CoreSystemClass);
	InternalClasses.set ("OpenCORE:ClassList", new ClassListClass);
	InternalClasses.set ("OpenCORE:Wallpaper", new WallpaperClass);
}

// ==========================================================================
// METHOD ModuleDB::init
//
// TODO: detect double class registration
// ==========================================================================
void ModuleDB::init (const value &reloadmods)
{
	value cache;
	
	DBManager db;
	if (! db.init ())
	{
		log::write (log::critical, "ModuleDB", "Could not init database");
		return;
	}
	
	// Indicate to the database that we're bypassing the authorization layer
	// for this primary exercise.
	db.enableGodMode();

	// Load the module.cache file, we will use this to track module versions
	// between opencore runs.
	string cachepath = PATH_CACHES;
	cachepath.strcat ("/module.cache");
	
	// Make sure the module cache exists at all, if not, we will create
	// an empty cache.
	if (! fs.exists (cachepath))
	{
		log::write (log::info, "ModuleDB", "No module cache found, assuming "
				   "new install");
				   
		cache["modules"]; // Initialize cache's tree structure.
		
		// If we can't write, consider that a dealbreaker.
		if (! cache.saveshox (cachepath))
		{
			log::write (log::critical, "ModuleDB", "Can not write to module "
					   "cache at <%s>, bailing." %format (cachepath));
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
			log::write (log::warning, "ModuleDB", "Loaded module cache "
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
					loadModule (mname, cache, db);
				}
				catch (exception e)
				{
					log::write (log::error, "ModuleDB", "Error loading "
							   "'%s': %s" %format (mname, e.description));
					
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
// METHOD ModuleDB::loadModule
// ==========================================================================
void ModuleDB::loadModule (const string &mname, value &cache, DBManager &db)
{
	static value history;
	
	// Prevent double registration through dependencies
	if (history.exists (mname)) return;
	history[mname] = true;
	
	string path = PATH_MODULES "/%s" %format (mname);
	log::write (log::info, "ModuleDB", "Loading <%s>" %format (path));
	
	// Make sure the module exists at all.
	if (! fs.exists (path))
	{
		log::write (log::critical, "ModuleDB", "Couldn't load <%s>" %format(path));
		
		cache["modules"].rmval (mname);
		throw (moduleInitException("Module does not exist"));
	}
	
	// Create a new CoreModule object
	CoreModule *m = new CoreModule (path, mname, this);
	
	// Run the module's verify script. Refrain from loading it if the
	// verify failed.
	if (! m->verify ())
	{
		delete m;
		throw (moduleInitException ("Verify failed"));
	}
	
	// Merge languages.
	languages << m->languages;
	
	loadDependencies (mname, cache, db, m);
	
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
	
	bool firsttime = checkModuleCache (mname, cache, m);
	registerQuotas (m);
	registerClasses (mname, cache, db, m);
	createStagingDirectory (mname);
	
	if (firsttime) handleGetConfig (mname, cache, db, m);
}

// ==========================================================================
// METHOD ModuleDB::checkModuleCache
// ==========================================================================
bool ModuleDB::checkModuleCache (const string &mname, value &cache,
						   CoreModule *m)
{
	if (cache["modules"].exists (mname))
	{
		// Apperently it is, let's see if we are dealing with
		// a new version or some such nonsense.
		int cachedver = cache["modules"][mname].ival();
		int newver = m->meta["version"];
		
		if (cachedver > newver)
		{
			log::write (log::critical, "ModuleDB", "Version downgrade "
					   "detected on module <%s> current version = %i, "
					   "old version = %i" %format (mname,newver,cachedver));
					   
			throw (moduleInitException());
		}
		else if (cachedver < newver)
		{
			log::write (log::info, "ModuleDB", "New version of module "
					   "<%s> detected" %format (mname));
		
			if (! m->updateOK (cachedver))
			{
				log::write (log::critical, "ModuleDB", "Module <%s> "
						   "cannot deal with objects created by"
						   "version %i" %format (mname, cachedver));
				
				throw (moduleInitException());
			}
		}
		else
		{
			log::write (log::info, "ModuleDB", "Using cached version");
		}
		
		cache["modules"][mname] = m->meta["version"];
		byname[mname] = m;
	
		return false;
	}

	cache["modules"][mname] = m->meta["version"];
	byname[mname] = m;
	
	log::write (log::info, "ModuleDB", "No cache entry for "
			   "module <%s>" %format (mname));
			   
	DEBUG.storeFile ("ModuleDB", "miss", cache["modules"], "checkModuleCache");
	return true;
}

// ==========================================================================
// METHOD ModuleDB::loadDependencies
// ==========================================================================
void ModuleDB::loadDependencies (const string &mname, value &cache,
								 DBManager &db, CoreModule *m)
{
	// Does the module require another module?
	if (m->meta.exists ("requires"))
	{
		// Yes, recurse.
		log::write (log::info, "ModuleDB", "Loading required module <%S>"
				    %format (m->meta["requires"]));
		
		try
		{
			loadModule (m->meta["requires"], cache, db);
		}
		catch (exception e)
		{
			log::write (log::error, "ModuleDB", "Error loading required "
					   "module <%S>, disabling depending module <%S>"
					   %format (m->meta["requires"], mname));
			
			delete m;
			throw (moduleInitException ("Dependency failed"));
		}
	}
	
}

// ==========================================================================
// METHOD ModuleDB::registerQuotas
// ==========================================================================
void ModuleDB::registerQuotas (CoreModule *m)
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
// METHOD ModuleDB::createStagingDirectory
// ==========================================================================
void ModuleDB::createStagingDirectory (const string &mname)
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
// METHOD ModuleDB::registerClasses
// ==========================================================================
void ModuleDB::registerClasses (const string &mname, value &cache,
								DBManager &db, CoreModule *m)
{
	value childrendeps;
	
	/// Iterate over the CoreClass objects
	foreach (classobj,m->classes)
	{
		if (byclass.exists (classobj.name))
		{
			log::write (log::critical, "ModuleDB", "Class <%S> "
					   "already exists in module <%S>"
					   %format (classobj.name, mname));

			cache["modules"].rmval (mname);
			throw (moduleInitException());
		}
		if (byclassuuid.exists (classobj.uuid))
		{
			log::write (log::critical, "ModuleDB", "Class with "
					   "uuid <%S> already registered."
					   %format (classobj.uuid));
		}
		byclass[classobj.name] = m;
		byclassuuid[classobj.uuid] = m;
		classlist[classobj.name] = mname;
		
		// Does this class require another class?
		if (classobj.requires)
		{
			log::write (log::info, "ModuleDB", "Registered class <%s> "
					   "requires <%s>" %format (classobj.name, classobj.requires));
		
			// Yes, add this class to the byparent list.
			byparent[classobj.requires].newval() = classobj.name;
		}
		else
		{
			log::write (log::info, "ModuleDB", "Registered class <%S> "
					   "at root" %format (classobj.name));
			
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
		
		DEBUG.storeFile ("ModuleDB", "regdata", regdata, "registerClasses");
		
		// Send it to the database manager.
		if (! db.registerClass (regdata))
		{
			// Failed, croak.
			log::write (log::critical, "ModuleDB", "Could not register "
					   "class <%S> with DBManager: %s"
					   %format (classobj.name, db.getLastError()));

			sleep (2);
			cache["modules"].rmval (mname);
			throw (moduleInitException());
		}
	}

	// If we recorde a "childrendep" in a parent class, set the
	// 'cascades' flag for this child class so we'll know we have
	// to cascade any updates to it.
	foreach (cdep, childrendeps)
	{
		if (classExists (cdep.sval()))
		{
			log::write (log::info, "ModuleDB", "Setting cascade for "
					   "class <%S>" %format (cdep));
			getClass (cdep.sval()).cascades = true;
		}
	}
}

// ==========================================================================
// METHOD ModuleDB::handleGetConfig
// ==========================================================================
void ModuleDB::handleGetConfig (const string &mname, value &cache,
							    DBManager &db, CoreModule *m)
{
	value current; //< Return from the module call.
	value out; //< Translated for DBManager flattening.
	value uuids; //< Local-to-global id mapping for flattening.
	value skipparents; // parent-uuids that already existed
	
	log::write (log::info, "ModuleDB", "Getconfig for <%s>" %format (mname));
	
	// First let's get the currentconfig tree from the module.
	if (m->meta["implementation"]["getconfig"].bval())
	{
		current = m->getCurrentConfig();
		
		// Go over the type="object" nodes inside
		foreach (node, current)
		{
			// exclude the result node
			if (node.id() != "OpenCORE:Result")
			{
				// flatten into the 'out' array.
				parsetree (node, out, -1);
			}
		}
		
		DEBUG.storeFile ("ModuleDB","getconfig-tree", out, "handleGetConfig");
		
		// Iterate over the flattened out array
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
				statstring parent = obj["parent"];
				// In that case we already created the parent
				// and stored its resulting uuid in the
				// uuids[] array.
				if (uuids.exists (parent))
				{
					// so now we have the parent as a uuid
					parentid = uuids[parent];
				}
				else if (skipparents.exists (parent))
				{
					skipparents[localid] = true;
					continue;
				}
				else if (obj.exists ("parentclass"))
				{
					statstring parentclass = obj["parentclass"];
					parentid  = db.findObject (nokey, parentclass, nokey, parent);
					log::write (log::info, "ModuleDB", "Resolved %s/%s to "
							   "<%s>" %format (parentclass,parent,parentid));
				}
			}
			
			// Hammer the DBManager.
			uuid = db.createObject (parentid, obj["members"],
									oclass, metaid, false, true);
			
			if ((! uuid) && (db.getLastErrorCode() != ERR_DBMANAGER_EXISTS))
			{
				log::write (log::critical, "ModuleDB", "Could not "
						   "commit initial <%S> object in database: "
						   "%s" %format (oclass, db.getLastError()));
						   
				cache["modules"].rmval (mname);
				throw (moduleInitException());
			}
			else if (! uuid)
			{
				skipparents[metaid] = true;
				continue;
			}
			
			value vdebug = $("parentid", parentid) ->
						   $("data", obj["members"]) ->
						   $("class", oclass) ->
						   $("metaid", metaid);
			
			DEBUG.storeFile ("ModuleDB","dbman-args", vdebug, "handleGetConfig");
			
			// Henny penny the sky is falling!
			if (! uuid)
			{
				log::write (log::error, "ModuleDB",
						  "Error importing external data for "
						  "class <%S> with metaid <%S>"
						  %format (oclass, metaid));
				break;
			}
			else // AOK
			{
				// Keep the uuid around for posterity.
				uuids[localid] = uuid.sval();
			}
		}
		
		DEBUG.storeFile ("ModuleDB","getconfig-uuids", uuids, "handleGetConfig");
	}
}

// ==========================================================================
// METHOD ModuleDB::parsetree
// ==========================================================================
void ModuleDB::parsetree (const value &tree, value &into, int parent)
{
	if (! into.count()) DEBUG.storeFile ("ModuleDB", "in", tree, "parsetree");
	foreach (instance, tree)
	{
		int pos = into.count();
		into.newval() =
			$("class", tree.id()) ->
			$("id", pos);

		if (parent>=0) into[pos]["parent"] = parent;
		else if (instance.attribexists ("parent"))
		{
			into[pos]["parent"] = instance("parent");
			into[pos]["parentclass"] = instance("parentclass");
		}

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
// METHOD ModuleDB::createObject
// ==========================================================================
corestatus_t ModuleDB::createObject (const statstring &ofclass,
									 const statstring &withid,
									 const value &parm,
									 string &err)
{
	CoreModule *m;
	corestatus_t res = status_failed;
	
	DEBUG.storeFile ("ModuleDB", "parm", parm, "createObject");
	
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
		log::write (log::critical, "ModuleDB", "Unexpected error trying to "
				   "resolve class <%S>" %format (ofclass));
		ALERT->alert ("Error resolving <%S> in ModuleDB" %format (ofclass));
		return status_failed;
	}
	
	value outp;
	value returnp;

	outp = parm; // Picking up the proper parameters for the module
	             // shifted to the DBManager.
	
	outp["OpenCORE:Command"] = "create";
	outp["OpenCORE:Session"]["classid"] = ofclass;
	
	if (! outp["OpenCORE:Session"].exists ("objectid"))
	{
		outp["OpenCORE:Session"]["objectid"] = withid;
	}

	res = m->action ("create", ofclass, outp, returnp);
	
	// FIXME: is it useful to keep returnp?
	// FIXME: report errors here or upstream?
	
	DEBUG.storeFile ("ModuleDB", "returnp", returnp, "createObject");
	
	if (res != status_ok)
	{
		err = "%[error]i:%[message]s" %format (returnp["OpenCORE:Result"]); 
	}
	
	return res;
}

// ==========================================================================
// METHOD ModuleDB::makeCrypt
// ==========================================================================
corestatus_t ModuleDB::makeCrypt (const statstring &ofclass,
								  const statstring &fieldid,
								  const string &plaintext,
								  string &crypted)
{
	CoreModule *m;
	corestatus_t res = status_failed;
	
	if (! byclass.exists (ofclass)) return status_failed;
	m = byclass[ofclass];
	if (! m)
	{
		log::write (log::critical, "ModuleDB", "Unexpected error trying to "
				   "resolve class <%S>" %format (ofclass));
		ALERT->alert ("Unexpected error trying to resolve class <%S> in "
					 "ModuleDB" %format (ofclass));
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
// METHOD ModuleDB::callMethod
// ==========================================================================
corestatus_t ModuleDB::callMethod (const statstring &parentid,
								   const statstring &ofclass,
								   const statstring &withid,
								   const statstring &method,
								   const value &param,
								   value &returnp,
								   string &err)
{
	corestatus_t res;
	CoreModule *m;
	
	if (! byclass.exists (ofclass))
	{
		log::write (log::error, "ModuleDB", "Callmethod '%S.%S': class "
				   "not found" %format (ofclass, method));
		
		return status_failed;
	}
	
	m = byclass[ofclass];
	if (! m)
	{
		log::write (log::critical, "ModuleDB", "Unexpected error trying to "
				   "resolve class '%S'" %format (ofclass));
		ALERT->alert ("Unexpected error trying to resolve class <%S> in "
					 "ModuleDB" %format (ofclass));
		return status_failed;
	}
	
	CoreClass &cl = getClass (ofclass);
	
	if (! cl.methods.exists (method))
	{
		log::write (log::error, "ModuleDB", "Methodcall for %s.%s is "
					"not defined in module.xml" %format (ofclass,method));
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

	DEBUG.storeFile ("ModuleDB","ctx", ctx, "callMethod");
	
	res = m->action ("callmethod", ofclass, ctx, returnp);

	if (res != status_ok)
	{
		err = "%[error]i:%[message]s" %format (returnp["OpenCORE:Result"]); 
	}
	
	return res;
}
									
// ==========================================================================
// METHOD ModuleDB::setSpecialPhysicalQuota
// ==========================================================================
corestatus_t ModuleDB::setSpecialPhysicalQuota
                                    (const statstring &tag,
									 const value &quota,
									 string &err)
{
	CoreModule *m;
	corestatus_t res = status_failed;
	
	DEBUG.storeFile ("ModuleDB", "quota", quota, "setSpecialPhysicalQuota");

    string ofclass = tag;
    ofclass = ofclass.left(ofclass.strchr('/'));
    
	if (! byclass.exists (ofclass))
	{
		log::write (log::error, "ModuleDB", "Cannot resolve class for "
					"quota tag %S" %format (tag));
		return status_failed;
	}
	
	m = byclass[ofclass];
	if (! m)
	{
		log::write (log::critical, "ModuleDB", "Unexpected error trying to "
				   "resolve class <%S>, specialquota tag <%S>"
				   %format (ofclass, tag));
		ALERT->alert ("Unexpected error trying to resolve class <%S>, specialquota tag <%S> in "
					 "ModuleDB" %format (ofclass, tag));
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
// METHOD ModuleDB::updateObject
// ==========================================================================
corestatus_t ModuleDB::updateObject (const statstring &ofclass,
									 const statstring &withid,
									 const value &parm,
									 string &err)
{
	CoreModule *m;
	corestatus_t res = status_failed;
	
	DEBUG.storeFile ("ModuleDB", "parm", parm, "updateObject");

	if (! byclass.exists (ofclass))
	{
		// FIXME: inadequate error reporting
		return status_failed;
	}
	
	m = byclass[ofclass];
	if (! m)
	{
		log::write (log::critical, "ModuleDB", "Unexpected error trying to "
				   "resolve class <%S>" %format (ofclass));
		ALERT->alert ("Unexpected error trying to resolve class <%S> in "
					 "ModuleDB" %format (ofclass));
		return status_failed;
	}
	
	value outp;
	value returnp;
	
	outp = parm; // Picking up the proper parameters for the module
	             // shifted to the DBManager.
	             
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
// METHOD ModuleDB::classIsDynamic
// ==========================================================================
bool ModuleDB::classIsDynamic (const statstring &forclass)
{
	if (! classExists (forclass)) return false;
	return getClass (forclass).dynamic;
}

// ==========================================================================
// METHOD ModuleDB::listDynamicObjects
// ==========================================================================
value *ModuleDB::listDynamicObjects (const statstring &parentid,
									 const statstring &mparentid,
									 const statstring &ofclass,
									 string &err, int count, int offset)
{
	if (! classExists (ofclass)) return NULL;
	if (getClass (ofclass).dynamic == false) return NULL;
	CoreModule *m = byclass[ofclass];
	
	err = "";

	value returnp;
	value outp = $("OpenCORE:Command", "listobjects") ->
				 $("OpenCORE:Session",
				 		$("parentid", parentid) ->
				 		$("parentmetaid", mparentid) ->
				 		$("classid", ofclass) ->
				 		$("count", count) ->
				 		$("offset", offset)
				  );
	
	corestatus_t rez = status_failed;
	rez = m->action ("listobjects", ofclass, outp, returnp);
	
	if (rez != status_ok)
	{
		log::write (log::error, "ModuleDB", "Error listing dynamic objects "
				   "for class <%S>" %format (ofclass));
				   
		err = "%[error]i:%[message]s" %format (returnp["OpenCORE:Result"]);
		return NULL;
	}

	returnclass (value) res retain;
	res = returnp["objects"];
	DEBUG.storeFile ("ModuleDB","res", res, "listDynamicObjects");
	return &res;
}

// ==========================================================================
// METHOD ModuleDB::listParamsForMethod
// ==========================================================================
value *ModuleDB::listParamsForMethod (const statstring &parentid,
									  const statstring &ofclass,
									  const statstring &withid,
									  const statstring &methodname)
{
	// Complain if it's an unknown class.
	if (! classExists (ofclass))
	{
		log::write (log::error, "ModuleDB", "listParamsForMethod called "
				   "for class=<%S>, class not found" %format (ofclass));
		
		return NULL;
	}
	
	// Get a reference to the class and look for the method.
	CoreClass &cl = getClass (ofclass);
	if (! cl.methods.exists (methodname))
	{
		log::write (log::error, "ModuleDB", "listParamsForMethod called "
				   "for class=<%S> methodname=<%S>, method not found"
				   %format (ofclass, methodname));
		
		return NULL;
	}

	CoreModule *m = byclass[ofclass];
	
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
			log::write (log::error, "ModuleDB", "Error getting list of "
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

	log::write (log::error, "ModuleDB", "listParamsForMethod called "
			   "for class=<%S> method=<%S>, not dynamic" %format (ofclass,
			   methodname));
	return NULL;
}

// ==========================================================================
// METHOD ModuleDB::synchronizeClass
// ==========================================================================
unsigned int ModuleDB::synchronizeClass (const statstring &ofclass,
								 unsigned int withserial)
{
	if (! classExists (ofclass))
	{
		log::write (log::error, "ModuleDB", "Got synchronization request "
				   "for class <%S> which does not exist" %format (ofclass));
		return withserial;
	}
	
	value returnp;
	CoreModule *m = byclass[ofclass];
	
	value outp = $("OpenCORE:Command", "sync") ->
				 $("OpenCORE:Session",
				 		$("classid", ofclass) ->
				 		$("serial", withserial)
				  );
	
	corestatus_t rez = status_failed;
	rez = m->action ("sync", ofclass, outp, returnp);
	
	if (rez != status_ok)
	{
		log::write (log::error, "ModuleDB", "Error syncing class "
				  "<%S>" %format (ofclass));
		return withserial;
	}

	DBManager db;
	db.enableGodMode ();
	
	foreach (update, returnp["objects"])
	{
		// Insert the object in context; figure out how.
	}
	
	return withserial;
}

// ==========================================================================
// METHOD ModuleDB::deleteObject
// ==========================================================================
corestatus_t ModuleDB::deleteObject (const statstring &ofclass,
									 const statstring &withid,
									 const value &parm,
									 string &err)
{
	CoreModule *m;
	corestatus_t res = status_failed;

	DEBUG.storeFile ("ModuleDB", "parm", parm, "deleteObject");

	if (! byclass.exists (ofclass))
	{
		// FIXME: better error handling.
		return status_failed;
	}
	
	m = byclass[ofclass];
	if (! m)
	{
		log::write (log::critical, "ModuleDB", "Unexpected error trying to "
				   "resolve class <%S>" %format (ofclass));
		ALERT->alert ("Unexpected error trying to resolve class <%S> in "
					 "ModuleDB" %format (ofclass));
		return status_failed;
	}
	
	value outp;
	value returnp;
	
	outp = parm; // Picking up the proper parameters for the module
	             // shifted to the DBManager.
	             
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
// METHOD ModuleDB::getMeta
// ==========================================================================
value *ModuleDB::getMeta (const statstring &forclass)
{
	returnclass (value) res retain;
	
	if (! byclass.exists (forclass)) return &res;
	res = byclass[forclass]->meta;
	return &res;
}

// ==========================================================================
// METHOD ModuleDB::getCurrentConfig
// ==========================================================================
value *ModuleDB::getCurrentConfig (const statstring &forprimaryclass)
{
	CoreModule *m;
	value *res;
	
	m = byclass[forprimaryclass];
	
	if (!m) return NULL;
	if (m->primaryclass != forprimaryclass) return NULL;
	
	res = m->getCurrentConfig ();
	DEBUG.storeFile ("ModuleDB", "res", *res, "getCurrentConfig");
	return res;
}

// ==========================================================================
// METHOD ModuleDB::normalize
// ==========================================================================
bool ModuleDB::normalize (const statstring &forclass, value &data, string &err)
{
	if (! classExists (forclass))
	{
		err = "Unknown class: %s" %format (forclass);
		return false;
	}
	
	CoreClass &C = getClass (forclass);
	return C.normalize (data, err);
}

// ==========================================================================
// METHOD ModuleDB::isAdminClass
// ==========================================================================
bool ModuleDB::isAdminClass (const statstring &classname)
{
	CoreModule *m = getModuleForClass (classname);
	if (! m) return false;
	return m->classes[classname].capabilities.attribexists ("admin");
}

// ==========================================================================
// METHOD ModuleDB::getClassInfo
// ==========================================================================
value *ModuleDB::getClassInfo (const statstring &classname, bool foradmin)
{
	returnclass (value) res retain;
	
	// Try to resolve the class name to a module.
	CoreModule *m = getModuleForClass (classname);

	// If we found something, process it.
	if (m)
	{
		// Resolve the class to a CoreClass object
		CoreClass &C = m->classes[classname];
		
		res = C.makeClassInfo ();
		if (! res)
		{
			log::write (log::warning, "ModuleDB", "Empty result data "
					   "on asking for classinfo on <%S>" %format (classname));
					   
			return &res;
		}
		
		// Read the requirement
		statstring pclass = C.requires;
		
		if (pclass) // Anything required?
		{
			// Resolve requirement class to module.
			CoreModule *mm = getModuleForClass (pclass);
			if (mm)
			{
				CoreClass &PC = mm->classes[pclass];
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
				log::write (log::error, "ModuleDB", "Error resolving module "
						   "for class <%S>" %format (pclass));
			}
		}
	}
	
	// Look at any possible child classes.
	foreach (cclass, byparent[classname])
	{
		// Don't show 'admin' classes to ordinary users.
		if (isAdminClass (cclass) && (! foradmin)) continue;
		// Resolve the child class to a module.
		CoreModule *mm = getModuleForClass (cclass.sval());
		if (mm) // Module found?
		{
			// Resolve to CoreClass, get data.
			CoreClass &C = mm->classes[cclass.sval()];
			
			res["info"]["children"][C.uuid] = $("id", cclass.sval()) ->
											  $("shortname", C.shortname) ->
											  $("menuclass", C.menuclass) ->
											  $("description", C.description);
		}
		else if (cclass != "ROOT") // This shouldn't happen.
		{
			log::write (log::error, "ModuleDB", "Error resolving module for "
					   "class <%S>" %format (cclass));
		}
	}

	// Return the happy news.	
	return &res;
}

// ==========================================================================
// METHOD ModuleDB::listModules
// ==========================================================================
value *ModuleDB::listModules (void)
{
	returnclass (value) res retain;
	CoreModule *c;
	
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
// METHOD ModuleDB::listClasses
// ==========================================================================
value *ModuleDB::listClasses (void)
{
	returnclass (value) res retain;
	
	sharedsection (modlock)
	{
		foreach (cv, classlist)
		{
			CoreClass &cl = getClass (cv.id());
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
// METHOD ModuleDB::Registermetasubclass
// ==========================================================================
void ModuleDB::registerMetaSubClass (const statstring &derivedid,
									 const statstring &baseid)
{
	metachildren[baseid][derivedid] = derivedid;
}

// ==========================================================================
// METHOD getMetaClassChildren
// ==========================================================================
const value &ModuleDB::getMetaClassChildren (const statstring &baseid)
{
	static value none;
	if (! metachildren.exists (baseid))
	{
		log::write (log::error, "ModuleDB", "No implementations found for "
					"alleged meta-class <%S>" %format (baseid));
		DEBUG.storeFile ("ModuleDB", "metachildren", metachildren, "getMetaClassChildren");
		return none;
	}
	return metachildren[baseid];
}
