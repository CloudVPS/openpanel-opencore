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

#include "rpcrequesthandler.h"

#include "opencore.h"
#include "session.h"
#include "debug.h"
#include "rpc.h"
#include <zlib.h>

//	=========================================================================
/// Constructor
//	=========================================================================
RPCRequestHandler::RPCRequestHandler (class OpenCoreApp *papp, httpd &server, 
					 	class SessionDB *db)
						: httpdobject (server, "*/json")
{
	app		= papp;
	sdb		= db;
}


//	=========================================================================
/// Http request handler
//	=========================================================================
int RPCRequestHandler::run (string &uri, string &postbody, value &inhdr,
                     string &out, value &outhdr, value &env,
                     tcpsocket &s)
{
	static lock<value> peercache;
	DEBUG.storeFile ("RPCRequestHandler","postbody", postbody, "run");
	CORE->log (log::debug, "rpc", "handle: %S %!" %format (uri, inhdr));
	value indata;
	value res;
	string origin = "ipc";
	uid_t uid = 0;
	RPCHandler hdl (sdb);

	indata.fromjson (postbody);
	if (inhdr.exists ("X-OpenCORE-Origin"))
	{
		origin = inhdr["X-OpenCORE-Origin"];
	}
	
	CORE->log (log::debug, "rpc", "body: %!" %format (indata));
	
	// Set up credentials if available
	s.getcredentials();
	CORE->log (log::debug, "rpc", "credentials: %d %d %d", s.peer_uid,
													s.peer_gid, s.peer_pid);
	if (s.peer_pid == 0)
	{
		string peer_name = s.peer_name;
		if (peer_name == "127.0.0.1")
		{
			statstring pid = "%i" %format ((int) s.peer_port);

			if (inhdr.exists ("X-Originating-Address"))
			{
				peer_name = inhdr["X-Originating-Address"];
				exclusivesection (peercache)
				{
					peercache[pid] = peer_name;
					if (peercache.count() > 128)
					{
						peercache.rmindex (0);
					}
				}
			}
			else
			{
				sharedsection (peercache)
				{
					if (peercache.exists (pid))
						peer_name = peercache[pid];
				}
			}
		}
			
		if (origin.strchr ('/') >0) origin = origin.cutat ('/');
		if (! origin) origin = "rpc";
		origin.strcat ("/src=%s" %format (peer_name));
	}

	res = hdl.handle (indata, s.peer_uid, origin);	
	out = res.tojson ();
	
	if (inhdr.exists ("Accept-Encoding"))
	{
		string ae = inhdr["Accept-Encoding"];
		if (ae.strstr ("deflate") >= 0)
		{
			unsigned long reslen = (out.strlen() * 1.05) + 12;
			char buf[reslen];
			
			if (compress2 ((Bytef*) buf, &reslen,
						   (const Bytef*) out.str(), out.strlen(), 4) == Z_OK)
			{
				outhdr["Content-Encoding"] = "deflate";
				out.strcpy (buf+2, reslen-2);
			}
			else
			{
				log::write (log::warning, "rpc", "Compress error");
			}
		}
	}
	
	outhdr["Content-type"] = "application/json";
	return HTTP_OK;
}


IconRequestHandler::IconRequestHandler (class OpenCoreApp *papp, httpd &serv)
	: httpdobject (serv, "/images/icons/*"), app (papp)
{
}

int IconRequestHandler::run (string &uri, string &postbody, value &inhdr,
							 string &out, value &outhdr, value &env,
							 tcpsocket &s)
{
	bool isdown = (uri.strstr ("/down/") >= 0);
	string uuid = uri.copyafterlast ("/");
	uuid.cropat ('.');
	
	app->log (log::debug, "httpicon", "Request for <%s>" %format (uuid));
	
	if (! app->mdb->classExistsUUID (uuid))
	{
		string orgpath;
		
		if (isdown)
		{
			orgpath = "/var/openpanel/http/images/icons/down_%s.png" %format (uuid);
		}
		else
		{
			orgpath = "/var/openpanel/http/images/icons/%s.png" %format (uuid);
		}
		if (fs.exists (orgpath))
		{
			out = fs.load (orgpath);
			outhdr["Content-type"] = "image/png";
			return 200;
		}
		return 404;
	}
	CoreClass &c = app->mdb->getClassUUID (uuid);
	string path;
	if (isdown)
	{
		path = "%s/down_%s" %format (c.module.path, c.icon);
	}
	else
	{
		path = "%s/%s" %format (c.module.path, c.icon);
	}
	
	app->log (log::debug, "httpicon", "Loading %s" %format (path));
	
	if (! fs.exists (path)) return 404;
	
	outhdr["Content-type"] = "image/png";
	out = fs.load (path);
	return 200;
}

ItemIconRequestHandler::ItemIconRequestHandler (class OpenCoreApp *papp, httpd &serv)
	: httpdobject (serv, "/images/itemicons/*"), app (papp)
{
}

int ItemIconRequestHandler::run (string &uri, string &postbody, value &inhdr,
							 string &out, value &outhdr, value &env,
							 tcpsocket &s)
{
	string uuid = uri.copyafterlast ("/");
	uuid.cropat ('.');
	
	app->log (log::debug, "httpicon", "Request for <%s>" %format (uuid));
	
	if (! app->mdb->classExistsUUID (uuid))
	{
		string orgpath;
		
		orgpath = "/var/openpanel/http/images/icons/%s_item.png" %format (uuid);

		if (fs.exists (orgpath))
		{
			out = fs.load (orgpath);
			outhdr["Content-type"] = "image/png";
			return 200;
		}
		return 404;
	}
	
	CoreClass &c = app->mdb->getClassUUID (uuid);
	string path = "%s/item_%s" %format (c.module.path, c.icon);
	app->log (log::debug, "itemicon", "Loading %s" %format (path));
	
	if (! fs.exists (path)) return 404;
	
	outhdr["Content-type"] = "image/png";
	out = fs.load (path);
	return 200;
}

EmblemRequestHandler::EmblemRequestHandler (class OpenCoreApp *papp, httpd &serv)
	: httpdobject (serv, "/images/emblems/*"), app (papp)
{
}

int EmblemRequestHandler::run (string &uri, string &postbody, value &inhdr,
							   string &out, value &outhdr, value &env,
							   tcpsocket &s)
{
	string uuid = uri.copyafterlast ("/");
	uuid.cropat ('.');
	
	if (! app->mdb->classExistsUUID (uuid))
	{
		out = "%s not found" %format (uuid);
		return 404;
	}
	
	CoreClass &c = app->mdb->getClassUUID (uuid);
	string path = "%s/large_%s" %format (c.module.path, c.icon);
	if (! fs.exists (path)) return 404;
	
	outhdr["Content-type"] = "image/png";
	out = fs.load (path);
	return 200;
}
