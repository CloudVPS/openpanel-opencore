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

#include <grace/str.h>
#include <grace/strutil.h>
#include <grace/valueindex.h>
#include <grace/md5.h>
#include <sqlite3.h>

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include "dbmanager.h"
#include "opencore.h"
#include "debug.h"
#include "error.h"

// TODO: check that we handle all relevant table columns at all places
// TODO: make sure 'wanted' and 'reality' are in all the necessary WHERE-clauses

// TODO: handle SQLITE3_BUSY, possibly as simply as sqlite3_busy_timeout() plus returning the error upwards

// TODO: when a reffed object/member changes, we need to smartly update -towards the modules- all other objects that reffed it
// TODO: schema support
// TODO: possibly consolidate fetch+find
// TODO: be smart about updating 'parent' and 'uniquecontext' refs when the ref'ed object is updated; i am this close >< to using UUID for those instead of localid

// TODO: convert all powermirror sql usage to the right kind of join

static lock<sqlite3*> dbhandle;
static bool dbinitdone = false;
static xmlschema schema;

void _dbmanager_sqlite3_trace_rcvr(void *ignore, const char *query)
{
	CORE->log (log::debug, "dbmgr", "sqlite3_trace: %s", query);
}

DBManager::DBManager (void)
{
	lasterror = "";
	errorcode = 0;
	god = false;
	userid = -1;
}

DBManager::~DBManager (void)
{
	// does nothing right now, don't forget to call deinit!
}

bool DBManager::init (char *dbfile)
{
    exclusivesection (dbhandle)
    {
        if(!dbinitdone)
        {
            if(sqlite3_open(dbfile, &dbhandle) != SQLITE_OK)
        	{
        		// FIXME: need consistent reporting upwards
        		lasterror = "sqlite3_open(%s) failed: %s" %format (dbfile, sqlite3_errmsg(dbhandle));
        		errorcode = ERR_DBMANAGER_INITFAIL;
        		sqlite3_close(dbhandle);
        		dbhandle = NULL;
        		breaksection return false;
        	}

        	if(!checkschema())
        	{
        		lasterror="Schema error on db init!";
        		errorcode = ERR_DBMANAGER_INITFAIL;
        		breaksection return false;
        	}
        	sqlite3_trace(dbhandle, _dbmanager_sqlite3_trace_rcvr, NULL);

        	schema.load("schema:sqlite.compact.schema.xml");
            dbinitdone = true;
        }
    }
    return true;
}

void DBManager::deinit ()
{
	// close dbhandle, free associated resources
    // sqlite3_close(dbhandle);
}

string *DBManager::findParent (const statstring &uuid)
{
	returnclass (string) res retain;

	string query = "SELECT /* findParent */ parent FROM objects WHERE ";
	value where;
	where["uuid"]=uuid;
    where["wanted"]=1;
	query.strcat(escapeforsql("=", " AND ", where));
	value dbres = dosqlite(query);
	CORE->log(log::debug, "dbmgr", "findParent SQL: %s" %format (query));
	if(!dbres["rows"].count())
	{
		lasterror = "Object not found in findParent";
		errorcode = ERR_DBMANAGER_NOTFOUND;
		res.clear();
		return &res;
	}

    if(!haspower(dbres["rows"][0]["parent"].ival(), userid))
    {
        lasterror = "Permission denied";
        errorcode = ERR_DBMANAGER_NOPERM;
        res.clear();
        return &res;
    }
	query = "";
	query.printf("SELECT /* findParent */ uuid FROM objects WHERE id=%d", dbres["rows"][0]["parent"].ival());
	dbres=dosqlite(query);
	if(!dbres["rows"].count())
	{
		lasterror = "Parent object not found; unpossible!";
		errorcode = ERR_DBMANAGER_FAILURE;
		res.clear();
		return &res;
	}
	res=dbres["rows"][0]["uuid"].sval();
		
	return &res;
}

int DBManager::findObjectDeletedVersion (int uc, const statstring &withmetaid)
{
	string query;
	value where;

	query="SELECT /* findObjectDeletedVersion */ id,version,deleted FROM objects WHERE ";
	where["uniquecontext"]=uc;
	where["metaid"]=withmetaid;
	
	query.strcat(escapeforsql("=", " AND ", where));
	query.strcat(" ORDER BY version DESC LIMIT 1");
	value v = dosqlite(query);
	DEBUG.storeFile("dbmgr", "dbres", v, "findObjectDeletedVersion");
	
	if(v["rows"].count() == 0)
	{
		return 0;
	}
	
	if(!v["rows"][0]["deleted"].ival())
	{
		lasterror = "An object with this id already exists";
		errorcode = ERR_DBMANAGER_EXISTS;
		DEBUG.storeFile("dbmgr","errorcontext", v, "findObjectDeletedVersion");
		return -1;
	}

	return v["rows"][0]["version"].ival();
}

string *DBManager::findObject (const statstring &parent, const statstring &ofclass, const statstring &withuuid, const statstring &withmetaid)
{
	returnclass (string) res retain;
	string query;
	value where;
	int classid;
	
	CORE->log(log::debug, "dbmgr", "findObject(parent=%s, ofclass=%s, "
			  "withuuid=%s, withmetaid=%s)" %format (parent, ofclass,
			   withuuid, withmetaid));

	classid=findclassid(ofclass); // TODO: handle 'class not found'
	if(ofclass && !classid)
	{
		lasterror = "Class %s not found" %format (ofclass);
		errorcode = ERR_DBMANAGER_NOTFOUND;
		res.clear();
		return &res;
	}

	query="SELECT /* findObject */ id, uuid FROM objects WHERE ";
	if(classid)
		where["class"]=classid;
	// FIXME: this bool, bool, compare to "" stuff is retarded
	if(withuuid) where["uuid"]=withuuid;
	if(withmetaid) where["metaid"]=withmetaid;
	if(parent != "") where["parent"]=findlocalid(parent);
	
	if(!withuuid && !withmetaid && parent == "")
	{
		errorcode = ERR_DBMANAGER_INVAL;
	    lasterror = "Not enough predicates passed";
        res.clear();
        return &res;
	}
	
    where["wanted"]=1;
	query.strcat(escapeforsql("=", " AND ", where));
	query.strcat(" ORDER BY version DESC LIMIT 1");
	CORE->log(log::debug, "dbmgr", "findObject: classid: %d" %format (classid));
	value v = dosqlite(query);
	DEBUG.storeFile("dbmgr", "dbres", v, "findObject");
	
	// query="SELECT objects.id,version,classes.name,reality,wanted FROM objects,classes WHERE objects.class=classes.id AND ";
	// where["parent"]=contextid;
	// where["uniqid"]=withid;
	// where["classes.name"]=ofclass;
	// query.strcat(escapeforsql("=", " AND ", where));
	// 	
	// printf("bind: query = %s\n", query.str());
	// dosqlite(query);
	
    if(v["rows"].count())
	{
		res=v["rows"][0]["uuid"].sval();
		if(!god && !haspower(v["rows"][0]["id"].ival(), userid) && !(v["rows"][0]["id"].ival()==userid) && !classhasattrib(classid, "worldreadable"))
		{	
			lasterror = "Object found but access denied";
			errorcode = ERR_DBMANAGER_NOPERM;
			res.clear();
			return &res;
		}
		return &res;
	}
	else
	{
		lasterror = "Object not found";
		errorcode = ERR_DBMANAGER_NOTFOUND;
		res.clear();
		return &res;
	}
	lasterror = "Pigs fly";
	errorcode = ERR_DBMANAGER_FAILURE;
	return &res;
}

bool DBManager::listObjectTree (value &into, const statstring &uuid)
{
	int localid;
	
	localid = findlocalid(uuid);
	if(!localid)
		return false;
		
	if(!haspower(localid, userid))
	{
        lasterror = "Permission denied";
        errorcode = ERR_DBMANAGER_NOPERM;
        return false;
	}

	if(!_listObjectTree(into, localid))
		return false;

	into.newval() = uuid;
	return true;
}


bool DBManager::_listObjectTree (value &into, int localid)
{
	string query;
	
	if(!localid)
	{
		lasterror = "Cowardly refusing to recurse from empty parentid";
		errorcode = ERR_DBMANAGER_INVAL;
		return false;
	}
	query.printf("SELECT /* _listObjectTree */ id, uuid FROM objects WHERE (parent=%d OR owner=%d) AND wanted=1 AND deleted=0", localid, localid);
	value qres = dosqlite(query);
	if(!qres)
		return false;

	foreach(row, qres["rows"])
	{
		if(!_listObjectTree(into, row["id"].ival()))
			return false;
		
		into.newval() = row["uuid"];
	}
	
	return true;
}

bool DBManager::listObjects (value &into, const statstring &parent, const value &ofclass, bool formodule, int count, int offset)
{
	value where;

	if(parent != nokey && parent != "")
	{
	    where["o.parent"]=findlocalid(parent);
	}
	else
	{
	    if (ofclass == nokey)
	    {
            where["o.parent"]=0;
        }
	}
		
	where["o.wanted"]=1;
	where["o.deleted"]=0;
	if(!god)
        where["p.powerid"]=userid;

    // TODO: check that this doesn't return objects more than once, like getquotausage did before opencore@3c2367c81cb6
	string query="SELECT /* listObjects */ o.id id, o.class class, o.content content, o.metaid metaid, o.uuid uuid, o.owner ownerid, o2.uuid parentuuid, o3.uuid owneruuid FROM powermirror p, objects o LEFT JOIN objects o2 ON o.parent=o2.id LEFT JOIN objects o3 ON o.owner=o3.id WHERE (o.owner=p.userid OR o.id=p.powerid) AND ";
	query.strcat(escapeforsql("=", " AND ", where));
	if (ofclass != nokey)
    {
        query.strcat(" AND o.class IN(");
        bool firstiter=true;
    	foreach (classname, ofclass)
    	{
    	    if(firstiter)
    			firstiter = false;
    		else
    			query.strcat(",");
			
        	if(ofclass != nokey && ofclass != "")
                query.strcat("%d" %format (findclassid(ofclass)));
    	}

        query.strcat(" )");
    }
    
	query.printf(" ORDER BY o.metaid LIMIT %d,%d", offset, count);
	value dbres = dosqlite(query); // FIXME: handle failure
    // value childclasses;
    // if(!formodule)
    // {
    //  query="SELECT /* listObjects */ o.id id, o3.metaid childclass FROM powermirror p, objects o, objects o2, objects o3 WHERE o.id=o2.parent AND o2.class=o3.id AND ";
    //         where.rmval("p.powerid");
    //  query.strcat(escapeforsql("=", " AND ", where));
    //  value ccres = dosqlite(query);
    //  foreach(row, ccres["rows"])
    //  {
    //      childclasses[row["id"]][row["childclass"]] = 1;
    //  }
    // }
//	res=dbres["rows"];
	foreach(row, dbres["rows"])
	{
		value resrow;
        // this checking has been moved to an sql join; 
        // TODO: that join does deny access to worldreadable classes, however.
        // if(formodule)
        // {
        //  // OK!
        // }
        // else if(classhasattrib(row["class"].ival(), "worldreadable"))
        // {
        //  // OK!
        // }
        // else
        // {
        //  CORE->log(log::debug, "dbmgr", "(%s) listObjects: not owner or no class rights, skipping object %d", DEBUG.uuid().cval(), row["id"].ival());
        //  continue;
        // }

		int localclassid = row["class"].ival();
		if(formodule)
		{
			value tmpv;
			tmpv=deserialize(row["content"]);

			if(!deref(tmpv, localclassid))
			{
				into.clear();
				return false;
			}
			resrow = tmpv;
		}
		else
		{	
		    if(god)
                resrow=deserialize(row["content"]);
			else
			    resrow=hidepasswords(deserialize(row["content"]), row["class"]);
		}
		
		resrow("type")="object";
		resrow["class"]=_classNameFromUUID(row["class"]); // FIXME: handle failure
		resrow["uuid"]=row["uuid"];
		if(row["parentuuid"].sval().strlen())
			resrow["parentid"]=row["parentuuid"];
		if(row["owneruuid"].sval().strlen())
			resrow["ownerid"]=row["owneruuid"];

		if(row["metaid"].sval().strlen())
		{
			resrow["id"]=row["metaid"];
			resrow["metaid"]=row["metaid"];
		}
		else
		{
			resrow["id"]=row["uuid"];
		}

        // if(!formodule)
        // {
        //  resrow["childclasses"] = childclasses[row["id"]];
        // }
		into[resrow["class"]][resrow["id"]]=resrow;
		into[resrow["class"]]("type")="class";
		
		if(formodule && classhasattrib(row["class"].ival(), "allchildren"))
		{	
			CORE->log(log::debug, "dbmgr", "listObjects: doing "
					  "allchildren (%s)" %format (row["uuid"]));
			value mergev;
			if(listObjects(mergev, row["uuid"], nokey, formodule))
			{
				into[resrow["class"]][resrow["id"]] << mergev;
			}
			else
			{
				return false;
			}
		}
		if(formodule && classhasattrib(row["class"].ival(), "childrendep"))
		{	
			string depclass = classgetattrib(row["class"].ival(), "childrendep");
			CORE->log(log::debug, "dbmgr", "listObjects: doing childrendep "
					  "(%s, %s)" %format (depclass, row["uuid"]));
			value mergev;
			if(listObjects(mergev, row["uuid"], $(depclass), formodule))
			{
				into[resrow["class"]][resrow["id"]] << mergev;
			}
			else
			{
				return false;
			}
		}
	}
	
	DEBUG.storeFile("dbmgr", "res", into, "listObjects");
	
	return true;
}

statstring *DBManager::classNameFromUUID(const statstring &uuid)
{
	returnclass (statstring) res retain;
	
	string query="SELECT /* classNameFromUUID */ class,metaid FROM objects WHERE ";
	value where;
	where["uuid"]=uuid;
	query.strcat(escapeforsql("=", " AND ", where));
	value dbres=dosqlite(query);
	if(!dbres)
	{
		res.clear();
		return &res;
	}
	
	if(!dbres["rows"].count())
	{
		lasterror = "Object not found in classNameFromUUID";
		errorcode = ERR_DBMANAGER_NOTFOUND;
		res.clear();
		return &res;
	}
	
	if(dbres["rows"][0]["class"].ival() == 1)
	{
		res = dbres["rows"][0]["metaid"].sval();
	}
	else
	{
		res = _classNameFromUUID(dbres["rows"][0]["class"].ival());
	}
	
	return &res;
}

// process:
// 1. stick requested object in output tree
// 2. recurse upwards wherever class says 'requires', stick object in output tree (at root)
// 3. when we encounter allchildren, stick a listObjects -inside- the current object
// done!
// NOTE: coresession absolutely relies on into[0] being the object the request
// actually pointed to
bool DBManager::fetchObject (value &into, const statstring &uuid, bool formodule)
{
	int localid;
	string module="";
	string reqclass="";
	
	localid=findlocalid(uuid);

	if(!god && !haspower(localid, userid) && !classhasattrib(findclassid(classNameFromUUID(uuid)), "worldreadable") && !formodule)
	{
		lasterror = "Permission denied for object %s" %format (uuid);
		errorcode = ERR_DBMANAGER_NOPERM;
		into.clear();
		return false;
	}

	while(1)
	{
		// if this object's class has a required-attribute, recurse upwards
		// and check it.
		string query;
		query.printf("SELECT /* fetchObject */ o.class class, o.parent parent, o.content content, o.metaid metaid, o.uuid uuid, o.version version, o.owner owner, o.deleted deleted, o2.uuid parentuuid, o3.uuid owneruuid FROM objects o LEFT JOIN objects o2 ON o.parent=o2.id LEFT JOIN objects o3 ON o.owner=o3.id WHERE o.id=%d", localid);

		value dbres = dosqlite(query);
		if(!dbres["rows"].count())
		{
			errorcode = ERR_DBMANAGER_NOTFOUND;
			lasterror = "Object (localid=%i) not found while "
						"recursing upwards" %format (localid);
			into.clear();
			return false;
		}
		value row=dbres["rows"][0];
		DEBUG.storeFile("dbmgr", "objectrow", row, "fetchObject");

		string id = _classNameFromUUID(row["class"]);
		
		if(reqclass && reqclass != id)
		{
			errorcode = ERR_DBMANAGER_NOTFOUND;
			lasterror = "Found parent is not of required class";
			into.clear();
			return false;
		}

		// query.crop();
		// query.printf("SELECT /* fetchObject */ content FROM objects WHERE id=%d", row["class"].ival());
		// value classdbres = dosqlite(query);
		// value classdata;
		// classdata.fromxml(classdbres["rows"][0]["content"].sval());
		value classdata = getClassData(row["class"].ival());
		DEBUG.storeFile("dbmgr", "classdata", classdata, "fetchObject");
		
		// stick it to me!
		// because class A cannot require class B, indexing on classname
		// here should be completely safe
		// FIXME: check access
		if(module == classdata("modulename").sval())
		{	
			if(formodule)
			{
				value tmp=deserialize(row["content"]);
				if(deref(tmp, row["class"]))
				{
					into[id]=tmp;
				}
				else
				{
					into.clear();
					return false;
				}
			}
			else
			{
				into[id]=hidepasswords(deserialize(row["content"]), row["class"], true);
			}
		}
		else
		{
			if(formodule)
			{
				value tmpv;
				tmpv=deserialize(row["content"]);
				if(!deref(tmpv, row["class"]))
				{
					string tmp;
					tmp=lasterror;
					lasterror = "fetchObject (!inmodule, formodule) deref -> ";
					lasterror.strcat(tmp);
					into.clear();
					return false;
				}
				into[id]=filter(tmpv, row["class"]);
			}
			else
			{
				value tmpv;
				tmpv=deserialize(row["content"]);
				into[id]=hidepasswords(filter(tmpv, row["class"]), row["class"], true);
			}
		}

		into[id]["uuid"]=row["uuid"];
		into[id]("type")="object";
		into[id]["version"]=row["version"];
		if(row["deleted"].ival())
			into[id]["deleted"]=1;
		into[id]("owner")=_findmetaid(row["owner"]);
		if(row["parentuuid"].sval().strlen())
			into[id]["parentid"]=row["parentuuid"];
		if(row["owneruuid"].sval().strlen())
			into[id]["ownerid"]=row["owneruuid"];
		
		if(row["metaid"].sval().strlen())
		{
			into[id]["id"]=row["metaid"];
			into[id]["metaid"]=row["metaid"];
		}
		else
		{
			into[id]["id"]=row["uuid"];
		}

		if(!module.strlen())
		{
			module=classdata("modulename");
		}
		
		if (classdata.attribexists ("requires"))
		{
			reqclass = classdata ("requires");
		}
		else
		{
			reqclass = "";
		}
		
		if(formodule && classdata.attribexists("allchildren") && module == classdata("modulename").sval())
		{	
			CORE->log(log::debug, "dbmgr", "fetchObject: doing "
					  "allchildren (%s)" %format (row["uuid"]));
					  
			value mergev;
			if(listObjects(mergev, row["uuid"], nokey, formodule))
			{
				into[id] << mergev;
			}
			else
			{
				return false;
			}
		}
		if(formodule && classdata.attribexists("childrendep"))
		{	
			DEBUG.storeFile("dbmgr", "childrendep", classdata, "fetchObject");
			CORE->log(log::debug, "dbmgr", "fetchObject: doing childrendep "
					 "(%s, %s)" %format (classdata("childrendep"), row["uuid"]));
					 
			value mergev;
			if(listObjects(mergev, row["uuid"], $(classdata("childrendep")), formodule))
			{
				into[id] << mergev;
			}
			else
			{
				return false;
			}
		}
        // if(!formodule)
        //          {   
        //                  value childclasses;
        //                  query.crop();
        //                  query.printf("SELECT /* fetchObject */ o.id id, o3.metaid childclass FROM objects o, objects o2, objects o3 WHERE o.id=o2.parent AND o2.class=o3.id AND o.id=%d", localid);
        //              value ccres = dosqlite(query);
        //              foreach(row, ccres["rows"])
        //              {
        //                  into[id]["childclasses"][row["childclass"]] = 1;
        //              }
        //          }
		if(!formodule || !classdata.attribexists("requires") || !row["parent"].ival())
		{
			CORE->log(log::debug, "dbmgr", "fetchObject: NOT recursing "
					 "upward from %d" %format(localid));
			break;
		}
		CORE->log(log::debug, "dbmgr", "fetchObject: recursing upward "
				  "from %i to %i" %format (localid, row["parent"]));
		localid = row["parent"].ival();
	}
	
	DEBUG.storeFile("dbmgr", "result", into, "fetchObject");
	
	return true;
}

bool DBManager::userisgone()
{
    value where;
    string q;
    
    if (god)
        return false;

    q.printf("SELECT /* userisgone */ id FROM objects WHERE ");
	where["id"]=userid;
	where["wanted"]=1;
	q.strcat(escapeforsql("=", " AND ", where));
	value dbres = dosqlite(q);

    if (dbres["rows"].count() == 0)
    {
        errorcode = ERR_DBMANAGER_LOGINFAIL;
        lasterror = "Your User object was changed, please log out";
        return true;
    }
    
    return false;
}

string *DBManager::createObject(const statstring &parent, const value &withmembers, const statstring &ofclass, const statstring &metaid, bool permcheck, bool immediate)
{
	returnclass (string) res retain;
	value v;
	string query;
	int classid, parentid=0, ownerid=-1, oldv=0;
	value members;
	
	if (metaid == "$prototype$" && !god)
	{
        lasterror = "Prototypes can only be created by modules";
        errorcode = ERR_DBMANAGER_NOPERM;
        return &res;
	}

	if (userisgone())
	{
        return &res;
	}

	members = withmembers;
	
	CORE->log(log::debug, "dbmgr", "createObject(parent=%s, [members], "
			 "ofclass=%s, metaid=%s)" %format (parent, ofclass, metaid));
	DEBUG.storeFile("dbmgr", "members", withmembers, "createObject");
	
	classid=findclassid(ofclass);  // FIXME: handle failure
	
	if(!checkfieldlist(members, classid))
	{
		return &res;
	}

	string classuuid = findObject("", "Class", nokey, ofclass);
	// TODO: check user quota 

  value classdata = getClassData(classid);

	if(parent)
	{
		string q;
		q.printf("SELECT /* createObject parentid */ id,owner FROM objects WHERE ");
		value where;
		where["uuid"]=parent;
		where["wanted"]=1;
		q.strcat(escapeforsql("=", " AND ", where));
		value uuiddbres = dosqlite(q);
		parentid=uuiddbres["rows"][0]["id"].ival();
        ownerid=uuiddbres["rows"][0]["owner"].ival();
	}
	v["parent"]=parentid;

	// find uniqueness context
	if(classdata.attribexists("uniquein"))
	{
		caseselector (classdata("uniquein").sval())
		{
			incaseof("parent"):
				v["uniquecontext"]=parentid;
				break;
			incaseof("class"):
				if(classdata.attribexists("uniqueclass"))
				{	
					v["uniquecontext"]=findclassid(classdata("uniqueclass").sval());
				}
				else
				{
					v["uniquecontext"]=classid;
				}	
				break;
			defaultcase:
				lasterror = "uniquein-attribute defective";
				errorcode = ERR_DBMANAGER_FAILURE;
				res.clear();
				return &res;
		}
	}

	// check whether the metaid has been used previously, fix the versionnumber accordingly
	if(classdata.attribexists("uniquein"))
	{
		oldv = findObjectDeletedVersion(v["uniquecontext"].ival(), metaid);
		if(oldv==-1)
		{
			// findObjectDeletedVersion has set lasterror for us
			res.clear();
			return &res;
		}
	}	

	// move to protected function to share code with updateObject
	v["version"]=oldv+1;
	v["content"]=serialize(members);
	v["class"]=classid;
	v["uuid"]=strutil::uuid();
	if(ownerid==-1)
	{
	    v["owner"]=userid;
	}
	else
	{
        v["owner"]=ownerid;
	}
	
	if(metaid) v["metaid"]=metaid;
	
	if(!god && parentid!=0 && !haspower(parentid, userid))
	{
        lasterror = "Access denied to parent object";
        errorcode = ERR_DBMANAGER_NOPERM;
        res.clear();
        return &res;
	}
	
    int currentusage;
    int currentquota;
    currentquota = getUserQuota(ofclass, "", &currentusage);
    if (!god && currentquota != -1 && currentusage >= currentquota)
    {
        lasterror.crop();
        lasterror.printf("Over quota (%d/%d)", currentusage, currentquota);
        errorcode = ERR_DBMANAGER_QUOTA;
        return &res;
    }
	
	if(permcheck) // don't do anything, just report that this would have been allowed
	{
		res="allowed";
		return &res;
	}
	
	if(classdata.attribexists("singleton"))
	{
		CORE->log(log::debug, "dbmgr", "comparing [%s] to singleton "
				  "metaid [%s]" %format (metaid, classdata("singleton")));
		
		if(classdata("singleton") != metaid)
		{
			errorcode = ERR_DBMANAGER_INVAL;
			lasterror = "Attempt to set metaid [%s] for singleton requiring "
						"[%s]" %format (metaid, classdata("singleton"));
			res.clear();
			return &res;
		}
	}

	// check requires-conformity
	if(classdata.attribexists("requires"))
	{
		statstring parentclass = classNameFromUUID(parent);
		if(classdata("requires").sval() != parentclass)
		{
			errorcode = ERR_DBMANAGER_INVAL;
			lasterror = "Object (%s) requires a parent of class %s "
						"but found %s (%s)" %format (v["uuid"],
							classdata("requires"), parentclass, parent);
			res.clear();
			return &res;
		}
		if(classdata.attribexists("parentrealm"))
		{
			string pmeta = _findmetaid(parentid);
			char sep;
			
			if(classdata("parentrealm") == "domainsuffix")
				sep = '.';

			if(classdata("parentrealm") == "emailsuffix")
				sep = '@';

			if(!_checkdomainsuffix(metaid, pmeta, sep))
			{
				errorcode = ERR_DBMANAGER_INVAL;
				lasterror = "Your chosen id '%s' does not fit into the "
							"domain '%s'" %format (metaid, pmeta);
				res.clear();
				return &res;
			}
		}
	}
	else
	{
		if(parentid)
		{
			errorcode = ERR_DBMANAGER_INVAL;
			lasterror = "Parent given when none required";
			res.clear();
			return &res;
		}
	}

	res.clear();
	
	if(classdata.attribexists("magicdelimiter"))
	{
		if(classdata("magicdelimiter") != "$")
		{
			errorcode = ERR_DBMANAGER_INVAL;
			lasterror = "Delimiters other than $ are unsupported, FIXME";
			res.clear();
			return &res;
		}
		if(classdata("prototype") != "prototype")
		{
			errorcode = ERR_DBMANAGER_INVAL;
			lasterror = "Prototype-tokens other than 'prototype' are unsupported, FIXME";
			res.clear();
			return &res;
		}
		string proto, protoid;
        proto = classdata("prototype").str();
		protoid.printf("%s%s%s", classdata("magicdelimiter").str(), classdata("prototype").str(), classdata("magicdelimiter").str());
		string query = "SELECT /* createObject prototype */ id FROM objects WHERE ";
		value where;
		where["metaid"]=protoid;
		where["class"]=classid;
		where["uniquecontext"]=classid; // for sanity
		where["wanted"]=1;
		query.strcat(escapeforsql("=", " AND ", where));
		value dbres = dosqlite(query);
		if(dbres["rows"].count() == 1)
		{
			value replacements;
            replacements[proto] = metaid;
			res=copyprototype(dbres["rows"][0]["id"], parentid, ownerid, replacements, true, members);
            if (immediate)
            {
                reportSuccess(res);
            }
            
			return &res;
		}
	}

  exclusivesection (dbhandle)
  {
    value qres, disposeme;
    qres=_dosqlite("BEGIN TRANSACTION /* createObject */");
    if(!qres)
    {
      goto createObject_rollbackandbreak;
    }

  	query="INSERT /* createObject */ INTO objects ";
  	query.strcat(escapeforinsert(v));
  	qres = _dosqlite(query); // FIXME: handle errors
	
  	if(!qres)
  	{
      goto createObject_rollbackandbreak; // dosqlite has set a message for us
  					 // TODO: replace with nicer message
  	}
	
  	if(ofclass == "User")
  	{
  		if(!_setpowermirror(qres["insertid"].ival()))
  		{
        goto createObject_rollbackandbreak;
  		}
  		if(userid == 0)
  		{
  			string query;

  			query.crop();
  			query.printf("REPLACE INTO powermirror (userid,powerid) VALUES(0,%d)", qres["insertid"].ival());
  			value qres = _dosqlite(query);
  			if(!qres)
  			{
          goto createObject_rollbackandbreak;
  			}
  		}
  	}
	
    query.crop();
  	query.printf("UPDATE /* createObject markcolumn */ objects SET wanted=(CASE WHEN version!=%d THEN 0 WHEN version=%d THEN 1 END) WHERE uuid='%s'", 
                 v["version"].ival(), v["version"].ival(), v["uuid"].str());
    qres=_dosqlite(query);
    if(!qres)
		{
      goto createObject_rollbackandbreak;
		}
  	res=v["uuid"].sval();

    if (immediate)
    {
        bool rsres = _reportSuccess(res);
        if (!rsres)
        {
          goto createObject_rollbackandbreak;
        }
    }

    qres = _dosqlite("COMMIT TRANSACTION /* createObject */");
    if (!qres)
    {
      goto createObject_rollbackandbreak;
    }
    
    goto createObject_success;
    
createObject_rollbackandbreak:
    disposeme = _dosqlite("ROLLBACK TRANSACTION /* createObject */");
    res.clear();
    // fallthrough
createObject_success:
    breaksection return &res;
  }
  // return &res;
}

string *DBManager::copyprototype(int fromid, int parentid, int ownerid, value &repl, bool rootobj, const value &members)
{
	returnclass (string) res retain;

	CORE->log(log::debug, "dbmgr", "copyprototype %d -> child of "
			 "%d" %format (fromid, parentid));

	// (a) copy fromid as child of parentid
	// (b) find all children of fromid
	// (c) call ourselves with each childid as fromid, the id from (a) as parentid
	
	// columns:
	// id|uuid|version|metaid|parent|owner|uniquecontext|class|content|reality|wanted|deleted|postponed
	// we select on: id
	// we copy: class, content
	// we rework: metaid
	string query;
	query.printf("SELECT /* copyprototype */ uniquecontext, class, content, metaid FROM objects WHERE id=%d", fromid);
	value dbres = dosqlite(query);
	if(dbres["rows"].count() != 1)
	{
		// this can't happen
		errorcode = ERR_DBMANAGER_FAILURE;
		lasterror = "Object disappeared / on the path of recursion / prototyping failed";
		res.clear();
		return &res;
	}
	value in = dbres["rows"][0];
	
	string iquery = "INSERT /* copyprototype */ INTO objects ";
	value v;
	int classid = in["class"];
	v["class"] = classid;
	CORE->log(log::debug, "dbmgr", "copyprototype: valueparsing "
			  "[%s]" %format (in["metaid"]));
			  
	DEBUG.storeFile("dbmgr", "repl", repl, "copyprototype");
	v["metaid"] = strutil::valueparse(in["metaid"], repl);

	// find class data
	string classquery;
	classquery.printf("SELECT /* copyprototype */ content FROM objects WHERE id=%d", classid);
	value classdbres = dosqlite(classquery);
	if(!classdbres["rows"].count())
	{
		errorcode = ERR_DBMANAGER_NOTFOUND;
		lasterror = "Class not found";
		res.clear();
		return &res;
	}

	value classdata = deserialize(classdbres["rows"][0]["content"]);

	// find uniqueness context
	if(classdata.attribexists("uniquein"))
	{
		caseselector (classdata("uniquein").sval())
		{
			incaseof("parent"):
				v["uniquecontext"]=parentid;
				break;
			incaseof("class"):
				if(classdata.attribexists("uniqueclass"))
				{	
					v["uniquecontext"]=findclassid(classdata("uniqueclass").sval());
				}
				else
				{
					v["uniquecontext"]=classid;
				}	
				break;
			defaultcase:
				errorcode = ERR_DBMANAGER_FAILURE;
				lasterror = "uniquein-attribute defective";
				res.clear();
				return &res;
		}
	}

	int oldv = 0;

	// check whether the metaid has been used previously, fix the versionnumber accordingly
	if(classdata.attribexists("uniquein"))
	{
		oldv = findObjectDeletedVersion(v["uniquecontext"].ival(), v["metaid"]);
		if(oldv==-1)
		{
			// findObjectDeletedVersion has set lasterror for us
			res.clear();
			return &res;
		}
	}
	
	value incontent = deserialize(in["content"]);
	value outcontent;
	foreach(member, incontent)
	{
	    if (members[member.id()].sval().strlen())
            outcontent[member.id()] = members[member.id()];
        else
        	outcontent[member.id()] = strutil::valueparse(member, repl);
	}
	v["version"] = oldv+1;
	v["content"] = serialize(outcontent);
	v["uuid"] = strutil::uuid();
	v["parent"] = parentid;
    v["owner"]=ownerid;
	v["wanted"] = 1;
	v["reality"] = 0;
	iquery.strcat(escapeforinsert(v));
	value idbres = dosqlite(iquery);
	if(!idbres)
	{
		// pass error from dosqlite implicitly
		res.clear();
		return &res;
	}
	res = v["uuid"];
	markcolumn("wanted", res, oldv+1);
	string cquery = "SELECT /* copyprototype */ id FROM objects WHERE ";
	value where;
	where["parent"] = fromid;
	where["wanted"] = 1;
	if(!rootobj)
		where["reality"] = 1;
	cquery.strcat(escapeforsql("=", " AND ", where));
	value cdbres = dosqlite(cquery);
	if(!cdbres)
	{
		// pass error from dosqlite implicitly
		res.clear();
		return &res;
	}
	foreach(row, cdbres["rows"])
	{
		string childuuid = copyprototype(row["id"].ival(), idbres["insertid"].ival(), ownerid, repl, false, emptyvalue);
		if(!childuuid)
		{
			res.clear();
			return &res;
		}
	}
	
	return &res;
}

bool DBManager::_checkdomainsuffix(const string &child, const string &parent, const char sep)
{
	// short-circuit equality approval
	if(child.strcasecmp(parent) == 0)
		return true;

	// parent cannot be longer than child
	if(child.strlen() < parent.strlen())
		return false;

	string dpart = child.right(parent.strlen());

	// check the domain part
	if(dpart.strcasecmp(parent) != 0)
		return false;

	// check that what's left ends in a label separator
	char s = child[child.strlen() - parent.strlen() - 1];
	if(s == sep)
		return true;

	return false;
}

bool DBManager::setpowermirror(int uid)
{
	string query;
	
	query.crop();
	query.printf("REPLACE INTO powermirror (userid,powerid) VALUES(%d,%d)", uid, userid);
	value qres = dosqlite(query);
	if(!qres)
	{
		return false;
	}
	
	query.crop();
	query.printf("REPLACE INTO powermirror (userid,powerid) VALUES(%d,%d)", uid, uid);
	qres = dosqlite(query);
	if(!qres)
	{
		return false;
	}

	query.crop();
	query.printf("REPLACE INTO powermirror (userid,powerid) SELECT %d, powerid FROM powermirror WHERE userid=%d", uid, userid);
	qres = dosqlite(query);
	if(!qres)
	{
		return false;
	}

	return true;
}

bool DBManager::_setpowermirror(int uid)
{
	string query;
	
	query.crop();
	query.printf("REPLACE INTO powermirror (userid,powerid) VALUES(%d,%d)", uid, userid);
	value qres = _dosqlite(query);
	if(!qres)
	{
		return false;
	}
	
	query.crop();
	query.printf("REPLACE INTO powermirror (userid,powerid) VALUES(%d,%d)", uid, uid);
	qres = _dosqlite(query);
	if(!qres)
	{
		return false;
	}

	query.crop();
	query.printf("REPLACE INTO powermirror (userid,powerid) SELECT %d, powerid FROM powermirror WHERE userid=%d", uid, userid);
	qres = _dosqlite(query);
	if(!qres)
	{
		return false;
	}

	return true;
}

bool DBManager::isDeleteable(const statstring &uuid)
{
	// FIXME: implement
	return true;
}

bool DBManager::deleteObject(const statstring &uuid, bool immediate, bool asgod)
{
	CORE->log(log::debug, "dbmgr", "deleteObject(uuid=%s)" %format (uuid));
	
	value empty;
	return updateObject(empty, uuid, immediate, true, asgod);
}

bool DBManager::updateObject(const value &withmembers, const statstring &uuid, bool immediate, bool deleted, bool asgod)
{
	value v;
    int localid;
	string query;
	value members;
	
	members=withmembers;
	
	CORE->log(log::debug, "dbmgr", "updateObject([members], uuid=%s, "
			  "deleted=%d)" %format (uuid, deleted ? 1 : 0));
	
    localid=findlocalid(uuid);
    
    if(!localid)
    {
        // object is already gone. huzzah!
        // TODO: are we sure this is the cleanest thing to do? right now listObjectTree does depend on it.
        return true;
    }

	if(!god && !asgod && !haspower(localid, userid))
	{
		errorcode = ERR_DBMANAGER_NOPERM;
        lasterror = "Access to object denied";
		return false;
	}

    if(deleted)
    {
	// don't allow deleting yourself
	if(localid==userid)
	{
		errorcode = ERR_DBMANAGER_FAILURE;
		lasterror = "Cannot delete yourself";
		return false;
	}
        // check that the object to be deleted is not the owner of anything
        query.crop();
        query.printf("SELECT /* updateObject deleting owner? */ COUNT(id) FROM objects WHERE owner=%d AND wanted=1 AND deleted=0", localid);
        value dbres = dosqlite(query);
        if(dbres["rows"][0][0].ival())
        {
        	errorcode = ERR_DBMANAGER_FAILURE;
            lasterror = "This user still owns other objects, please delete these first";
            return false;
        }
    }

    query="SELECT /* updateObject */ id, content, class, version, metaid, uniquecontext, parent, wanted, reality, owner FROM objects WHERE ";
	value where;
	where["id"]=localid;
	query.strcat(escapeforsql("=", " AND ", where));
	query.strcat(" ORDER BY version DESC LIMIT 1");
    value dbres = dosqlite(query);
	if(!dbres)
	{
		return false;
	}
	if(!dbres["rows"].count())
	{
		errorcode = ERR_DBMANAGER_NOTFOUND;
		lasterror = "No previous version of this object was found";
		return false;
	}
	value fetched = dbres["rows"][0];
	
	if(fetched["metaid"] == "$prototype$" and deleted and !god)
    {
        lasterror = "Prototypes cannot be deleted";
        errorcode = ERR_DBMANAGER_NOPERM;
        return false;
	}
    
	int updatedclassid = findclassid(_classNameFromUUID(fetched["class"]));
	
	if(!checkfieldlist(members, updatedclassid))
	{
		return false;
	}

	CORE->log(log::debug, "dbmgr", "%s %i %i" %format (fetched["version"],
			  fetched["version"], fetched["version"].ival() + 1));
			  
	v["content"]=serialize(members);
	v["class"]=updatedclassid;
	v["uuid"]=uuid;
	
	v["version"]=fetched["version"].ival() + 1;
	v["metaid"]=fetched["metaid"];

	if(fetched["uniquecontext"] != "")
		v["uniquecontext"]=fetched["uniquecontext"];

	v["parent"]=fetched["parent"];
	v["deleted"]=deleted ? 1 : 0;
	v["owner"]=fetched["owner"];
	
	query="INSERT /* updateObject */ INTO objects ";
	query.strcat(escapeforinsert(v));
	value qres = dosqlite(query); // FIXME: handle errors
	
	if(!qres)
	{
		return false; // dosqlite has set a message for us
					 // TODO: replace with nicer message
	}
	

	// if(!copyrelation(fetched["id"].ival(), qres["insertid"].ival()))
	// {
	// 	return false;
	// }
	string updatechildrenquery;

	updatechildrenquery.printf("UPDATE /* updateObject */ objects SET parent=%d WHERE parent=%d", qres["insertid"].ival(), fetched["id"].ival());
	value childdbres = dosqlite(updatechildrenquery);
	if(!childdbres)
	{
		return false;
	}

	markcolumn("wanted", v["uuid"].str(), v["version"].ival());
	
    return true;	
}

int DBManager::findclassid(const statstring &classname)
{
	string query;
	value v;
	string tmp;
	
	// query.printf("SELECT id FROM objects WHERE class=0 AND metaid='%S\'", classname.cval());
	query="SELECT /* findclassid */ id FROM objects WHERE ";
	value where;
	where["class"]=1;
	where["metaid"]=classname;
	query.strcat(escapeforsql("=", " AND ", where));
	query.strcat(" ORDER BY version DESC LIMIT 1");
	v = dosqlite(query);
	DEBUG.storeFile("dbmgr", "dbres", v, "findclassid");
	
	CORE->log(log::debug, "dbmgr","findclassid: %i" %format(v["rows"][0]["id"]));
    return v["rows"][0]["id"].ival(); // FIXME: check if it's there?
}

string *DBManager::_classNameFromUUID(const int classid)
{
	static lock<value> cache_classNameFromUUID;
	string idstring;
	returnclass (string) res retain;
	value v;
	string query;
	
	idstring.printf("%i", classid);
	sharedsection(cache_classNameFromUUID)
	{
		if(cache_classNameFromUUID.exists(idstring))
		{
			res=cache_classNameFromUUID[idstring];
			breaksection return &res;
		}
	}
	// query.printf("SELECT id FROM objects WHERE class=0 AND metaid='%S\'", classname.cval());
	query.printf("SELECT /* _classNameFromUUID */ metaid FROM objects WHERE class=1 AND id=%d", classid);
	v = dosqlite(query);
    res=v["rows"][0]["metaid"].sval();
	exclusivesection(cache_classNameFromUUID)
	{
		cache_classNameFromUUID[idstring]=res;
	}
	return &res; // FIXME: check if it's there?
}

string *DBManager::_findmetaid(const int id)
{
	returnclass (string) res retain;
	value v;
	string query;
	
	query.printf("SELECT /* _findmetaid */ metaid FROM objects WHERE id=%d", id);
	v = dosqlite(query);
    res=v["rows"][0]["metaid"].sval();
	return &res; // FIXME: check if it's there?
}

value *DBManager::dosqlite (const statstring &query)
{
    returnclass (value) res retain;
	
    exclusivesection (dbhandle)
    {
        res = _dosqlite(query);
    }
    
    return &res;
}

value *DBManager::_dosqlite (const statstring &query)
{
	returnclass (value) res retain;
	sqlite3_stmt *qhandle;
	int qres;
	timestamp t1, t2, rt1, rt2;
	int rowcount=0;
	int colcount=0;
	
	t1 = kernel.time.unow();
	CORE->log (log::debug, "dbmgr", "dosqlite: %s" %format (query));
	
	if(sqlite3_prepare(dbhandle.o, query.str(), -1, &qhandle, 0) != SQLITE_OK)
	{
		errorcode = ERR_DBMANAGER_FAILURE;
		lasterror.crop();
		lasterror.printf("sqlite3_prepare(%s) failed: %s", query.str(), sqlite3_errmsg(dbhandle.o));
		return &res; // empty
	}
	
	bool done=false;
	while((qres = sqlite3_step(qhandle)) && !done)
	{
		rt1 = kernel.time.unow();
		switch(qres)
		{
			case SQLITE_BUSY:
				sleep(1);
				continue;
				break;
			case SQLITE_MISUSE:  // fallthrough
			case SQLITE_ERROR:
				// the -useful- errors seem to come from _finalize - let's hope that's a consistent assumption
				CORE->log (log::debug, "dbmgr", "sqlite3_step(%s) failed: "
						   "%s" %format (query, sqlite3_errmsg(dbhandle.o)));
				// fallthrough to DONE
			case SQLITE_DONE:
				done=true;
				break;
			case SQLITE_ROW:
				res["columncount"] = colcount = sqlite3_column_count(qhandle);
				if(colcount == 0)
				{
					res["rowschanged"]=sqlite3_changes(dbhandle.o);
					done=true;
					break;
				}
				value row;
				for(int i=0; i < colcount; i++)
				{
					if(sqlite3_column_type(qhandle, i) != SQLITE_NULL)
						row[sqlite3_column_name(qhandle, i)]=(const char *) sqlite3_column_text(qhandle, i);
					CORE->log (log::debug, "dbmgr", "column text: %s" %format (sqlite3_column_text(qhandle,i)));
				}
				
				res["rows"].newval()=row;
				break;
		}
		rt2 = kernel.time.unow();
		rt1 = rt2 - rt1;
		CORE->log (log::debug, "dbmgr", "row %d: %U usecs", rowcount, rt1.getusec());
		rowcount++;		
	}
	
	if(!done)
	{
		// we should never get here!
		// FIXME: rant and rave harder
		lasterror.crop();
		lasterror.printf("DBManager::dosqlite(%s): pigs fly :(", query.cval());
		errorcode = ERR_DBMANAGER_FAILURE;
		res.clear();
		return &res;
	}
	
	if(sqlite3_finalize(qhandle) != SQLITE_OK)
	{
		CORE->log (log::debug, "dbmgr", "sqlite3_finalize(%s) failed (%s): "
				  "%s" %format (query, sqlite3_errmsg(dbhandle.o)));
				  
		lasterror.crop();
		lasterror.printf("sqlite3_finalize(%s) failed: %s", query.cval(), sqlite3_errmsg(dbhandle.o));
		errorcode = ERR_DBMANAGER_FAILURE;
		res.clear();
		return &res;
	}
	res["insertid"]=sqlite3_last_insert_rowid(dbhandle.o);
	t2 = kernel.time.unow();
	t1 = t2 - t1;
	CORE->log (log::debug, "dbmgr", "dosqlite (%U usecs) returning result of: %s", t1.getusec(), query.cval());	
	return &res;
}

bool DBManager::checkschema (void)
{
	return true; // FIXME: do actual check
}

// HACK: removed const so that sval() for the classid int works
string *DBManager::escapeforsql (const statstring &glue, const statstring &sep, value &vars)
{
	returnclass (string) res retain;
	bool firstiter = true;
	
	value repl;
	repl["'"] = "''";
	foreach(v, vars)
	{
		if(firstiter)
			firstiter = false;
		else
			res.strcat(sep.sval());
		
		// column-name
		res.strcat(v.label().sval());

		// glue
		res.strcat(glue.sval());
		
		// value
		res.strcat("'");
		string escaped = new string(v.sval());
		escaped.replace(repl);
		res.strcat(escaped);
		res.strcat("'");
	}
	
	return &res;
}

string *DBManager::escapeforinsert (const value &vars)
{
	returnclass (string) res retain;
	string res2;
	bool firstiter = true;
	
	res = "(";
	res2 = "VALUES(";
	
	foreach(v, vars)
	{
		if(firstiter)
			firstiter = false;
		else
		{
			res.strcat(',');
			res2.strcat(',');
		}
			
		// column-name
		res.strcat(v.label().sval());

		// value
		// FIXME: handle NULL
		res2.strcat("'");
		string escaped = sqlstringescape(v.sval());
		res2.strcat(escaped);
		res2.strcat("'");
	}
	
	res.strcat(") ");
	res.strcat(res2);
	res.strcat(")");
	
	return &res;
}

string *DBManager::serialize(const value &members)
{
	returnclass (string) res retain;
	
	res=members.toxml(value::compact, schema);
	
	return &res;
}

value *DBManager::deserialize(const string &content)
{
	returnclass (value) res retain;
	
	res.fromxml(content, schema);
	
	return &res;
}

// we're in a raincoat that has four sleeves
// gets us through all kind of stormy weather
bool DBManager::deref(value &members, int localclassid)
{
	string query;
	query.printf("SELECT /* deref */ uuid,metaid,content FROM objects WHERE id=%d", localclassid);
	value classdbres = dosqlite(query);
	
	// FIXME: check count()
	value classdata;
	classdata.fromxml(classdbres["rows"][0]["content"].sval());
		
	// string s = members.toxml();
	// string s2 = classdata.toxml();
	// CORE->log(log::debug, "dbmgr", "(%s) deref: (%d, %s)", DEBUG.uuid().cval(), localclassid, s.cval());
	// CORE->log(log::debug, "dbmgr", "(%s) deref classdef: (%s)", DEBUG.uuid().cval(), s2.cval());
	foreach(field, members)
	{
		if(classdata[field.id()]("type") == "ref")
		{
			CORE->log(log::debug, "dbmgr", "replacing ref %s from object %s, "
					 "field %s" %format (field.id(), field,
					 					 classdata[field.id()]("reflabel")));
					 					 
			value where;
			where["uuid"]=field.cval();
			where["wanted"]=1;
			string q = "SELECT /* deref type==ref */ metaid, uuid, content FROM objects WHERE ";
			q.strcat(escapeforsql("=", " AND ", where));
			value refdbres = dosqlite(q);
			if(!refdbres["rows"].count())
			{
				errorcode = ERR_DBMANAGER_NOTFOUND;
				lasterror = "Referenced object not found";
				members.clear();
				return false;
			}
			DEBUG.storeFile("dbmgr", "dbres", refdbres, "deref");

			value row = refdbres["rows"][0];
			value fields = deserialize(row["content"]);
			
			if(row["metaid"].sval().strlen())
            {
                    fields["id"]=row["metaid"];
                    fields["metaid"]=row["metaid"];
            }
            else
            {
                    fields["id"]=row["uuid"];
            }
			
			// wrong, need to rename field according to spec in module.xml!
			// members[classdata[field.id()]("nick")] = fields[classdata[field.id()]("reflabel")];
			members[classdata[field.id()]("nick")] = fields[classdata[field.id()]("reflabel")];
			
			CORE->log(log::debug, "dbmgr", "deref: debugcontent found: %s", classdata[field.id()]("reflabel").cval());
			CORE->log(log::debug, "dbmgr", "deref: debugcontent found: %s", classdata[field.id()]("nick").cval());
			DEBUG.storeFile("dbmgr", "field", fields, "deref");

			CORE->log(log::debug, "dbmgr", "content found: %s", fields[classdata[field.id()]("reflabel")].cval());
			// FIXME: do the replace -> fetch the right field from the right object etc.
		}
		else if(classdata[field.id()].attribexists("nick"))
		{
			members[classdata[field.id()]("nick")] = field;
		}
		// don't "else" this, the module may want to know about the reference
//		res[field.id()] = field;
	}
	// res=members;

	return true;
}

value *DBManager::filter(const value &members, int localclassid)
{
	returnclass (value) res retain;
	
	string query;
	
	query.printf("SELECT /* filter */ content FROM objects WHERE id=%d", localclassid);
	value classdbres = dosqlite(query);
	
	// FIXME: check count()
	value classdata;
	classdata.fromxml(classdbres["rows"][0]["content"].sval());
		
	// string s = members.toxml();
	// string s2 = classdata.toxml();
	// CORE->log(log::debug, "dbmgr", "(%s) deref: (%d, %s)", DEBUG.uuid().cval(), localclassid, s.cval());
	// CORE->log(log::debug, "dbmgr", "(%s) deref classdef: (%s)", DEBUG.uuid().cval(), s2.cval());
	foreach(field, members)
	{
		if(!classdata[field.id()].attribexists("privateformodule"))
		{
			res[field.id()]=field;
		}
	}
	
	return &res;
}

value *DBManager::hidepasswords(const value &members, int localclassid, bool tagonly)
{
	returnclass (value) res retain;
	
	string query;
	
	CORE->log(log::debug, "dbmgr", "hidepasswords: (%d, %s)", localclassid, tagonly ? "EGWEL" : "EGNIE");
    // DEBUG.storeFile("dbmgr", "members", members, "hidepasswords");
	
	value classdata = getClassData(localclassid);
		
	// string s = members.toxml();
	// string s2 = classdata.toxml();
	// CORE->log(log::debug, "dbmgr", "(%s) deref: (%d, %s)", DEBUG.uuid().cval(), localclassid, s.cval());
	// CORE->log(log::debug, "dbmgr", "(%s) deref classdef: (%s)", DEBUG.uuid().cval(), s2.cval());
	foreach(field, members)
	{
		if(classdata[field.id()]("type") == "password")
		{
			if(tagonly)
			{
				res[field.id()]=field;
				res[field.id()]("type") = "password";
			}
			else
			{
				res[field.id()] = "";
			}
		}
		else
		{
			res[field.id()]=field;
		}
	}
	return &res;
}

bool DBManager::applyFieldWhiteLabel(value &objs, value &whitel)
{
	value blackl, classdata;
	valueindex whitelv;

	DEBUG.storeFile("dbmgr", "objs", objs, "whitel");
	DEBUG.storeFile("dbmgr", "whitel", whitel, "whitel");

	if(!whitel.count())
		return true;
		
	whitelv.indexvalues(whitel);
		
	// this breaks when the list contains objects of different classes
	// the GUI will never do that
	classdata = getClassData(findclassid(objs[0][0]["class"]));
	foreach(member, classdata)
	{
		if(!whitelv.exists(member.id()))
			blackl.newval() = member.id();
	}
	
	DEBUG.storeFile("dbmgr", "blackl", blackl, "whitel");
	
	foreach(object, objs[0])
	{
	    DEBUG.storeFile("dbmgr", "obj-prerm", object, "whitel");
		foreach(b, blackl)
		{
			object.rmval(b);
		}
	    DEBUG.storeFile("dbmgr", "obj-postrm", object, "whitel");
	}

	return true;
}

string *DBManager::sqlstringescape(const string &s)
{
	returnclass (string) res retain;
	
	res=string(s);
	value rep;

    // according to the sqlite docs on their %q, this is the only replace we need
	rep["'"]="''";

	res.replace(rep);
	
	return &res;
}

bool DBManager::login(const statstring &username, const statstring &password)
{	
	md5checksum csum;
	string query;
	query = "SELECT /* login */ id, content FROM objects WHERE ";
	value where;
	where["metaid"]=username;
	where["class"]=findclassid("User");
	where["reality"]=1;
	
    userid = -1;
	query.strcat(escapeforsql("=", " AND ", where));
	value qres = dosqlite(query); // TODO: handle 'not found'
    if(qres["rows"].count())
	{
		value members=deserialize(qres["rows"][0]["content"]);
		if(!deref(members, where["class"]))
		{
			return false;
		}
		string h;
		h = csum.md5pw(password.str(), members["password"].str());
		CORE->log(log::debug, "dbmgr", "checking password: [%s] [%s]", members["password"].str(), h.str());
		if(members["password"].sval() == h)
		{
			int id=qres["rows"][0]["id"].ival();
			if(id)
			{
				userid=id;
				return true;
			}
		}
	}
	
	errorcode = ERR_DBMANAGER_LOGINFAIL;
	lasterror = "Username not found or invalid password";
	return false;
}

void DBManager::getCredentials(value &creds)
{
    creds["userid"]=userid;
}

void DBManager::setCredentials(const value &creds)
{
    userid=creds["userid"];
}

bool DBManager::userLogin(const statstring &username)
{	
	string query;
	query = "SELECT /* userLogin */ id FROM objects WHERE ";
	value where;
	where["metaid"]=username;
	where["class"]=findclassid("User");
	where["reality"]=1;
	
	query.strcat(escapeforsql("=", " AND ", where));
	value qres = dosqlite(query);
    if(!qres["rows"].count())
	{
		errorcode = ERR_DBMANAGER_LOGINFAIL;
		lasterror = "Authentication failed";
		return false;
	}

	int id=qres["rows"][0]["id"].ival();
	
	if(id)
	{
		userid=id;
		return true;
	}
	
	errorcode = ERR_DBMANAGER_LOGINFAIL;
	lasterror = "Lookup error";
	return false;
}

void DBManager::logout(void)
{
	userid=-1;
}

#if 0
bool DBManager::copyrelation(int P, int Q)
{
	value qres;
	string query;
	
	qres=dosqlite("BEGIN TRANSACTION /* copyrelation */");
	if(!qres)
	{
		value disposeme = dosqlite("ROLLBACK TRANSACTION /* copyrelation */");
		return false;
	}

	// P <> P
	query.printf("INSERT /* copyrelation */ INTO objectrelations (objectA,relation,objectB) SELECT %d,relation,%d", Q, Q);
	query.printf(" FROM objectrelations WHERE objectA = %d AND objectB = %d", P, P);
	qres=dosqlite(query);
	if(!qres)
	{
		value disposeme = dosqlite("ROLLBACK TRANSACTION /* copyrelation */");
		return false;
	}

	// P <> x
	query.crop();
	query.printf("INSERT /* copyrelation */ INTO objectrelations (objectA,relation,objectB) SELECT %d,relation,objectB", Q);
	query.printf(" FROM objectrelations WHERE objectA = %d AND objectB!= %d", P, P);
	qres=dosqlite(query);
	if(!qres)
	{
		value disposeme = dosqlite("ROLLBACK TRANSACTION /* copyrelation */");
		return false;
	}

	// x <> P
	query.crop();
	query.printf("INSERT /* copyrelation */ INTO objectrelations (objectA,relation,objectB) SELECT objectA,relation,%d", Q);
	query.printf(" FROM objectrelations WHERE objectA!= %d AND objectB = %d", P, P);
	qres=dosqlite(query);
	
	// drag children along
	query.crop();
	query.printf("UPDATE /* copyrelation */ objects SET parent=%d WHERE parent=%d", Q, P);
	qres=dosqlite(query);
	
	qres=dosqlite("COMMIT TRANSACTION /* copyrelation */");

	return true; // TODO: handle failure
}
#endif

string &DBManager::getLastError(void)
{
	return lasterror;
}

int DBManager::getLastErrorCode(void)
{
	return errorcode;
}

bool DBManager::markcolumn(const statstring &column, const statstring &uuid, int version)
{
	value qres;
	string query;
	
	// P <> P
	query.printf("UPDATE /* markcolumn */ objects SET %s=(CASE WHEN version!=%d THEN 0 WHEN version=%d THEN 1 END) WHERE uuid='%s'", 
	             column.cval(), version, version, uuid.str());
	qres=dosqlite(query);

	return true; // TODO: handle failure
}

int DBManager::findlocalid(const statstring &uuid)
{
	string query="SELECT /* findlocalid */ id FROM objects WHERE ";
	value where;
	where["wanted"]=1;
	where["uuid"]=uuid;
	query.strcat(escapeforsql("=", " AND ", where));
	value dbres = dosqlite(query); // FIXME: handle failure
	if(!dbres["rows"].count())
	{
		lasterror.crop();
		lasterror.printf("Localid for uuid %s not found", uuid.cval());
		return 0;
	}
	return dbres["rows"][0]["id"].ival();
}

// only return false on real errors; 'we already have this class/version'
// is not an error condition!
// FIXME: do something useful with modulename
bool DBManager::registerClass(const value &classdata)
{
	string query;
	value v;
	string tmp;
	
	string debug;
	DEBUG.storeFile("dbmgr", "parm", classdata, "registerClass");

	CORE->log(log::debug, "dbmgr", "registerClass UUID: %S", classdata("uuid").cval());
	if(!classdata.attribexists("uuid"))
	{
		lasterror = "Class needs a UUID";
		return false;
	}
	
	if(!classdata.attribexists("version")) 
	{
		lasterror = "Class needs a version";
		return false;
	}
	
	if(classdata("indexing") == "manual" && !classdata.attribexists("uniquein"))
	{
		lasterror = "Manual indexing needs a uniqueness context";
		return false;
	} 

	CORE->log(log::debug, "dbmgr", "registerClass ID: [%S]", classdata("name").cval());
	
	if(! classdata("name").sval().strlen())
	{
		lasterror = "Class needs a name/id";
		return false;
	}

	query="SELECT /* registerClass */ id,uuid,version FROM objects WHERE ";
	value where;
	where["class"]=1;
	where["metaid"]=classdata ("name");
	query.strcat(escapeforsql("=", " AND ", where));
	query.strcat(" ORDER BY version DESC LIMIT 1");
	value dbres = dosqlite(query);
	DEBUG.storeFile("dbmgr", "dbres", dbres, "registerClass");
	if(dbres["rows"].count()) // zero rows means this class is new
	{	
		value row = dbres["rows"][0];
		
		if(row["uuid"].sval() != classdata("uuid").sval())
		{
			lasterror.crop();
			lasterror.printf("UUID mismatch, old = %S, new = %S, metaid = %S", row["uuid"].cval(), classdata("uuid").cval(), classdata("name").cval());
			return false;
		}

		if(row["version"].ival() > classdata("version").ival())
		{
			lasterror = "Trying to update to an older version";
			return false;
		}
		
		CORE->log(log::debug, "dbmgr", "versions: %d %d", row["version"].ival(), classdata("version").ival());
		if(row["version"].ival() == classdata("version").ival())
		{
			// the version is the same, so everything is cool
			return true;
		}
	}
	
	// apparently this class is new to us!
	
	query="INSERT /* registerClass */ INTO objects ";
	v["uuid"]=classdata("uuid");
	v["version"]=classdata("version");
	v["metaid"]=classdata("name");
	v["uniquecontext"]=v["class"]=1; // predefined constant for Class Class
	v["content"]=serialize(classdata);
	v["wanted"]=v["reality"]=1; // FIXME: handle migrations from old to new versions
	
    int oldid = findclassid(classdata("name"));
    
	query.strcat(escapeforinsert(v));
	value qres = dosqlite(query); // FIXME: handle error
	if(!qres)
		return false;
		
    if (oldid != 1)
    {
        query.crop();
        query.printf("UPDATE /* registerClass */ objects SET class=%d WHERE class=%d", qres["insertid"].ival(), oldid);
        qres = dosqlite(query);
        if(! qres)
        {
        	return false;
        }
    }

	if(!markcolumn("wanted", classdata("uuid").sval(), classdata("version")))
		return false;
		
	if(!markcolumn("reality", classdata("uuid").sval(), classdata("version")))
		return false;
	
	return true;
}

bool DBManager::reportSuccess(const statstring &uuid)
{
  bool res;
  value qres;
    
  exclusivesection (dbhandle)
  {
    qres=_dosqlite("BEGIN TRANSACTION /* reportSuccess */");
  	if(!qres)
  	{
  		value disposeme = _dosqlite("ROLLBACK TRANSACTION /* reportSuccess */");
  		breaksection return false;
  	}
  	
    res = _reportSuccess(uuid);
    
    if(res)
    {
      qres=_dosqlite("COMMIT TRANSACTION /* _reportSuccess */");
    	if(!qres)
    	{
    		value disposeme = _dosqlite("ROLLBACK TRANSACTION /* _reportSuccess */");
    		return false;
    	}
    }
    else
    {
      value disposeme = _dosqlite("ROLLBACK TRANSACTION /* reportSuccess */");
    }
  }
  
  return res;
}
  
bool DBManager::_reportSuccess(const statstring &uuid)
{
  string query;
	value where;
	int oldid, newid;

	query = "SELECT /* _reportSuccess */ id FROM objects WHERE ";
	where.clear();
	where["uuid"]=uuid;
	where["reality"]=1;
	where["deleted"]=0;
	query.strcat(escapeforsql("=", " AND ", where));
	value qres = _dosqlite(query);
	if(! qres)
		return false;

	if(qres["rows"].count())
		oldid=qres["rows"][0]["id"].ival();
	else
		oldid=0;	

	query = "SELECT /* _reportSuccess */ id FROM objects WHERE ";
	where.clear();
	where["uuid"]=uuid;
	where["wanted"]=1;
	query.strcat(escapeforsql("=", " AND ", where));
	query.strcat(" ORDER BY version DESC LIMIT 1");
	qres = _dosqlite(query);
	if(! qres)
		return false;

	newid=qres["rows"][0]["id"].ival();

	query.crop();
	query.printf("UPDATE /* _reportSuccess */ objects SET reality=(CASE WHEN (deleted=0 AND id=%d) THEN 1 ELSE 0 END), wanted=(CASE WHEN (deleted=0 AND id=%d) THEN 1 ELSE 0 END) WHERE ", newid, newid);
	where.clear();
	where["uuid"]=uuid;
	query.strcat(escapeforsql("=", " AND ", where));
	qres = _dosqlite(query);
	if(! qres)
	{
		return false;
	}

	if(oldid)
	{	
		query.crop();
		query.printf("UPDATE /* _reportSuccess */ objects SET parent=%d WHERE parent=%d", newid, oldid);
		qres = _dosqlite(query);
		if(! qres)
		{
			return false;
		}

		query.crop();
		query.printf("UPDATE /* _reportSuccess */ objects SET owner=%d WHERE owner=%d", newid, oldid);
		qres = _dosqlite(query);
		if(! qres)
		{
			return false;
		}	

		query.crop();
		query.printf("UPDATE /* _reportSuccess */ classquota SET userid=%d WHERE userid=%d", newid, oldid);
		qres = _dosqlite(query);
		if(! qres)
		{
			return false;
		}	

		query.crop();
		query.printf("UPDATE /* _reportSuccess */ specialquota SET userid=%d WHERE userid=%d", newid, oldid);
		qres = _dosqlite(query);
		if(! qres)
		{
			return false;
		}	

		query.crop();
		query.printf("UPDATE /* _reportSuccess */ specialquotausage SET userid=%d WHERE userid=%d", newid, oldid);
		qres = _dosqlite(query);
		if(! qres)
		{
			return false;
		}	

		query.crop();
		query.printf("UPDATE /* _reportSuccess */ powermirror SET userid=%d WHERE userid=%d", newid, oldid);
		qres = _dosqlite(query);
		if(! qres)
		{
			return false;
		}

		query.crop();
		query.printf("UPDATE /* _reportSuccess */ powermirror SET powerid=%d WHERE powerid=%d", newid, oldid);
		qres = _dosqlite(query);
		if(! qres)
		{
			return false;
		}

		if(userid == oldid)
			userid = newid;
	}

  return true;
}

// TODO: if version 1 fails, recursively clean all children too, for
// prototype copying
bool DBManager::reportFailure(const statstring &uuid)
{
	value qres, where;
	string query;
	int failedid, gotoid;
	
	exclusivesection (dbhandle)
    {	
    	qres=_dosqlite("BEGIN TRANSACTION /* reportFailure */");
    	if(!qres)
    	{
    		value disposeme = _dosqlite("ROLLBACK TRANSACTION /* reportFailure */");
    		breaksection return false;
    	}
		
    	query = "SELECT /* reportFailure */ id FROM objects WHERE ";
    	where.clear();
    	where["uuid"]=uuid;
    	where["wanted"]=1;
    	query.strcat(escapeforsql("=", " AND ", where));
    	qres=_dosqlite(query);
    	if(!qres)
    	{
    		value disposeme = _dosqlite("ROLLBACK TRANSACTION /* reportFailure */");
    		breaksection return false;
    	}
    	if(!qres["rows"].count())
    	{
    		lasterror = "Object not found in deleteObject";
    		value disposeme = _dosqlite("ROLLBACK TRANSACTION /* reportFailure */");
    		breaksection return false;
    	}

    	failedid=qres["rows"][0]["id"].ival();

    	query = "SELECT /* reportFailure */ id FROM objects WHERE ";
    	where.clear();
    	where["uuid"]=uuid;
    	where["reality"]=1;
    	query.strcat(escapeforsql("=", " AND ", where));
    	qres=_dosqlite(query);
    	if(!qres)
    	{
    		value disposeme = _dosqlite("ROLLBACK TRANSACTION /* reportFailure */");
    		breaksection return false;
    	}
    	if(qres["rows"].count())
    		gotoid=qres["rows"][0]["id"].ival();
    	else
    		gotoid=0;
	
    	query = "DELETE /* reportFailure */ FROM objects WHERE ";
    	where.clear();
    	where["id"]=failedid;
    	query.strcat(escapeforsql("=", " AND ", where));
    	qres = _dosqlite(query);
    	if(!qres)
    	{
    		value disposeme = _dosqlite("ROLLBACK TRANSACTION /* reportFailure */");
    		breaksection return false;
    	}

    	if(gotoid)
    	{
    		query = "UPDATE /* reportFailure */ objects SET wanted=1 WHERE ";
    		where.clear();
    		where["id"]=gotoid;
    		query.strcat(escapeforsql("=", " AND ", where));
    		qres = _dosqlite(query);
    		if(!qres)
    		{
    			value disposeme = _dosqlite("ROLLBACK TRANSACTION /* reportFailure */");
    			breaksection return false;
    		}
    	}	
    	query.crop();
    	query.printf("UPDATE /* reportFailure */ objects SET parent=%d WHERE parent=%d", gotoid, failedid);
    	qres = _dosqlite(query);
    	if(!qres)
    	{
    		value disposeme = _dosqlite("ROLLBACK TRANSACTION /* reportFailure */");
    		breaksection return false;
    	}

    	qres=_dosqlite("COMMIT TRANSACTION /* reportFailure */");
    	if(!qres)
    	{
    		value disposeme = _dosqlite("ROLLBACK TRANSACTION /* reportFailure */");
    		breaksection return false;
    	}

    	breaksection return true;
    }
}

// we iterate upwards from our logged-in user to find all
// applying limits. a smaller limit overrides a bigger one,
// any limit overrides infinity, infinity never overrides any limit
int DBManager::getUserQuota(const statstring &ofclass, const statstring &useruuid, int *usage)
{
    int lookupid, localid, quota, realuserid;
    string q;
	
	if(useruuid)
	{
		lookupid = findlocalid(useruuid);
		if(!lookupid)
		{
			lasterror = "User not found";
			return -2;
		}
	}
	else
	{
		lookupid=userid;
	}
	
	if(lookupid != userid && !haspower(lookupid, userid))
	{
        lasterror = "Permission denied";
        return -2;
	}
    realuserid=lookupid;
	localid=findclassid(ofclass);
	
	quota=-1;
	
	while(1)
	{
		int thisquota;
        q.crop();
		q.printf("SELECT /* getUserQuota */ quota FROM classquota WHERE userid=%d AND classid=%d", lookupid, localid);
		value dbres = dosqlite(q);
		if(!dbres["rows"].count())
		{
			thisquota = -1; // no information -> don't limit!
		}
		else
		{
			thisquota=dbres["rows"][0]["quota"].ival();
		}
		// smaller limit overrides bigger limit
		// infinity never overrides anything
		if((thisquota < quota && thisquota != -1)
		|| (quota == -1 && thisquota != -1))
			quota=thisquota;
			
		q.crop();
		q.printf("SELECT /* getUserQuota */ owner FROM objects WHERE id=%d AND wanted=1", lookupid);
		dbres = dosqlite(q);
		if(!dbres["rows"].count() || dbres["rows"][0]["owner"].ival() == 0)
		{
			break; // we're done looking stuff up
		}
		lookupid=dbres["rows"][0]["owner"].ival();
	}
	
	if(usage)
    {
    	// calculate usage
        q.crop();
        q.printf("SELECT /* getUserQuota */ COUNT(DISTINCT(o.id)) FROM objects o, powermirror p WHERE class=%d AND wanted=1 AND o.owner=p.userid AND p.powerid=%d", localid, realuserid);
        value dbres = dosqlite(q);
        *usage = dbres["rows"][0][0].ival();

    	CORE->log(log::debug, "dbmgr", "getUserQuota: usage: %d out of %d allowed", *usage, quota);
	}

	return quota;
}

bool DBManager::setUserQuota(const statstring &ofclass, int count, const statstring &useruuid)
{
	int uid,classid;
	string q;
	
	uid=findlocalid(useruuid);
	if(!uid)
	{
        lasterror = "User not found";
        return false;
	}
	if(!haspower(uid, userid) || uid==userid)
	{
        lasterror = "Permission denied";
        return false;
	}
	classid=findclassid(ofclass);
	q.printf("REPLACE /* setUserQuota */ INTO classquota (userid,classid,quota) VALUES(%d,%d,%d)", uid, classid, count);
	value dbres = dosqlite(q);
	
	return (bool) dbres;
}

bool DBManager::haspower(int oid, int uid)
{
	int owner;

	if(oid == uid)  // TODO: remove this and add it in the places that want it
		return true;

	string q;
	q.printf("SELECT /* haspower */ owner FROM objects WHERE id=%d", oid);
	value dbres=dosqlite(q);
	if(!dbres["rows"].count())
		return false;
		
	string tmp=dbres.toxml();
	CORE->log(log::debug, "dbmgr", "haspower: %s" %format (tmp));
	
	owner=dbres["rows"][0]["owner"];
	if(owner == uid)
	    return true;

	q.crop();
	q.printf("SELECT /* haspower */ userid, powerid FROM powermirror WHERE userid=%d AND powerid=%d", owner, uid);
    dbres=dosqlite(q);
	if(!dbres["rows"].count())
		return false;

	return true;
}

void DBManager::enableGodMode(void)
{
	int classid;
	string q;
	
	userid=0;
	god=true;
}

value *DBManager::getClassData(int classid)
{
	static lock<value> cache_getClassData;
	string idstring;
	returnclass (value) res retain;
	string query;

	CORE->log(log::debug, "dbmgr", "getClassData %i" %format (classid));

	idstring.printf("%i", classid);
	sharedsection(cache_getClassData)
	{
		if(cache_getClassData.exists(idstring))
		{
			res=cache_getClassData[idstring];
			DEBUG.storeFile("dbmgr", "result-fromcache", res, "getClassData");
			breaksection return &res;
		}
	}

	query.printf("SELECT /* getClassData */ content FROM objects WHERE id=%d", classid);
	value classdbres = dosqlite(query);
	value classdata;
	classdata.fromxml(classdbres["rows"][0]["content"].sval());
	res = classdata;

	exclusivesection(cache_getClassData)
	{
		cache_getClassData[idstring]=res;
	}
	DEBUG.storeFile("dbmgr", "result-fromdb", res, "getClassData");
	return &res;
}

bool DBManager::classhasattrib(int classid, const statstring attrib)
{	
	value classdata;
	
	classdata=getClassData(classid);
	return classdata.attribexists(attrib);
}

string *DBManager::classgetattrib(int classid, const statstring attrib)
{	
	value classdata;
	returnclass (string) res retain;
	
	classdata=getClassData(classid);
	if(classdata.attribexists(attrib))
		res = classdata(attrib);
	return &res;
}

bool DBManager::chown(const statstring &objectuuid, const statstring &useruuid)
{
	// TODO: update powermirror

	int objid, nuserid;
	
	if(!(objid = findlocalid(objectuuid)) || !(nuserid = findlocalid(useruuid)))
	{
		return false;
	}
	
	if(!god && !(haspower(objid, userid) && haspower(nuserid, userid)))
	{
        lasterror = "Permission denied";
        return false;
	}
	
	string query;
	query.printf("SELECT /* chown */ parent FROM objects WHERE id=%d", objid);
	value pdbres = dosqlite(query);
	if(!pdbres)
		return false;
	
	if(pdbres["rows"].count() != 1)
	{
		lasterror = "Object disappeared";
		return false;
	}
	
	if(pdbres["rows"][0]["parent"].ival() != 0)
	{
		lasterror = "Object has parent, please chown higher up";
		return false;
	}

    query.crop();
    query.printf("SELECT /* chown */ COUNT(id) FROM objects WHERE parent=%d AND wanted=1", objid);
    value dbres = dosqlite(query);
    if(dbres["rows"][0][0].ival())
    {
        lasterror = "Object is parent of other objects, please chown before creating objects under another";
        return false;
    }
    
	query.crop();
	query.printf("UPDATE /* chown */ objects SET owner=%d WHERE id=%d", nuserid, objid);
	value udbres = dosqlite(query);
	if(!udbres)
		return false;

    return true;
}

bool DBManager::checkfieldlist(value &members, int classid)
{
	value classdata;
	string fid;
	
	classdata=getClassData(classid);
	
	foreach(field, members)
	{
		fid=field.id();
		if(classdata.exists(field.id()))
		{
			if(classdata[field.id()]("type") == "ref")
			{
				// lookup by UUID
				if(!findlocalid(field))
				{
					string uuid;
					uuid = findObject("", classdata[field.id()]("refclass"), "", field);
					if(!uuid.strlen())
					{
						lasterror="Reference %s (%s) not found" %format (field, field.id());
						return false;
					}
					field = uuid;
				}
			}
		}
		else
		{
			if(fid == "id" || fid == "uuid" || fid == "metaid" || fid == "parentid" || fid == "ownerid")
			{
				CORE->log (log::warning, "dbmgr", "Member <%S> not found "
						   "in class definition", fid.str());
			}
			else if(fid == "version")
			{
				// that's ok
			}
			else
			{
				lasterror = "Member '%S' not found in class definition" %format (field.id());
				return false;
			}
		}
		
	}
	members.rmval("id");
	members.rmval("uuid");
	members.rmval("metaid");
	// TODO: remove version from fetchObject/findObject again; change createObject
	// to find the version through a separate method, then this here can go.
	members.rmval("version");
	
	return true;
}

/// Get a non-object quota number for a specific tag/user.					
int DBManager::getSpecialQuota (const statstring &tag, const statstring &useruuid)
{
    int uid;
	string q;
    value v;

	uid=findlocalid(useruuid);
	if(!haspower(uid, userid))
	{
        lasterror = "permission denied";
        return -2;
	}
	
    q=("SELECT /* getSpecialQuota */ quota FROM specialquota WHERE ");
    v["tag"]=tag;
    v["userid"]=uid;
    q.strcat(escapeforsql("=", " AND ", v));
    value dbres = dosqlite(q);

    return dbres["rows"][0]["quota"];
}

/// Get recursively accumulated usage for a specific tag/user.
int DBManager::getSpecialQuotaUsage (const statstring &tag, const statstring &useruuid)
{
    // probably easily done with a left join to powermirror, a WHERE on class, and a SUM
    int lookupid;
    string q;
	
	lookupid = findlocalid(useruuid);
	if(!lookupid)
	{
		lasterror = "user not found";
		return -2;
	}
	
	if(lookupid != userid && !haspower(lookupid, userid))
	{
        lasterror = "permission denied";
        return -2;
	}

	// calculate usage
	
    q="SELECT /* getUserQuota */ SUM(squ.usage) FROM specialquotausage squ LEFT JOIN powermirror p ON squ.userid=p.userid WHERE ";
    value where;
    where["p.powerid"]=lookupid;
    where["squ.tag"]=tag;
	q.strcat(escapeforsql("=", " AND ", where));
    value dbres = dosqlite(q);

    return dbres["rows"][0][0].ival();
}

/// Get the quota warning level for a specific tag/user.
int DBManager::getSpecialQuotaWarning (const statstring &tag, const statstring &useruuid)
{
    int uid;
	string q;
    value v;

	uid=findlocalid(useruuid);
	if(!haspower(uid, userid))
	{
        lasterror = "permission denied";
        return -2;
	}
    q=("SELECT /* getSpecialQuotaWarning */ warning FROM specialquota WHERE ");
    v["tag"]=tag;
    v["userid"]=uid;
    q.strcat(escapeforsql("=", " AND ", v));
    value dbres = dosqlite(q);

    return dbres["rows"][0]["warning"];
}

/// Set a non-object quota number for a specific tag/user.
bool DBManager::setSpecialQuota (const statstring &tag, const statstring &useruuid, int quota, int warning, value &phys)
{
    int uid;
	string q;
    value v;

	uid=findlocalid(useruuid);
	if(!haspower(uid, userid))
	{
        lasterror = "permission denied";
        return false;
	}
    int lookupid=uid;

	if (warning > quota)
	{
		lasterror = "warninglevel can't be higher than quotalevel";
		return false;
	}
    
    string key=_findmetaid(lookupid);
    phys[key]=quota;
    
	while(1)
	{
	    int subassigned, ownquota;
        
        q="SELECT /* setSpecialQuota */ owner FROM objects WHERE ";
        value where;
        where["id"]=lookupid;
    	q.strcat(escapeforsql("=", " AND ", where));
        value dbres = dosqlite(q);
        if(!dbres["rows"].count())
        {
            break;
        }
        
        lookupid = dbres["rows"][0]["owner"].ival();
    
        q="SELECT s.q FROM (SELECT /* setSpecialQuota */ sq.quota q FROM specialquota sq JOIN powermirror p ON sq.userid=p.userid WHERE ";
        where.clear();
        where["p.powerid"]=lookupid;
        where["sq.tag"]=tag;
    	q.strcat(escapeforsql("=", " AND ", where));
		q.printf(" AND sq.userid!=%d AND sq.userid!=%d GROUP BY sq.id) s", lookupid, uid);
        dbres = dosqlite(q);

        subassigned=dbres["rows"][0][0].ival();
        
        q="SELECT /* setSpecialQuota */ sq.quota FROM specialquota sq WHERE ";
        where.clear();
        where["sq.userid"]=lookupid;
        where["sq.tag"]=tag;
    	q.strcat(escapeforsql("=", " AND ", where));
        dbres = dosqlite(q);

        if(dbres["rows"].count())
        {
            ownquota=dbres["rows"][0][0].ival();
            if(subassigned+quota > ownquota)
            {
                lasterror="assigned quota too large for allowance";
                return false;
            }

            string key=_findmetaid(lookupid);
            phys[key]=ownquota-subassigned-quota;
	    }
	}
	
    q=("REPLACE /* setSpecialQuota */ INTO specialquota");
    v["tag"]=tag;
    v["userid"]=uid;
    v["quota"]=quota;
    v["warning"]=warning;
    q.strcat(escapeforinsert(v));
    
	value dbres = dosqlite(q);
	DEBUG.storeFile("dbmgr", "phys", phys, "setSpecialQuota");
	return (bool) dbres;
}

/// Set non-recursive(!) usage for a specific tag/user.
bool DBManager::setSpecialQuotaUsage (const statstring &tag, const statstring &useruuid, int amt)
{
    int uid;
	string q;
    value v;
    
    if (!god)
    {
        lasterror = "usage is set automatically";
        return false;
    }

	uid=findlocalid(useruuid);
    q=("REPLACE /* setSpecialQuotaUsage */ INTO specialquotausage");
    v["tag"]=tag;
    v["userid"]=uid;
    v["usage"]=amt;
    q.strcat(escapeforinsert(v));
    
	value dbres = dosqlite(q);

	return (bool) dbres;
}

value *DBManager::listSpecialQuota()
{
 	returnclass (value) res retain;
   
    res["Domain:Vhost/traffic"] = "Web traffic";
    res["User/diskspace"] = "Disk usage";
    
    return &res;
}
