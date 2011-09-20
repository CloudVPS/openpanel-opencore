// This file is part of OpenPanel - The Open Source Control Panel
// OpenPanel is free software: you can redistribute it and/or modify it 
// under the terms of the GNU General Public License as published by the Free 
// Software Foundation, using version 3 of the License.
//
// Please note that use of the OpenPanel trademark may be subject to additional 
// restrictions. For more information, please visit the Legal Information 
// section of the OpenPanel website on http://www.openpanel.com/

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
	try
	{
		DEBUG.storeFile ("RPCRequestHandler","postbody", postbody, "run");
		CORE->log (log::debug, "RPC", "handle: %S %!" %format (uri, inhdr));
		value indata;
		value res;
		string origin = "rpc";
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
				if (inhdr.exists ("X-Forwarded-For"))
				{
					peer_name = inhdr["X-Forwarded-For"];
				}
			}
				
			if (origin.strchr ('/') >0) origin = origin.cutat ('/');
			if (! origin) origin = "RPC";

			origin.strcat ("/src=%s" %format (peer_name));
			env["ip"] = s.peer_name = peer_name;
		}

		if (indata.exists ("header") && indata["header"].exists ("command"))
		{
			uri.strcat ("/%s" %format (indata["header"]["command"]));
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
	: httpdobject (serv, "/dynamic/icons/*"), app (papp)
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
	
	if (inhdr.exists ("If-Modified-Since"))
	{
		s.puts ("HTTP/1.1 304 NOT CHANGED\r\n"
				"Connection: %s\r\n"
				"Content-length: 0\r\n\r\n"
				%format (env["keepalive"].bval() ? "keepalive" : "close"));
		
		env["sentbytes"] = 0;
		return -304;
	}
	
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
	: httpdobject (serv, "/dynamic/itemicons/*"), app (papp)
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

WallpaperHandler::WallpaperHandler (class OpenCoreApp *papp, httpd &serv)
	: httpdobject (serv, "/dynamic/wallpaper*"), app (papp)
{
}

int WallpaperHandler::run (string &uri, string &postbody, value &inhdr,
						   string &out, value &outhdr, value &env,
						   tcpsocket &s)
{
	outhdr["Content-type"] = "image/jpeg";
	
	if (uri == "/dynamic/wallpaper.jpg")
	{
		string p = WallpaperClass::getCurrentWallpaper();
		log::write (log::info, "WallP", "Serving %s" %format (p));
		if (fs.exists (p)) out = fs.load (p);
		return 200;
	}
	else
	{
		string fn = uri.copyafterlast ("/");
		string path = "/var/openpanel/wallpaper/%s.preview" %format (fn);
		if (fs.exists (path)) out = fs.load (path);
		return 200;
	}
}

// ==========================================================================
// CONSTRUCTOR EmblemRequestHandler
// ==========================================================================
EmblemRequestHandler::EmblemRequestHandler (class OpenCoreApp *papp, httpd &serv)
	: httpdobject (serv, "/dynamic/emblems/*"), app (papp)
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
LandingPageHandler::LandingPageHandler (OpenCoreApp *papp, httpd &srv, 	class SessionDB		*sessionDB )
	: httpdobject (srv, "/dynamic/welcome.html")
{
	app = papp;
	sdb = sessionDB;
	schema.load ("schema:rss.2.0.schema.xml");
}

// ==========================================================================
// METHOD LandingPageHandler::run
// ==========================================================================
int LandingPageHandler::run (string &uri, string &postbody, value &inhdr,
							 string &out, value &outhdr, value &env,
							 tcpsocket &s)
{
	CoreSession *session = NULL;	

	int pos;
	if ((pos = uri.strchr ('?')) >= 0)
	{
		string reqdata = uri.mid (pos+1);
		value reqenv = strutil::httpurldecode (reqdata);

		
		session = sdb->get (reqenv["sid"]);	
	}	
	
	if (!session)
	{
		// can't allow access without a session
		return 403;
	}
	
	
	value senv;
	struct utsname name;
	
	senv["admin"]=session->isAdmin();
	senv["session"]=session->meta;
	
	sdb->release(session);
	
	uname (&name);
	senv = $("os_name",name.sysname)->
		   $("os_release",name.release)->
		   $("openpanel_release",version::release);

	if (fs.exists ("/proc/cpuinfo")) 
	{
		string scpuinfo;
		scpuinfo = fs.load ("/proc/cpuinfo");
		value lcpuinfo = strutil::splitlines (scpuinfo);	
		foreach (l,lcpuinfo)
		{
			if (l.sval().strncasecmp ("model name",10) == 0)
			{
				string scpu = l.sval().copyafter (": ");
				scpu.replace ($("(R)","&reg;"));
				scpu.replace ($("(tm)","&trade;"));
				scpu.replace ($("(TM)","&trade;"));
				senv["os_cpu"] = scpu;
			}			
			else if (l.sval().strncasecmp ("processor",9) == 0)
			{
				string scpu = l.sval().copyafter (": ");
				scpu.replace ($("(R)","&reg;"));
				scpu.replace ($("(tm)","&trade;"));
				scpu.replace ($("(TM)","&trade;"));
				senv["os_cpu"] = scpu;
			} 
			else if (l.sval().strncasecmp ("hardware",8) == 0)
			{
				string scpu = l.sval().copyafter (": ");
				scpu.replace ($("(R)","&reg;"));
				scpu.replace ($("(tm)","&trade;"));
				scpu.replace ($("(TM)","&trade;"));
				senv["os_hw"] = scpu;
			}			
			else if (l.sval().strncasecmp ("model",5) == 0)
			{
				string scpu = l.sval().copyafter (": ");
				scpu.replace ($("(R)","&reg;"));
				scpu.replace ($("(tm)","&trade;"));
				scpu.replace ($("(TM)","&trade;"));
				senv["os_cpu"] = scpu;
			} 
			else if (l.sval().strncasecmp ("machine",7) == 0)
			{
				string scpu = l.sval().copyafter (": ");
				scpu.replace ($("(R)","&reg;"));
				scpu.replace ($("(tm)","&trade;"));
				scpu.replace ($("(TM)","&trade;"));
				senv["os_hw"] = scpu;
			} 
		}
	}	
	
	if (fs.exists ("/etc/redhat-release"))
	{
		senv["os_distro"] = fs.load ("/etc/redhat-release");
	}
	else if (fs.exists ("/etc/lsb-release"))
	{
		value lsb;
		lsb.loadini ("/etc/lsb-release");
		if (lsb.exists ("DISTRIB_DESCRIPTION"))
		{
			senv["os_distro"] = lsb["DISTRIB_DESCRIPTION"];
		}
		else
		{
			senv["os_distro"] = "Unknown LSB distribution";
		}
	}
	else if (fs.exists ("/etc/debian_version"))
	{
		senv["os_distro"] = "Debian %s" %format(fs.load ("/etc/debian_version"));
	}
	
	senv["updates_count"] = "unavailable";
	
	if (fs.exists ("/var/openpanel/db/softwareupdate.db"))
	{
		value updates;
		updates.loadshox ("/var/openpanel/db/softwareupdate.db");
		int count = updates.count();
		senv["updates_count"] = count;
		
		if (count)
		{
			string description = "<b>";
			for (int i=0; (i<5) && (i<count); ++i)
			{
				if (i) description.strcat (", ");
				description.strcat (updates[i].id());
			}
			
			description.strcat ("</b>");
			
			if (count > 5)
			{
				if (count == 6)
				{
					description.strcat (" and one other");
				}
				else
				{
					description.strcat (" and %i others" %format (count-5));
				}
			}
			
			senv["updates_description"] = description;
		}
	}
	
	if (fs.exists ("/proc/uptime")) 
	{
		string suptime;
		suptime = fs.load ("/proc/uptime");
		suptime.cropat (' ');
		int iuptime = suptime.toint(10);
	
		senv["uptime_days"] = iuptime / 86400;
		senv["uptime_hours"] = (iuptime % 86400) / 3600;
		senv["uptime_minutes"] = (iuptime % 3600) / 60;
		senv["uptime_seconds"] = iuptime % 60;
		senv["uptime_hms"] = "%i:%02i:%02i" %format (senv["uptime_hours"],
								senv["uptime_minutes"], senv["uptime_seconds"]);
	}	

	if (fs.exists ("/proc/meminfo")) 
	{
		string smeminfo = fs.load ("/proc/meminfo");
		value lmeminfo = strutil::splitlines (smeminfo);
		
		foreach (l,lmeminfo)
		{
			if (l.sval().strncasecmp ("MemTotal:",9) == 0)
			{
				string smem = l.sval().copyafter (": ");
				smem = smem.trim();
				senv["mem_total"] = smem;
			}
			else if (l.sval().strncasecmp ("Active:",7) == 0)
			{
				string smem = l.sval().copyafter (": ");
				smem = smem.trim();
				senv["mem_active"] = smem;
			} 
		}
	}	
	
	if (fs.exists ("/proc/loadavg")) 
	{
		string sload;
		sload = fs.load ("/proc/loadavg");
		value vload = strutil::splitspace (sload);
		senv["load_1"] = vload[0];
		senv["load_5"] = vload[1];
		senv["load_15"] = vload[2];
	}	
	
	
	value output;
	
	systemprocess proc ($("/bin/df")->$("-kPl"));
	proc.run();
	while (! proc.eof())
	{
		output.newval() = proc.gets();
	}
	proc.close();
	proc.serialize();
	
	value skipfs = $("udev",true) -> $("none",true) -> $("devtmpfs",true);
	
	for (int i=1; i<output.count(); ++i)
	{
		value splt = strutil::splitspace (output[i]);
		if (splt.count() < 6) continue;
		if (skipfs.exists (splt[0].sval())) continue;
		
		value &into = senv["mounts"][splt[5].sval()];
		
		into = $("device",splt[0])->
			   $("size",splt[1].ulval() / (1024 * 1024))->
			   $("usage",splt[4].ival())->
			   $("freepct",100-splt[4].ival())->
			   $("mountpoint",splt[5]);
	}
	
	value rss = getRSS ("http://blog.openpanel.com/feed/");
	
	foreach (item, rss[0])
	{
		if (item.count())
		{
			value &into = senv["devrss"][item["guid"]];
			into["title"] = item["title"];
			into["url"	] = item["link"];
			if (senv["devrss"].count() > 4) break;
		}
	}
	
	rss = getRSS ("http://forum.openpanel.com/index.php?type=rss;action=.xml");
	
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

	

	
	
	try
	{
		string script = fs.load ("/var/openpanel/http/dynamic/welcome.html");
		scriptparser p;
		p.build (script);
	
		p.run (senv, out);
	}
	catch (...)
	{
		out = "Exception caught";
	}
	outhdr["Content-type"] = "text/html";
	return 200;
}

value *LandingPageHandler::getRSS (const string &url)
{
	returnclass (value) res retain;
	
	timestamp tnow = core.time.now();
	sharedsection (rsscache)
	{
		if (rsscache.exists (url))
		{
			timestamp tcache = rsscache[url]["ts"];
			tcache += 300;
			
			if (tnow > tcache)
			{
				res = rsscache[url]["data"];
				return &res;
			}
		}
	}
	
	httpsocket hs;
	value rss;
	
	hs.setheader ("User-Agent", "OpenPanel/%s" %format (version::release));
	
	string rssdat = hs.get (url);
	rss.fromxml (rssdat, schema);
	
	exclusivesection (rsscache)
	{
		rsscache[url]["ts"] = tnow;
		rsscache[url]["data"] = rss;
	}
	
	res = (const value &) rss;
	return &res;
}
