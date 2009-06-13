#include "opencore.h"
#include "internalclass.h"
#include "version.h"
#include "debug.h"
#include <grace/version.h>

// ==========================================================================
// CONSTRUCTOR internalclass
// ==========================================================================
internalclass::internalclass (void)
{
}

// ==========================================================================
// DESTRUCTOR internalclass
// ==========================================================================
internalclass::~internalclass (void)
{
}

// ==========================================================================
// METHOD internalclass::listobjects
// ==========================================================================
value *internalclass::listobjects (coresession *s, const statstring &parentid)
{
	CORE->log (log::error, "iclass", "Listobjects on base class");
	seterror ("List not implemented");
	return NULL;
}

// ==========================================================================
// METHOD internalclass::getobject
// ==========================================================================
value *internalclass::getobject (coresession *s, const statstring &parentid,
								 const statstring &id)
{
	returnclass (value) res retain;
	value tmp = listobjects (s, parentid);
	res[tmp[0].id()] = tmp[0][id];
	return &res;
}

// ==========================================================================
// METHOD internalclass::createobject
// ==========================================================================
string *internalclass::createobject (coresession *s,
									 const statstring &parentid,
									 const value &withparam,
									 const statstring &withid)
{
	seterror ("Create not implemented");
	return NULL;
}

// ==========================================================================
// METHOD internalclass::deleteobject
// ==========================================================================
bool internalclass::deleteobject (coresession *s,
								  const statstring &parentid,
								  const statstring &witid)
{
	seterror ("Delete not implemented");
	return false;
}

// ==========================================================================
// METHOD internalclass::updateobject
// ==========================================================================
bool internalclass::updateobject (coresession *s,
								  const statstring &parentid,
								  const statstring &withid,
								  const value &withparam)
{
	seterror ("Update not implemented");
	return false;
}

// ==========================================================================
// METHOD internalclass::getuuid
// ==========================================================================
const string &internalclass::getuuid (const statstring &parentid,
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
// METHOD internalclass::resolveuuid
// ==========================================================================
const value &internalclass::resolveuuid (const statstring &uuid)
{
	return metamap[uuid];
}

// ==========================================================================
// METHOD internalclass::metaid
// ==========================================================================
const string &internalclass::getmetaid (const statstring &uuid)
{
    DEBUG.storefile("internalclass",uuid, metamap, "getmetaid");
	return metamap[uuid][1];
}

// ==========================================================================
// METHOD internalclass::getparentid
// ==========================================================================
const string &internalclass::getparentid (const statstring &uuid)
{
	DEBUG.storefile("internalclass", uuid, metamap, "getmetaid");
	return metamap[uuid][0];
}

// ==========================================================================
// CONSTRUCTOR quotaclass
// ==========================================================================
quotaclass::quotaclass (void)
{
}

// ==========================================================================
// DESTRUCTOR quotaclass
// ==========================================================================
quotaclass::~quotaclass (void)
{
}

// ==========================================================================
// METHOD quotaclass::listobjects
// ==========================================================================
value *quotaclass::listobjects (coresession *s,
								const statstring &parentid)
{
	returnclass (value) res retain;
	
	value &qres = res["OpenCORE:Quota"];
	qres("type") = "object";
	value cl = s->mdb.listclasses ();
	statstring cuuid;
	
	foreach (c, cl)
	{
		cuuid = getuuid (parentid, c.id());
		int usage = 0;
		int quota = s->db.getuserquota (c.id(), parentid, &usage);
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
	
	value slist = s->db.listspecialquota();
	foreach (q, slist)
	{
		statstring qid = q.id();
		statstring qwarningid = "%s:warning" %format (q.id());
		string qdesc = "%s (warning level)" %format (q);
		
		cuuid = getuuid (parentid, qid);
		qres << $(qid,
					$("class", "OpenCORE:Quota") ->
					$("uuid", cuuid) ->
					$("id", qid) ->
					$("metaid", qid) ->
					$("description", q.sval()) ->
					$("units", "MBytes") ->
					$("usage", s->db.getspecialquotausage (qid, parentid)) ->
					$("quota", s->db.getspecialquota (qid, parentid)));
		
		cuuid = getuuid (parentid, qwarningid);
		qres << $(qwarningid,
					$("class", "OpenCORE:Quota") ->
					$("uuid", cuuid) ->
					$("id", qwarningid) ->
					$("metaid", qwarningid) ->
					$("description", qdesc) ->
					$("units", "Mbytes") ->
					$("usage", s->db.getspecialquotausage (qid, parentid)) ->
					$("quota", s->db.getspecialquotawarning (qid, parentid)));
	}
	
	return &res;
}

// ==========================================================================
// METHOD quotaclass::getobject
// ==========================================================================
value *quotaclass::getobject (coresession *s,
							  const statstring &parentid,
							  const statstring &withid)
{
	returnclass (value) res retain;

	statstring mid = withid;
	statstring cuuid;
	
	if (mid.sval().strchr ('-') == 8)
	{
		mid = getmetaid (mid);
		cuuid = withid;
	}
	else
	{
		cuuid = getuuid (parentid, mid);
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
	int quota = s->db.getuserquota (withid, parentid, &usage);
	
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
// METHOD quotaclass::updateobject
// ==========================================================================
bool quotaclass::updateobject (coresession *s,
							   const statstring &_parentid,
							   const statstring &withid,
							   const value &withparam)
{
	statstring parentid = _parentid;
	int count = withparam["quota"];
    statstring mid;
	if (withid.sval().strchr ('-') == 8)
	{
		mid = getmetaid (withid);
		parentid = getparentid (withid);
		
	}
	else
	{
        mid = withid;
    }

	statstring parentmeta = s->uuidtometa (parentid);
	
	if (parentmeta == s->meta["user"])
	{
		seterror ("Cannot change own quota");
		return false;
	}
    
    CORE->log (log::debug, "quotaclass", "Updateobject id=<%s> metaid=<%s> "
    		   "parentid=<%s> param=%J" %format (withid, mid, parentid, withparam));

	if (mid)
	{
		// Non-object quotas carry a name that should not
		// clash with a classname. This is not the cleanest
		// way to distinguish them, but probably the quickest ;)
		if (s->mdb.classexists (mid))
		{
			if (s->setuserquota (mid, count, parentid)) return true;
			seterror (s->error()["message"]);
			return false;
		}
		
		bool iswarning = mid.sval().strstr (":warning") > 0;
        bool isusage = mid.sval().strstr (":usage") > 0;
        if (isusage)
        {
            // TODO: strip ':usage' from mid
            if(s->db.setspecialquotausage(mid, parentid, withparam["usage"])) return true;
            seterror (s->db.getlasterror());
            return false;
        }
        value phys;
		if (iswarning)
		{
            // a warning change should not cause physical quota changes
            // TODO: strip ':warning' from mid
			string tag = mid.sval().copyuntil (":warning");
            int curquota=s->db.getspecialquota(tag, parentid);
            if(s->db.setspecialquota(tag, parentid, curquota, count, phys)) return true;
            seterror (s->db.getlasterror());
            return false;
		}
		else
		{
		    int curwarn=s->db.getspecialquotawarning(mid, parentid);
            if (!(s->db.setspecialquota(mid, parentid, count, curwarn, phys)))
            {            
                seterror (s->db.getlasterror());
                return false;
            }                

            // now communicate phys towards moduledb
            string moderr;
            if(s->mdb.setspecialphysicalquota(mid, phys, err) == status_ok) return true;
            
            seterror (moderr);
            return false;
		}
		return false; // can't happen
	}
	if (s->setuserquota (withid, count, parentid)) return true;
	seterror (s->error()["message"]);
	return false;
}

// ==========================================================================
// CONSTRUCTOR sessionlistclass
// ==========================================================================
sessionlistclass::sessionlistclass (void)
{
}

// ==========================================================================
// DESTRUCTOR sessionlistclass
// ==========================================================================
sessionlistclass::~sessionlistclass (void)
{
}

// ==========================================================================
// METHOD sesionlistclass::listobjects
// ==========================================================================
value *sessionlistclass::listobjects (coresession *s, const statstring &pid)
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
// CONSTRUCTOR errorlogclass
// ==========================================================================
errorlogclass::errorlogclass (void)
{
}

// ==========================================================================
// DESTRUCTOR errorlogclass
// ==========================================================================
errorlogclass::~errorlogclass (void)
{
}

// ==========================================================================
// METHOD errorlogclass::listobjects
// ==========================================================================
value *errorlogclass::listobjects (coresession *s, const statstring &pid)
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
// CONSTRUCTOR coresystemclass
// ==========================================================================
coresystemclass::coresystemclass (void)
{
	sysuuid = strutil::uuid ();
}

// ==========================================================================
// DESTRUCTOR coresystemclass
// ==========================================================================
coresystemclass::~coresystemclass (void)
{
}

// ==========================================================================
// METHOD coresystemclass::listobjects
// ==========================================================================
value *coresystemclass::listobjects (coresession *s, const statstring &pid)
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
// CONSTRUCTOR classlistclass
// ==========================================================================
classlistclass::classlistclass (void)
{
}

// ==========================================================================
// DESTRUCTOR classlistclass
// ==========================================================================
classlistclass::~classlistclass (void)
{
}

// ==========================================================================
// METHOD classlistclass::listobjects
// ==========================================================================
value *classlistclass::listobjects (coresession *s, const statstring &pid)
{
	returnclass (value) res retain;
	
	res["OpenCORE:ClassList"] = s->mdb.listclasses ();
	foreach (n, res["OpenCORE:ClassList"])
	{
		n["id"] = n["metaid"] = n.id();
		n["class"] = "OpenCORE:ClassList";
	}
	
	return &res;
}
