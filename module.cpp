// This file is part of OpenPanel - The Open Source Control Panel
// The Grace library is free software: you can redistribute it and/or modify it 
// under the terms of the GNU General Public License as published by the Free 
// Software Foundation, either version 3 of the License.
//
// Please note that use of the OpenPanel trademark may be subject to additional 
// restrictions. For more information, please visit the Legal Information 
// section of the OpenPanel website on http://www.openpanel.com/

#include "module.h"
#include <grace/xmlschema.h>
#include <grace/validator.h>
#include <grace/pcre.h>
#include "opencore.h"
#include "debug.h"
#include "alerts.h"

// ==========================================================================
// CONSTRUCTOR CoreClass
// ==========================================================================
CoreClass::CoreClass (void)
	: module (*(new CoreModule ("","",NULL)))
{
	// Should never be initialized without metadata, but a default
	// constructor is necessary if you want iteration to work.
	throw (invalidClassAccessException());
}

CoreClass::CoreClass (const value &imeta, CoreModule *p)
	: module (*p)
{
	// Copy all the metadata to the object. Keep in mind that we're
	// receiving a const value, so we're going to have to check for
	// certain values to exist if they're optional and not enforced
	// by the schema.
	name = imeta.id().sval();
	version = imeta["version"];
	uuid = imeta["uuid"].sval();
	cascades = false;
	
	#define DESERIALIZE(x) \
		if (imeta.exists (# x)) x = imeta[# x];
		
	#define DESERIALIZEFROM(x,y) \
		if (imeta.exists (# x)) y = imeta[# x];
		
	#define DEFDESERIALIZE(x,defl) \
		if (imeta.exists (# x)) x = imeta[# x]; \
		else x = defl;
		
	manualindex = (imeta["indexing"] == "manual");
	uniqueinparent = (imeta["uniquein"] == "parent");
	ismetabase = (imeta["metatype"] == "base");
	
	DEFDESERIALIZE (icon, "rsrc:module.png");
	DESERIALIZE (uniqueclass);
	DESERIALIZE (childrendep);
	DESERIALIZE (shortname);
	DESERIALIZE (title);
	DESERIALIZE (description);
	DESERIALIZE (menuclass);
	DESERIALIZE (capabilities);
	DESERIALIZE (methods);
	DEFDESERIALIZE (sortindex,50);
	DEFDESERIALIZE (hidegrid,false);
	DEFDESERIALIZE (formmargin,0);
	DEFDESERIALIZE (gridmargin,0);
	DEFDESERIALIZE (gridheight,100);
	DEFDESERIALIZE (emptytext,"No objects found");
	DEFDESERIALIZE (worldreadable,false);
	DEFDESERIALIZE (dynamic,false);
	DEFDESERIALIZE (allchildren, false);
	DEFDESERIALIZE (maxinstances, 0);
	DESERIALIZE (singleton);
	DESERIALIZE (parentrealm);
	DESERIALIZE (requires);
	DESERIALIZEFROM (parameters, param);
	DESERIALIZE (magicdelimiter);
	DESERIALIZE (prototype);
	DESERIALIZE (enums);
	DEFDESERIALIZE (hasprototype,false);
	
	foreach (p, param)
	{
		if ( (! p.attribexists ("regexp")) || (! p("regexp").sval()) )
		{
			p("regexp") = "^[\\x20-\\xFF]*";
		}
	}
	
	if (imeta.exists ("explanation") && imeta["explanation"].sval())
	{
		string fn = "%s/%s" %format (module.path, imeta["explanation"]);
		explanation = fs.load (fn);
	}
	
	if (imeta["metatype"] == "derived")
	{
		metabaseclass = imeta["metabase"];
		metadescription = imeta["metadescription"];
		
		// Register this class as one to query when requesting
		// a list of our meta-baseclass.
		module.mdb.registerMetaSubClass (name, metabaseclass);
	}
}

// ==========================================================================
// DESTRUCTOR CoreModule
// ==========================================================================
CoreClass::~CoreClass (void)
{
}

// ==========================================================================
// METHOD ::normalize
// ==========================================================================
bool CoreClass::normalize (value &mdata, string &error)
{
	// Go over the data elements
	foreach (node, mdata)
	{
		// Verify that the parameter was defined in the class definition.
		if (! param.exists (node.id()))
		{
			// That's a no.
			error = "Unknown parameter: ";
			error.strcat (node.id().sval());
			return false;
		}
		
		value &p = param[node.id()];
		if (p("regexp").sval() && node.sval())
		{
			string re = p("regexp");
			re = CORE->regexp (re);
			pcregexp RE (re);
			
			if (! RE.match (node.sval()))
			{
				error = "The data for parameter '%s' does not match "
						"the required regexp." %format (node.id());
				return false;
			}
		}
		if (p("type") == "enum")
		{
		    if (!checkEnum(p.id(), mdata[p.id()].sval()))
		    {
		        error = "Invalid value '%s' for enum "
                        "'%s'" % format(mdata[p.id()].sval(), p.id());
                return false;
		    }
		}
		
	}
	
	value dbug =
		$("mdata", mdata) ->
		$("param", param);

	DEBUG.storeFile ("CoreClass", "gathered", dbug, "normalize");
	
	foreach (p, param)
	{
		bool r;
		
		if (! normalizeLayoutNode (p, mdata, error))
			return false;
	}
	return true;
}

// ==========================================================================
// METHOD ::checkEnum
// ==========================================================================
bool CoreClass::checkEnum (const statstring &id, const string &val)
{
	if (! enums.exists (id))
	{
		log::write (log::warning, "CoreClass", "Got reference to "
				    "unknown enum '%S'" %format (id));
		return false;
	}
	
	foreach (num, enums[id])
	{
        log::write (log::debug, "Module", "checkEnum comparing: <%S> <%S>"
        			%format(num.id(), val));
		if (num.id() == val) return true;
	}
	
	return false;
}

// ==========================================================================
// METHOD ::normalizeLayoutNode
// ==========================================================================
bool CoreClass::normalizeLayoutNode (value &p, value &mdata, string &error)
{
	if (p.id() == "id") return true;
	if (! param.exists (p.id())) return true;
	
	if ((p.attribexists ("required")))
	{
		// If the parameter is not in the primary lay-out, it must be
		// grouped (negating many requirements).
		
		string nm;
		nm = "field-%s.%s" %format (name, p.id());
		DEBUG.storeFile ("CoreClass", nm, p, "normalizeLayoutNode");

		bool exists = mdata.exists (p.id()) && mdata[p.id()].sval();
		if ((! exists) && (! p("default").sval()))
		{
			DEBUG.storeFile ("CoreClass","req-no-default", p, "normalizeLayoutNode");
			error = "Required element with no default: %s" %format (p.id());
			return false;
		}
		// Does the data actually carry this field?
		if (! exists)
		{
			// No, we'll have to apply a default but keep the proper
			// internal type, so first let's determine the field's
			// data-type, then set a type-specific default value.
			string typ = param[p.id()]("type").sval();
			caseselector (typ)
			{
				incaseof ("string") : mdata[p.id()] = p("default").sval(); break;
				incaseof ("integer") : mdata[p.id()] = p("default").ival(); break;
				incaseof ("bool") : mdata[p.id()] = p("default").bval(); break;
				incaseof ("enum") :
					if (! checkEnum (p.id(), p("default")))
					{
						error = "Invalid default enum value: %s" %format (p("default"));
						return false;
					}
					
					mdata[p.id()] = p("default").sval();
					break;
					
				defaultcase :
					error = "Unknown type: %s" %format (typ);
					return false;
			}
		}
	}
	
	return true;
}

// ==========================================================================
// METHOD ::flattenParam
// ==========================================================================
value *CoreClass::flattenParam (void)
{
	returnclass (value) res retain;
	
	
	foreach (pobj, param)
	{
		value &p = res[pobj.id()];
		p["description"] = pobj.sval();
		p["type"] = pobj("type").sval();
		
		#define CONDCPBOOLATTR(zz) if (pobj.attribexists (zz)) p[zz] = (pobj(zz)=="true")
		#define CONDCPINTATTR(zz) if (pobj.attribexists (zz)) p[zz] = pobj(zz).ival()
		#define CONDCPSTRATTR(zz) if (pobj.attribexists (zz)) p[zz] = pobj(zz);
		
		CONDCPBOOLATTR("clihide");
		CONDCPINTATTR("cliwidth");
		CONDCPBOOLATTR("gridhide");
		CONDCPINTATTR("gridwidth");
		CONDCPSTRATTR("gridlabel");
		CONDCPINTATTR("textwidth");
		CONDCPBOOLATTR("sameline");
		CONDCPBOOLATTR("breakcolumn");
		CONDCPINTATTR("paddingtop");
		CONDCPINTATTR("labelwidth");
		CONDCPBOOLATTR("hidelabel");
		CONDCPINTATTR("rows");

		#undef CONDCPBOOLATTR
		#undef CONDCPINTATTR
		#undef CONDCPSTRATTR

		#define GETBOOLATTR(zz) pobj.attribexists (zz) ? pobj(zz).bval() : false

		p["enabled"] = GETBOOLATTR("enabled");
		p["visible"] = GETBOOLATTR("visible");
		p["required"] = GETBOOLATTR("required");
		
		#undef GETBOOLATTR
		
		string theregexp = pobj("regexp");
		if (theregexp) theregexp = CORE->regexp (theregexp);
		
		p["regexp"] = theregexp;
		p["example"] = pobj("example");
		p["default"] = pobj("default");
		p["tooltip"] = pobj("tooltip");
	}
	
	return &res;
}

// ==========================================================================
// METHOD ::flattenMethods
// ==========================================================================
value *CoreClass::flattenMethods (void)
{
	returnclass (value) res retain;
	
	if (methods.count()) foreach (m, methods)
	{
		// The 'args' attribute of a method definition determines whether
		// the parameters should be treated as dynamic (that is, the
		// module needs to look at the specific object and make a decision
		// on what parameters are needed to execute this method for the
		// object) or static (all instances of the class share the same
		// parameter definition as defined in module.xml).

		value &p = res[m.id()];

		p["description"] = m("description");
		if (m.attribexists ("args"))
		{
			p["type"] = m("args");
		}
		if (m.count())
		{
			// No matter what was defined, having actual parameters
			// defined _implies_ args="static".
			p["type"] = "static";
			
			// Go over the defined parameters.
			foreach (param, m)
			{
				value &P = p["param"][param.id()];
			
				P["description"] = param.sval();
				P["type"] = param("type").sval();
			}
		}
		else
		{
			p["args"] = "none";
		}
	}
	
	return &res;
}

// ==========================================================================
// METHOD ::flattenEnums
// ==========================================================================
value *CoreClass::flattenEnums (void)
{
	returnclass (value) res retain;
	
	res.type (t_dict);
	
	foreach (myenum, enums)
	{
		value &crsr = res[myenum.id()];
		foreach (myval, myenum)
		{
			crsr[myval.id()]["description"] = myval.sval();
			if (myval.attribexists ("val"))
				crsr[myval.id()]["val"] = myval("val");
		}
	}
	return &res;
}

// ==========================================================================
// METHOD ::makeClassInfo
// ==========================================================================
value *CoreClass::makeClassInfo (void)
{
	returnclass (value) res retain;
	
	value metachildren;
	if (ismetabase)
	{
		metachildren = module.mdb.getMetaClassChildren (name);
	}
	
	res =  $("structure",
				$("parameters", flattenParam()) ->
				$("methods", flattenMethods())
		    )->
		   $("capabilities",
				$("create", capabilities.attribexists ("create")) ->
				$("delete", capabilities.attribexists ("delete")) ->
				$("update", capabilities.attribexists ("update")) ->
				$("getinfo", capabilities.attribexists ("getinfo"))
			)->
		   $("enums", flattenEnums()) ->
		   $("class",
				$("id", name) ->
				$("uuid", uuid.sval()) ->
				$("shortname", shortname) ->
				$("title", title) ->
				$("menuclass", menuclass) ->
				$("description", description) ->
				$("explanation", explanation) ->
				$("emptytext", emptytext) ->
				$("sortindex", sortindex) ->
				$("gridheight", gridheight) ->
				$("hidegrid", hidegrid) ->
				$("formmargin", formmargin) ->
				$("gridmargin", gridmargin) ->
				$("version", version) ->
				$("magicdelimiter", magicdelimiter) ->
				$("parentrealm", parentrealm) ->
				$("prototype", prototype) ->
				$("singleton", singleton) ->
				$("metatype", ismetabase ? "base" :
								metabaseclass ? "derived" : "none") ->
				$("metabase", metabaseclass) ->
				$("metadescription", metadescription) ->
				$("metachildren", metachildren) ->
				$("indexing", manualindex ? "manual" : "auto")
			);

	DEBUG.storeFile ("CoreClass","res", res, "makeClassInfo");
	
	return &res;
}

// ==========================================================================
// METHOD ::getregistration
// ==========================================================================
value *CoreClass::getregistration (void)
{
	returnclass (value) res retain;
	
	foreach (p, param)
	{
		if (p.id() != "id")
		{
			res[p.id()]("type") = p("type");
			res[-1]("description") = p.sval();
		}
	}
	
	res("name") = name;
	res("uuid") = uuid;
	res("version") = version;
	res("indexing") = manualindex ? "manual" : "auto";
	if (manualindex) res("uniquein") = uniqueinparent ? "parent" : "class";
	res("shortname") = shortname;
	res("description") = description;
	
	if (menuclass) res("menuclass") = menuclass;
	if (uniqueclass) res("uniqueclass") = uniqueclass;
	if (childrendep) res("childrendep") = childrendep;
	if (requires) res("requires") = requires;
	if (allchildren) res("allchildren") = allchildren;
	if (parentrealm) res("parentrealm") = parentrealm;
	if (worldreadable) res("worldreadable") = worldreadable;
	if (maxinstances) res("maxinstances") = maxinstances;
	if (singleton) res("singleton") = singleton;
	if (magicdelimiter) res("magicdelimiter") = magicdelimiter;
	if (prototype) res("prototype") = prototype;
	
	DEBUG.storeFile ("CoreClass", "res", res, "getregistration");
	DEBUG.storeFile ("CoreClass", "uuid", uuid.sval(), "getregistration");
	return &res;
}

// ==========================================================================
// CONSTRUCTOR CoreModule
// ==========================================================================
CoreModule::CoreModule (const string &mpath, const string &mname,
						ModuleDB *pp) : mdb (*pp)
{
	string metapath;
	string xmlerr;
	path = mpath;
	name = mname;
	value enums;
	
	#define CRIT_FAILURE(foo) { \
			string err = foo; \
			CORE->log (log::critical, "Module", err); \
			CORE->delayedexiterror (err); \
			sleep (2); exit (1); \
		}
	
	// Don't allow html-style data with mixed tags, this is much more
	// likely to stem from a HTML error.
	defaults::xml::permitmixed = false;
	
	metapath = path;
	metapath.strcat ("/module.xml");
	
	if (! fs.exists ("schema:com.openpanel.opencore.module.schema.xml"))
		CRIT_FAILURE ("Installation problem: Could not load module schema");
	
	if (! fs.exists ("schema:com.openpanel.opencore.module.validator.xml"))
		CRIT_FAILURE ("Installation problem: Could not load validator schema");
	
	if (! fs.exists (metapath))
		CRIT_FAILURE ("Could not load <%s>" %format (metapath));
	
	xmlschema modschema ("schema:com.openpanel.opencore.module.schema.xml");
	validator modvalid ("schema:com.openpanel.opencore.module.validator.xml");
	
	if (! meta.loadxml (metapath, modschema, xmlerr))
		CRIT_FAILURE ("Error in '%s': %s" %format (metapath, xmlerr));
	
	if (! modvalid.check (meta, xmlerr))
		CRIT_FAILURE ("Error in '%s': %s" %format (metapath, xmlerr));
	
	DEBUG.storeFile ("CoreModule","loaded-meta", meta);
	
	if (meta["name"].sval() != mname)
	{
		CRIT_FAILURE ("Module name does not match directory name for "
					  "<%S>" %format (mpath));
	}
	
	apitype = meta["implementation"]["apitype"].sval();
	if (meta.exists ("enums")) enums = meta["enums"];
	
	// Create CoreClass objects from the classes array.
	foreach (cl, meta["classes"])
	{
		value v = cl;
		v["enums"] = enums;
		CoreClass *C = new CoreClass (v, this);
		classes.set (cl.id(), C);
		classesuuid.set (C->uuid, *C);
	}
	
	// Set at least one default language.
	languages["en_EN"] = true;
	if (meta.exists ("languages"))
	{
		languages = meta["languages"].attributes();
	}
	
	if (meta.exists ("license")) license = meta["license"];
	if (meta.exists ("author")) author = meta["author"];
	if (meta.exists ("url")) url = meta["url"];
	
	#undef CRIT_FAILURE
}

// ==========================================================================
// DESTRUCTOR CoreModule
// ==========================================================================
CoreModule::~CoreModule (void)
{
}

// ==========================================================================
// METHOD CoreModule::verify
// ==========================================================================
bool CoreModule::verify (void)
{
	value tmpa, tmpb;
	string mName = meta["name"];
	mName = mName.cutat (".module");
	
	if (API::execute (mName, "commandline", path, "verify", tmpa, tmpb))
	{
		string errstr;
		if (tmpb.exists ("OpenCORE:Result"))
		{
			errstr = tmpb["OpenCORE:Result"]["message"];
		}
		else
		{
			errstr = tmpb;
		}
		errstr = strutil::regexp (errstr, "s/\n$//;s/\n/ -- /g");
		log::write (log::error, "Module", "Verify failed for '%s': %s"
				    %format (path, errstr));
		return false;
	}
	return true;
}

// ==========================================================================
// METHOD CoreModule::action
// ==========================================================================
corestatus_t CoreModule::action (const statstring &command,
								 const statstring &classname,
								 const value &param,
								 value &returndata)
{
	int result;
	value vin = param;
	log::write (log::info, "Module", "Action class=<%S> "
			   			   "command=<%S>" %format (classname, command));

	DEBUG.storeFile ("CoreModule", "parm", vin, "action");

	string mName = meta["name"];
	mName = mName.cutat (".module");

    exclusivesection (serlock)
    {
    	// Modules with the wantsrpc attribute set to true want a
    	// sessionid to have their own little talk with opencore.
    	if (meta["implementation"]["wantsrpc"])
    	{
            value creds;
            CoreSession *usersession;
    	    CoreSession *modulesession = NULL;
    	    
        	try
        	{
        		modulesession = CORE->sdb->create (meta);
        	}
        	catch (exception e)
        	{
        		log::write (log::error, "rpc", "Exception caught while trying"
                    		"to create modulesession: %s" 
                    		%format (e.description));
                    		
                breaksection return status_failed;
            }
        
            usersession = CORE->sdb->get(vin["OpenCORE:Session"]["sessionid"]);
            usersession->getCredentials(creds);
            modulesession->setCredentials(creds);
            CORE->sdb->release(usersession);
        
            vin["OpenCORE:Session"]["sessionid"] = modulesession->id;
            result = API::execute (mName, apitype, path,
            					   "action", vin, returndata);
            					   
            CORE->sdb->release(modulesession);
            CORE->sdb->remove(modulesession);
    	}
    	else
    	{
            vin["OpenCORE:Session"].rmval("sessionid");
			result = API::execute (mName, apitype, path, "action",
								   vin, returndata);
    	}
    }
    
    return (corestatus_t) result;
}

// ==========================================================================
// METHOD getCurrentConfig
// ==========================================================================
value *CoreModule::getCurrentConfig (void)
{
	returnclass (value) res retain;
	value out;
	corestatus_t returnval;
	string mName = meta["name"];
	mName = mName.cutat (".module");
	
	out["OpenCORE:Command"] = "getconfig";
	DEBUG.storeFile ("CoreModule", "getconfig-param", out, "getCurrentConfig");
	
	returnval = (corestatus_t) API::execute (mName, apitype, path, "action",
											 out, res);
											 
	DEBUG.storeFile ("CoreModule", "getconfig-result", res, "getCurrentConfig");

	if (returnval != status_ok) res.clear();
	return &res;
}

// ==========================================================================
// METHOD CoreModule::reportUsage
// ==========================================================================
value *CoreModule::reportUsage (const statstring &id,
								int month, int year)
{
	return NULL; // stub
}

// ==========================================================================
// METHOD CoreModule::getResources
// ==========================================================================
value *CoreModule::getResources (void)
{
	return NULL; // stub
}

// ==========================================================================
// METHOD CoreModule::updateok
// ==========================================================================
bool CoreModule::updateOK (int currentversion)
{
	corestatus_t returnval;
	value out;
	value res;
	string mName = meta["name"].sval().copyuntil (".module");
	
	out["OpenCORE:Command"] = "updateok";
	out["OpenCORE:Session"]["currentversion"] = currentversion;
	
	returnval = (corestatus_t) API::execute (mName, apitype, path,
											 "updateok", out, res);
	
	if (returnval != status_ok) return false;
	if (res["OpenCORE:Result"]["code"] != 0) return false;
	log::write (log::info, "Module", "Update on %S ok" %format (meta["name"]));
	return true;
}
