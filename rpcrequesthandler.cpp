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
#include "version.h"
#include <zlib.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <grace/http.h>

// ==========================================================================
// CONSTRUCTOR RPCRequestHandler
// ==========================================================================
RPCRequestHandler::RPCRequestHandler (class OpenCoreApp *papp, httpd &server, 
					 	class SessionDB *db)
						: httpdobject (server, "*/json")
{
	app		= papp;
	sdb		= db;
}

// ==========================================================================
// METHOD RPCRequestHandler::run
// ==========================================================================
int RPCRequestHandler::run (string &uri, string &postbody, value &inhdr,
                     string &out, value &outhdr, value &env,
                     tcpsocket &s)
{
	static lock<value> peercache;
	
	try
	{
		DEBUG.storeFile ("RPCRequestHandler","postbody", postbody, "run");
		CORE->log (log::debug, "RPC", "handle: %S %!" %format (uri, inhdr));
		value indata;
		value res;
		string origin = "IPC";
		uid_t uid = 0;
		RPCHandler hdl (sdb);
	
		indata.fromjson (postbody);
		if (inhdr.exists ("X-OpenCORE-Origin"))
		{
			origin = inhdr["X-OpenCORE-Origin"];
		}
		
		CORE->log (log::debug, "RPC", "body: %!" %format (indata));
		
		// Set up credentials if available
		s.getcredentials();
		CORE->log (log::debug, "RPC", "credentials: %d %d %d", s.peer_uid,
														s.peer_gid, s.peer_pid);
		if (s.peer_pid == 0)
		{
			string peer_name = s.peer_name;
			if (peer_name == "127.0.0.1")
			{
				statstring pid = "%i" %format ((int) s.peer_port);
	
				if (inhdr.exists ("X-Forwarded-For"))
				{
					peer_name = inhdr["X-Forwarded-For "];
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
			if (! origin) origin = "RPC";
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
					log::write (log::warning, "RPC", "Compress error");
				}
			}
		}
		
		outhdr["Content-type"] = "application/json";
	}
	catch (...)
	{
		log::write (log::error, "RPC", "Exception caught");
	}
	return HTTP_OK;
}


// ==========================================================================
// CONSTRUCTOR IconRequestHandler
// ==========================================================================
IconRequestHandler::IconRequestHandler (class OpenCoreApp *papp, httpd &serv)
	: httpdobject (serv, "/images/icons/*"), app (papp)
{
}

// ==========================================================================
// METHOD IconRequestHandler::run
// ==========================================================================
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

// ==========================================================================
// CONSTRUCTOR ItemIconRequestHandler
// ==========================================================================
ItemIconRequestHandler::ItemIconRequestHandler (class OpenCoreApp *papp, httpd &serv)
	: httpdobject (serv, "/images/itemicons/*"), app (papp)
{
}

// ==========================================================================
// METHOD ItemIconRequestHandler::run
// ==========================================================================
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

// ==========================================================================
// CONSTRUCTOR EmblemRequestHandler
// ==========================================================================
EmblemRequestHandler::EmblemRequestHandler (class OpenCoreApp *papp, httpd &serv)
	: httpdobject (serv, "/images/emblems/*"), app (papp)
{
}

// ==========================================================================
// METHOD EmblemRequestHandler::run
// ==========================================================================
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

// ==========================================================================
// CONSTRUCTOR ImagePreloader
// ==========================================================================
ImagePreloader::ImagePreloader (OpenCoreApp *papp, httpd &x)
	: httpdobject (x, "/dynamic/imagelist.*")
{
	app = papp;
}

// ==========================================================================
// DESTRUCTOR ImagePreloader
// ==========================================================================
ImagePreloader::~ImagePreloader (void)
{
}

// ==========================================================================
// METHOD ImagePreloader::run
// ==========================================================================
int ImagePreloader::run (string &uri, string &postbody, value &inhdr,
						 string &out, value &outhdr, value &env,
						 tcpsocket &s)
{
	string fname = uri.copyafterlast ("/");
	if (fname == "imagelist.js")
	{
		out = "function preloadImages () {\n"
			  "preloadedGUIImages = new Array();\n";
		value dir = fs.dir ("/var/openpanel/http/images/gui");
		foreach (img, dir)
		{
			string ext = img.sval().copyafterlast('.');
			if ((ext != "png")&&(ext!="jpg")&&(ext!="gif")) continue;
			
			out += "preloadedGUIImages[\"%{0}s\"] = new Image(32,32);\n"
				   "preloadedGUIImages[\"%{0}s\"].src = \"/images/gui/%{0}s\";\n"
				   %format (img.id());
		}
		
		out += "}\n";
	}
	else
	{
		value images;
		value dir = fs.dir ("/var/openpanel/http/images/gui");
		foreach (img, dir)
		{
			string ext = img.sval().copyafterlast('.');
			if ((ext != "png")&&(ext!="jpg")&&(ext!="gif")) continue;
			
			images.newval() = "/images/gui/%s" %format (img.id());
		}
		
		out = images.tojson();
	}
	
	outhdr["Content-type"] = "text/javascript";
	return 200;
}

// ==========================================================================
// CONSTRUCTOR LandingPageHandler
// ==========================================================================
LandingPageHandler::LandingPageHandler (OpenCoreApp *papp, httpd &srv)
	: httpdobject (srv, "/dynamic/welcome.html")
{
	app = papp;
	schema.load ("schema:rss.2.0.schema.xml");
}

// ==========================================================================
// METHOD LandingPageHandler::run
// ==========================================================================
int LandingPageHandler::run (string &uri, string &postbody, value &inhdr,
							 string &out, value &outhdr, value &env,
							 tcpsocket &s)
{
	value senv;
	struct utsname name;
	
	uname (&name);
	senv = $("os_name",name.sysname)->
		   $("os_release",name.release)->
		   $("openpanel_release",version::release);

	string scpuinfo = fs.load ("/proc/cpuinfo");
	value lcpuinfo = strutil::splitlines (scpuinfo);
	foreach (l,lcpuinfo)
	{
		if (l.sval().strncmp ("model name",10)) continue;
		string scpu = l.sval().copyafter (": ");
		scpu.replace ($("(R)","&reg;"));
		senv["os_cpu"] = scpu;
		break;
	}
	
	if (fs.exists ("/etc/redhat-release"))
	{
		senv["os_distro"] = fs.load ("/etc/redhat-release");
	}
	else if (fs.exists ("/etc/debian_version"))
	{
		senv["os_distro"] = fs.load ("/etc/debian_version");
	}
	else if (fs.exists ("/etc/lsb-release"))
	{
		senv["os_distro"] = fs.load ("/etc/lsb-release");
	}
	
	string suptime = fs.load ("/proc/uptime");
	suptime.cropafter (' ');
	int iuptime = suptime.toint(10);
	
	senv["uptime_days"] = iuptime / 86400;
	senv["uptime_hours"] = (iuptime % 86400) / 3600;
	senv["uptime_minutes"] = (iuptime % 3600) / 60;
	senv["uptime_seconds"] = iuptime % 60;
	senv["uptime_hms"] = "%i:%02i:%02i" %format (senv["uptime_hours"],
							senv["uptime_minutes"], senv["uptime_seconds"]);
	
	string sload = fs.load ("/proc/loadavg");
	value vload = strutil::splitspace (sload);
	senv["load_1"] = vload[0];
	senv["load_5"] = vload[1];
	senv["load_15"] = vload[2];
	
	value output;
	
	systemprocess proc ($("/bin/df")->$("-kPl"));
	proc.run();
	while (! proc.eof())
	{
		output.newval() = proc.gets();
	}
	proc.close();
	proc.serialize();
	
	for (int i=1; i<output.count(); ++i)
	{
		value splt = strutil::splitspace (output[i]);
		if (splt.count() < 6) continue;
		value &into = senv["mounts"][splt[0].sval()];
		
		into = $("device",splt[0])->
			   $("size",splt[1].ival() / (1024 * 1024))->
			   $("usage",splt[4].ival())->
			   $("freepct",100-splt[4].ival())->
			   $("mountpoint",splt[5]);
	}
	
	httpsocket hs;
	value rss;
	string rssdat = hs.get ("http://blog.openpanel.com/feed/");
	rss.fromxml (rssdat, schema);
	
	foreach (item, rss[0])
	{
		if (item.count())
		{
			value &into = senv["devrss"][item["guid"]];
			into["title"] = item["title"];
			into["url"] = item["link"];
			if (senv["devrss"].count() > 4) break;
		}
	}
	
	rssdat = hs.get ("http://forum.openpanel.com/index.php?type=rss;action=.xml");
	rss.fromxml (rssdat, schema);
	
	foreach (item, rss[0])
	{
		if (item.count())
		{
			value &into = senv["forumrss"][item["guid"]];
			into["title"] = item["title"];
			into["url"] = item["link"];
			if (senv["forumrss"].count() > 4) break;
		}
	}
	
	string script = fs.load ("/var/openpanel/http/dynamic/welcome.html");
	scriptparser p;
	p.build (script);
	
	p.run (senv, out);
	outhdr["Content-type"] = "text/html";
	return 200;
}
