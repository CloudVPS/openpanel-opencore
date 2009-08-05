#include "opencore.h"
#include "internalclass.h"
#include "version.h"
#include "debug.h"
#include <grace/version.h>

// ==========================================================================
// CONSTRUCTOR InternalClass
// ==========================================================================
InternalClass::InternalClass (void)
{
}

// ==========================================================================
// DESTRUCTOR InternalClass
// ==========================================================================
InternalClass::~InternalClass (void)
{
}

// ==========================================================================
// METHOD InternalClass::listObjects
// ==========================================================================
value *InternalClass::listObjects (coresession *s, const statstring &parentid)
{
	CORE->log (log::error, "iclass", "Listobjects on base class");
	setError ("List not implemented");
	return NULL;
}

// ==========================================================================
// METHOD InternalClass::getObject
// ==========================================================================
value *InternalClass::getObject (coresession *s, const statstring &parentid,
								 const statstring &id)
{
	returnclass (value) res retain;
	value tmp = listObjects (s, parentid);
	res[tmp[0].id()] = tmp[0][id];
	return &res;
}

// ==========================================================================
// METHOD InternalClass::createObject
// ==========================================================================
string *InternalClass::createObject (coresession *s,
									 const statstring &parentid,
									 const value &withparam,
									 const statstring &withid)
{
	setError ("Create not implemented");
	return NULL;
}

// ==========================================================================
// METHOD InternalClass::deleteObject
// ==========================================================================
bool InternalClass::deleteObject (coresession *s,
								  const statstring &parentid,
								  const statstring &witid)
{
	setError ("Delete not implemented");
	return false;
}

// ==========================================================================
// METHOD InternalClass::updateObject
// ==========================================================================
bool InternalClass::updateObject (coresession *s,
								  const statstring &parentid,
								  const statstring &withid,
								  const value &withparam)
{
	setError ("Update not implemented");
	return false;
}

// ==========================================================================
// METHOD InternalClass::getUUID
// ==========================================================================
const string &InternalClass::getUUID (const statstring &parentid,
									  const statstring &metaid)
{
	statstring pid = parentid;
	if (! pid) pid = "@";
	
	if (! uuidmap[pid].exists (metaid))
	{
		statstring uuid = strutil::uuid ();
		metamap[uuid] = $(parentid)->$(metaid);
		return uuidmap[pid][metaid] = uuid;
	}
	
	return uuidmap[pid][metaid];
}

// ==========================================================================
// METHOD InternalClass::resolveUUID
// ==========================================================================
const value &InternalClass::resolveUUID (const statstring &uuid)
{
	return metamap[uuid];
}

// ==========================================================================
// METHOD InternalClass::metaid
// ==========================================================================
const string &InternalClass::getMetaID (const statstring &uuid)
{
    DEBUG.storeFile("InternalClass",uuid, metamap, "getMetaID");
	return metamap[uuid][1];
}

// ==========================================================================
// METHOD InternalClass::getParentID
// ==========================================================================
const string &InternalClass::getParentID (const statstring &uuid)
{
	DEBUG.storeFile("InternalClass", uuid, metamap, "getMetaID");
	return metamap[uuid][0];
}

// ==========================================================================
// CONSTRUCTOR QuotaClass
// ==========================================================================
QuotaClass::QuotaClass (void)
{
}

// ==========================================================================
// DESTRUCTOR QuotaClass
// ==========================================================================
QuotaClass::~QuotaClass (void)
{
}

// ==========================================================================
// METHOD QuotaClass::listObjects
// ==========================================================================
value *QuotaClass::listObjects (coresession *s,
								const statstring &parentid)
{
	returnclass (value) res retain;
	
	value &qres = res["OpenCORE:Quota"];
	qres("type") = "object";
	value cl = s->mdb.listclasses ();
	statstring cuuid;
	
	foreach (c, cl)
	{
		cuuid = getUUID (parentid, c.id());
		int usage = 0;
		int quota = s->db.getUserQuota (c.id(), parentid, &usage);
		coreclass &cl = s->mdb.getclass (c.id());
		
		if (cl.dynamic) continue;
		if (! cl.capabilities.attribexists ("create")) continue;
		
		qres[c.id()] =
			$("class", "OpenCORE:Quota") ->
			$("uuid", cuuid) ->
			$("id", c.id()) ->
			$("description", cl.description) ->
			$("metaid", c.id()) ->
			$("units", "Objects") ->
			$("usage", usage) ->
			$("quota", quota);
	}
	
	value slist = s->db.listSpecialQuota();
	foreach (q, slist)
	{
		statstring qid = q.id();
		statstring qwarningid = "%s:warning" %format (q.id());
		string qdesc = "%s (warning level)" %format (q);
		
		cuuid = getUUID (parentid, qid);
		qres << $(qid,
					$("class", "OpenCORE:Quota") ->
					$("uuid", cuuid) ->
					$("id", qid) ->
					$("metaid", qid) ->
					$("description", q.sval()) ->
					$("units", "MBytes") ->
					$("usage", s->db.getSpecialQuotaUsage (qid, parentid)) ->
					$("quota", s->db.getSpecialQuota (qid, parentid)));
		
		cuuid = getUUID (parentid, qwarningid);
		qres << $(qwarningid,
					$("class", "OpenCORE:Quota") ->
					$("uuid", cuuid) ->
					$("id", qwarningid) ->
					$("metaid", qwarningid) ->
					$("description", qdesc) ->
					$("units", "Mbytes") ->
					$("usage", s->db.getSpecialQuotaUsage (qid, parentid)) ->
					$("quota", s->db.getSpecialQuotaWarning (qid, parentid)));
	}
	
	return &res;
}

// ==========================================================================
// METHOD QuotaClass::getObject
// ==========================================================================
value *QuotaClass::getObject (coresession *s,
							  const statstring &parentid,
							  const statstring &withid)
{
	returnclass (value) res retain;

	statstring mid = withid;
	statstring cuuid;
	
	if (mid.sval().strchr ('-') == 8)
	{
		mid = getMetaID (mid);
		cuuid = withid;
	}
	else
	{
		cuuid = getUUID (parentid, mid);
	}
	
	if (mid == "bandwidth")
	{
		res = $("OpenCORE:Quota",
					$("class", "OpenCORE:Quota") ->
					$("uuid", cuuid) ->
					$("id", "bandwidth") ->
					$("metaid", "bandwidth") ->
					$("description", "Bandwidth") ->
					$("units", "MB") ->
					$("usage", 250) ->
					$("quota", 1000));
		return &res;
	}
	else if (mid == "diskspace")
	{
		res = $("OpenCORE:Quota",
					$("class", "OpenCORE:Quota") ->
					$("uuid", cuuid) ->
					$("id", "diskspace") ->
					$("metaid", "diskspace") ->
					$("description", "Disk space") ->
					$("units", "MB") ->
					$("usage", 50) ->
					$("quota", 14));
		return &res;
	}

	int usage = 0;
	int quota = s->db.getUserQuota (withid, parentid, &usage);
	
	res = $("OpenCORE:Quota",
				$("class", "OpenCORE:Quota") ->
				$("uuid", cuuid) ->
				$("id", mid) ->
				$("metaid", mid) ->
				$("description", withid) ->
				$("units", "Objects") ->
				$("usage", usage) ->
				$("quota", quota));
	
	return &res;
}

// ==========================================================================
// METHOD QuotaClass::updateObject
// ==========================================================================
bool QuotaClass::updateObject (coresession *s,
							   const statstring &_parentid,
							   const statstring &withid,
							   const value &withparam)
{
	statstring parentid = _parentid;
	int count = withparam["quota"];
    statstring mid;
	if (withid.sval().strchr ('-') == 8)
	{
		mid = getMetaID (withid);
		parentid = getParentID (withid);
		
	}
	else
	{
        mid = withid;
    }

	statstring parentmeta = s->uuidtometa (parentid);
	
	if (parentmeta == s->meta["user"])
	{
		setError ("Cannot change own quota");
		return false;
	}
    
    CORE->log (log::debug, "QuotaClass", "Updateobject id=<%s> metaid=<%s> "
    		   "parentid=<%s> param=%J" %format (withid, mid, parentid, withparam));

	if (mid)
	{
		// Non-object quotas carry a name that should not
		// clash with a classname. This is not the cleanest
		// way to distinguish them, but probably the quickest ;)
		if (s->mdb.classexists (mid))
		{
			if (s->setUserQuota (mid, count, parentid)) return true;
			setError (s->error()["message"]);
			return false;
		}
		
		bool iswarning = mid.sval().strstr (":warning") > 0;
        bool isusage = mid.sval().strstr (":usage") > 0;
        if (isusage)
        {
            // TODO: strip ':usage' from mid
            if(s->db.setSpecialQuotaUsage(mid, parentid, withparam["usage"])) return true;
            setError (s->db.getLastError());
            return false;
        }
        value phys;
		if (iswarning)
		{
            // a warning change should not cause physical quota changes
            // TODO: strip ':warning' from mid
			string tag = mid.sval().copyuntil (":warning");
            int curquota=s->db.getSpecialQuota(tag, parentid);
            if(s->db.setSpecialQuota(tag, parentid, curquota, count, phys)) return true;
            setError (s->db.getLastError());
            return false;
		}
		else
		{
		    int curwarn=s->db.getSpecialQuotaWarning(mid, parentid);
            if (!(s->db.setSpecialQuota(mid, parentid, count, curwarn, phys)))
            {            
                setError (s->db.getLastError());
                return false;
            }                

            // now communicate phys towards moduledb
            string moderr;
            if(s->mdb.setspecialphysicalquota(mid, phys, err) == status_ok) return true;
            
            setError (moderr);
            return false;
		}
		return false; // can't happen
	}
	if (s->setUserQuota (withid, count, parentid)) return true;
	setError (s->error()["message"]);
	return false;
}

// ==========================================================================
// CONSTRUCTOR SessionListClass
// ==========================================================================
SessionListClass::SessionListClass (void)
{
}

// ==========================================================================
// DESTRUCTOR SessionListClass
// ==========================================================================
SessionListClass::~SessionListClass (void)
{
}

// ==========================================================================
// METHOD sesionlistclass::listObjects
// ==========================================================================
value *SessionListClass::listObjects (coresession *s, const statstring &pid)
{
	//if (s->meta["user"] != "openadmin") return NULL;
	
	returnclass (value) res retain;
	value &qres = res["OpenCORE:ActiveSession"];
	value m = CORE->sdb->list ();
	
	if (s->meta["user"] != "openadmin") return &res;
	
	foreach (row, m)
	{
		qres[row.id()] = $("id", row.id()) ->
						 $("metaid", row.id()) ->
						 $("uuid", row.id()) ->
						 $("class", "OpenCORE:ActiveSession") ->
						 $merge (row);
	}
	
	return &res;
}

// ==========================================================================
// CONSTRUCTOR ErrorLogClass
// ==========================================================================
ErrorLogClass::ErrorLogClass (void)
{
}

// ==========================================================================
// DESTRUCTOR ErrorLogClass
// ==========================================================================
ErrorLogClass::~ErrorLogClass (void)
{
}

// ==========================================================================
// METHOD ErrorLogClass::listObjects
// ==========================================================================
value *ErrorLogClass::listObjects (coresession *s, const statstring &pid)
{
	returnclass (value) res retain;
	value &qres = res["OpenCORE:ErrorLog"];
	value m = CORE->geterrors ();
	statstring uu;
	
	foreach (row, m)
	{
		uu = strutil::uuid ();
		qres[uu] = $("id", uu) ->
				   $("metaid", uu) ->
				   $("uuid", uu) ->
				   $("class", "OpenCORE:ErrorLog") ->
				   $merge (row);
	}
	
	return &res;
}

// ==========================================================================
// CONSTRUCTOR CoreSystemClass
// ==========================================================================
CoreSystemClass::CoreSystemClass (void)
{
	sysuuid = strutil::uuid ();
}

// ==========================================================================
// DESTRUCTOR CoreSystemClass
// ==========================================================================
CoreSystemClass::~CoreSystemClass (void)
{
}

// ==========================================================================
// METHOD CoreSystemClass::listObjects
// ==========================================================================
value *CoreSystemClass::listObjects (coresession *s, const statstring &pid)
{
	return $("OpenCORE:System",
				$("system",
					$("id", "system") ->
					$("uuid", sysuuid) ->
					$("metaid", "system")->
					$("class", "OpenCORE:System") ->
					$("version", version::release) ->
					$("buildhost", version::hostname) ->
					$("builddate", version::date) ->
					$("grace", grace::VERSION)
				 )
			);
}

// ==========================================================================
// CONSTRUCTOR ClassListClass
// ==========================================================================
ClassListClass::ClassListClass (void)
{
}

// ==========================================================================
// DESTRUCTOR ClassListClass
// ==========================================================================
ClassListClass::~ClassListClass (void)
{
}

// ==========================================================================
// METHOD ClassListClass::listObjects
// ==========================================================================
value *ClassListClass::listObjects (coresession *s, const statstring &pid)
{
	returnclass (value) res retain;
	
	res["OpenCORE:ClassList"] = s->mdb.listclasses ();
	foreach (n, res["OpenCORE:ClassList"])
	{
		n["id"] = n["metaid"] = n.id();
		n["class"] = "OpenCORE:ClassList";
		
		coremodule *m = s->mdb.getmoduleforclass (n["metaid"].sval());
		if (m)
		{
			n["apitype"] = m->apitype;
			n["license"] = m->license;
			n["author"] = m->author;
			n["url"] = m->url;
		}
	}
	
	return &res;
}
