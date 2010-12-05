// This file is part of OpenPanel - The Open Source Control Panel
// OpenPanel is free software: you can redistribute it and/or modify it 
// under the terms of the GNU General Public License as published by the Free 
// Software Foundation, using version 3 of the License.
//
// Please note that use of the OpenPanel trademark may be subject to additional 
// restrictions. For more information, please visit the Legal Information 
// section of the OpenPanel website on http://www.openpanel.com/

#include "opencore.h"
#include "version.h"
#include "debug.h"
#include "alerts.h"

#include <grace/defaults.h>

#include <grace/thread.h>

APPOBJECT(OpenCoreApp);

OpenCoreApp *CORE = NULL;
bool APP_SHOULDRUN;

// ==========================================================================
// CONSTRUCTOR OpenCoreApp
// ==========================================================================
OpenCoreApp::OpenCoreApp (void)
	: daemon ("com.openpanel.svc.opencore"),
	  conf (this),
	  shell (this, fin, fout)
{
	CORE = this;
}

// ==========================================================================
// DESTRUCTOR OpenCoreApp
// ==========================================================================
OpenCoreApp::~OpenCoreApp (void)
{
}

#define PRINTERR(x) ferr.printf (x ": (%04i) %s\n", \
				s->errorCount() ? s->error(-1)["code"].ival() : 0, \
				s->errorCount() ? s->error(-1)["message"].cval() : \
				"Unknown error")

// Import from debug.cpp
extern bool DISABLE_DEBUGGING;

void OpenCoreApp::dancingBears (void)
{
	ferr.writeln ("    _--_     _--_    _--_     _--_     _--_     _--_     _--_     _--_");
	ferr.writeln ("   (    )~~~(    )  (    )~~~(    )   (    )~~~(    )   (    )~~~(    )");
	ferr.writeln ("    \\           /    \\           /     \\           /     \\           /");
	ferr.writeln ("     (  ' _ `  )      (  ' _ `  )       (  ' _ `  )       (  ' _ `  )");
	ferr.writeln ("      \\       /        \\       /         \\       /         \\       /");
	ferr.writeln ("    .__( `-' )          ( `-' )           ( `-' )        .__( `-' )  ___");
	ferr.writeln ("   / !  `---' \\      _--'`---_          .--`---'\\       /   /`---'`-'   \\");
	ferr.writeln ("  /  \\         !    /         \\___     /        _>\\    /   /          ._/   __");
	ferr.writeln (" !   /\\        )   /   /       !  \\   /  /-___-'   ) /'   /.-----\\___/     /  )");
	ferr.writeln (" !   !_\\       ). (   <        !__/ /'  (        _/  \\___//          `----'   !");
	ferr.writeln ("  \\    \\       ! \\ \\   \\      /\\    \\___/`------' )       \\            ______/");
	ferr.writeln ("   \\___/   )  /__/  \\--/   \\ /  \\  ._    \\      `<         `--_____----'");
	ferr.writeln ("     \\    /   !       `.    )-   \\/  ) ___>-_     \\   /-\\    \\    /");
	ferr.writeln ("     /   !   /         !   !  `.    / /      `-_   `-/  /    !   !");
	ferr.writeln ("    !   /__ /___       /  /__   \\__/ (  \\---__/ `-_    /     /  /__");
	ferr.writeln ("    (______)____)     (______)        \\__)         `-_/     (______)");
}

void OpenCoreApp::memoryLeakHandler (void)
{
	log::write (log::critical, "Main", "Memory leak detected. Application "
				"performance will suffer.");
}

// ==========================================================================
// METHOD OpenCoreApp::main
// ==========================================================================
int OpenCoreApp::main (void)
{
	// Enterprise mode ON.
	defaults::memory::leakprotection = false;
	defaults::memory::leakcallback = memoryLeakHandler;
	
	if (argv.exists ("--emit-dancing-bears"))
	{
		dancingBears ();
	}
	
	string conferr; // Error return from configuration class.
	DISABLE_DEBUGGING = true;
	bool dconsole = false;
	
	if (argv.exists ("--debugging-console"))
	{
		DISABLE_DEBUGGING = false;
		dconsole = true;
		setforeground(); // No daemonizing to background for now.
	}
	
	if (argv.exists ("--debug"))
	{
		ferr.writeln ("Using debugging flags: %s" %format (argv["--debug"]));
		DISABLE_DEBUGGING = false;
		
		if (argv["--debug"] != "all")
		{
			value tval = strutil::split (argv["--debug"], ',');
			foreach (v, tval) debugfilter[v] = true;
			DEBUG.setFilter (debugfilter);
		}
	}

	if (! checkAuthDaemon ()) return 1;

	// Discard of any groups in our set.
	setgroups (0, 0);
	
	// Storage variables for resolving our credentials.
	uid_t uid_opencore = 0;
	gid_t gid_opencore = 0;
	gid_t gid_authd = 0;
	value pw;
	value gw;
	
	// Find the opencore user.
	pw = kernel.userdb.getpwnam ("openpanel-core");
	if (! pw)
	{
		ferr.writeln ("Could not find opencore user");
		return false;
	}
	uid_opencore = pw["uid"].uval();
	gid_opencore = pw["gid"].uval();
	
	// Find the authd group
	gw = kernel.userdb.getgrnam ("openpanel-authd");
	if (! gw)
	{
		ferr.writeln ("Could not find openpanel-authd group");
		return false;
	}
	gid_authd = gw["gid"].uval();
	
	// Fiddle the credentials we should get.
	settargetgroups ($("openpanel-authd"));
	settargetgid (gid_opencore, gid_opencore);
	settargetuid (uid_opencore);

	// Set up collection objects.
	mdb = new ModuleDB;
	sdb = new SessionDB (*mdb);
	
	if (fs.exists ("/var/openpanel/db/session.xml"))
	{
		sdb->loadFromDisk ("/var/openpanel/db/session.xml");
	}
	
	rpc = NULL; // will be initialized by confRpc.
		
	// Add watcher value for event log. System will daemonize after
	// configuration was validated.
	conf.addwatcher ("system", &OpenCoreApp::confSystem);
	
	// Add confwatcher for the daemons RPC configuration
	conf.addwatcher ("rpc", &OpenCoreApp::confRpc);
	
	// Load will fail if watchers did not valiate.
	if (! conf.load ("com.openpanel.svc.opencore", conferr))
	{
		ferr.writeln ("%% Error loading configuration: %s" %format(conferr));
		return 1;
	}

	log (log::info, "Main", "Application starting");
	
	exclusivesection (regexpdb)
	{
		regexpdb.loadxml ("rsrc:regexpclass.rsrc.xml");
		if (! regexpdb.count())
		{
			log (log::warning, "Main", "regexpclass.rsrc.xml load failed");
		}
	}

	// Set up alert and session expire threads.
	ALERT = new AlertHandler (conf["alert"]);
	sexp = new SessionExpireThread (sdb);

	// Get the list of modules that should be reinitialized through their
	// getconfig.
	value initlist;
	if (argv.exists ("--reinitialize-module-config"))
	{
		initlist = strutil::split (argv["--reinitialize-module-config"],',');
	}

	// Load modules.
	mdb->init (initlist);
	
	if (dconsole)
	{
		commandline (); // This will go :)
	}
	else
	{
		// Detach if we're not using the debugging console.
		log (log::debug, "Main", "Detaching parent");
		delayedexitok ();
		log (log::info, "Main", "All subsystems initialized");

		APP_SHOULDRUN = true;
		while (APP_SHOULDRUN)
		{
			value ev = waitevent ();
			if (ev.type() == "shutdown") APP_SHOULDRUN = false;
		}
	}
	
	log (log::info, "Main", "Shutting down");

	sdb->saveToDisk ("/var/openpanel/db/session.xml");

	sexp->shutdown();
	ALERT->shutdown();
	stoplog();
	return 0;
}

// ==========================================================================
// METHOD OpenCoreApp::checkAuthDaemon
// ==========================================================================
bool OpenCoreApp::checkAuthDaemon (void)
{
	tcpsocket sauth;
	
	if (! sauth.uconnect ("/var/openpanel/sockets/authd/authd.sock"))
	{
		ferr.writeln ("% Error connecting to authd socket");
		return false;
	}
	
	try
	{
		sauth.writeln ("hello openpanel-core");
		string line;
		line = sauth.gets();
		if (! line.strlen())
		{
			ferr.writeln ("% Error getting reply from authd socket");
			return false;
		}
		if (line[0] != '+')
		{
			ferr.writeln ("%% Error from authd: %s\n" %format (line));
			return false;
		}
		sauth.writeln ("quit");
		line = sauth.gets();
	}
	catch (...)
	{
	}
	
	sauth.close ();
	return true;
}

// ==========================================================================
// METHOD OpenCoreApp::confSystem
// ==========================================================================
bool OpenCoreApp::confSystem (config::action act, keypath &kp,
						      const value &nval, const value &oval)
{
	string tstr;
	
	switch (act)
	{
		case config::isvalid:
			// Check if the path for the event log exists.
			tstr = strutil::makepath (nval["eventlog"].sval());
			if (! tstr.strlen()) return true;
			if (! fs.exists (tstr))
			{
				ferr.writeln ("%% Event log path %s does not exist" %format(tstr));
				return false;
			}
			if (! nval.exists ("debuglog")) return true;
			tstr = strutil::makepath (nval["debuglog"].sval());
			if (! tstr.strlen()) return true;
			if (! fs.exists (tstr))
			{
				ferr.writeln ("%% Debug log path %s does not exist" %format(tstr));
				return false;
			}
			return true;
			
		case config::create:
			// Set the event log target and daemonize.
			addlogtarget (log::file, nval["eventlog"].sval(),
						  log::allinfo, 1024*1024);
						  
			if (nval.exists ("debuglog") && (! DISABLE_DEBUGGING))
			{
				addlogtarget (log::file, nval["debuglog"].sval(), log::debug,
							  1024*1024);
			}
			
			daemonize(true);
			return true;
	}
	
	return false;
}

// ==========================================================================
// METHOD OpenCoreApp::confRpc
// ==========================================================================
bool OpenCoreApp::confRpc (config::action act, keypath &kp,
						   const value &nval, const value &oval)
{
	if (! rpc) rpc = new OpenCoreRPC (sdb, this);

	switch (act)
	{
		// Validate given configuration
		case config::isvalid:
			if (! rpc->confcheck (nval))
			{
				log (log::error, "openrpc ", "RPC config error: %s"
					 %format (rpc->error));
				return false;
			}
			return true;

		case config::create:
			if (! rpc->confcreate (nval))
			{
				log (log::critical, "openrpc ", "RPC setup failed: %s"
					 %format (rpc->error));
				return false;
			}
			return true;

		case config::change:
			if (! rpc->confupdate (nval))
			{
				log (log::error, "openrpc ", "RPC reconfig failed: %s"
					 %format (rpc->error));
				return false;
			}
			return true;
	}
	
	return false;
}

// ==========================================================================
// METHOD OpenCoreApp::commandline
// ==========================================================================
void OpenCoreApp::commandline (void)
{
	bool shouldrun = true;
	
	shell.addsrc    ("@sessionid", &OpenCoreApp::srcSessionId);
	
	shell.addsyntax ("show classes", &OpenCoreApp::cmdShowClasses);
	shell.addsyntax ("show session", &OpenCoreApp::cmdShowSessions);
	shell.addsyntax ("show session @sessionid", &OpenCoreApp::cmdShowSession);
	
	shell.addsyntax ("show threads", &OpenCoreApp::cmdShowThreads);
	shell.addsyntax ("show version", &OpenCoreApp::cmdShowVersion);
	shell.addsyntax ("exit", &OpenCoreApp::cmdExit);
	
	shell.addhelp ("show", "Display information");
	shell.addhelp ("show classes", "All class registrations");
	shell.addhelp ("show session", "All active sessions (or specify id)");
	shell.addhelp ("show threads", "Active system threads");
	shell.addhelp ("show version", "Version information");
	shell.addhelp ("exit", "Exit admin console and stop OpenCORE");
	
	shell.setprompt ("opencore# ");
	
	shell.run ("opencore# ");
}

// ==========================================================================
// METHOD OpenCoreApp::cmdShowSession
// ==========================================================================
int OpenCoreApp::cmdShowSession (const value &cmdata)
{
	if (cmdata.count() != 3)
	{
		ferr.writeln ("% Wuh");
		return 0;
	}
	
	CoreSession *s;
	statstring sid;
	
	sid = cmdata[2].sval();
	
	s = sdb->get (sid);
	if (! s)
	{
		ferr.writeln ("% Unknown session");
		return 0;
	}
	
	string out;
	value dat = $("OpenCORE:Session",$("id",sid));
	
	out = dat.encode();
	fout.puts (out);
	
	sdb->release (s);
	return 0;
}

// ==========================================================================
// METHOD OpenCoreApp::srcSessionId
// ==========================================================================
value *OpenCoreApp::srcSessionId (const value &cmd, int p)
{
	returnclass (value) res retain;
	
	value lst = sdb->list ();
	foreach (row, lst)
	{
		res[row.id()] = row["user"];
	}
	return &res;
}

// ==========================================================================
// METHOD OpenCoreApp::cmdShowSessions
// ==========================================================================
int OpenCoreApp::cmdShowSessions (const value &cmdata)
{
	value res;
	res = sdb->list ();
	int i=1;
	foreach (row, res)
	{
		string str;
		string org;
		fout.printf ("%2i \033[1m", i++);
		str = row["user"].sval();
		
		org = row["origin"].sval();
		
		str.crop (74 - org.strlen());
		str.pad (74 - org.strlen(), ' ');
		fout.puts (str);
		fout.writeln ("\033[0m[%s]" %format (org));
		
		fout.writeln ("   %s" %format (row.id()));
	}
	return 0;
}

// ==========================================================================
// METHOD OpenCoreApp::cmdShowVersion
// ==========================================================================
int OpenCoreApp::cmdShowVersion (const value &cmdata)
{
	fout.writeln ("OpenCORE version %s - %s (%s@%s)"
				 %format (version::release, version::date,
				 		  version::user, version::hostname));
				 
	fout.writeln ("Available under the GNU General Public License");
	fout.writeln ("Copyright (C) 2006-2010 OpenPanel");
	return 0;
}

// ==========================================================================
// METHOD OpenCoreApp::cmdShowThreads
// ==========================================================================
int OpenCoreApp::cmdShowThreads (const value &cmdata)
{
	value v;
	string out;
	v = thread::getlist ();
	out = v.encode();
	fout.writeln (out);
	return 0;
}

// ==========================================================================
// METHOD OpenCoreApp::cmdShowClasses
// ==========================================================================
int OpenCoreApp::cmdShowClasses (const value &cmdata)
{
	shell.setprompt ("opencore# ");
	fout.writeln ("Class                                           Module");
	string out;
	foreach (cl, mdb->classlist)
	{
		out = cl.id();
		out.pad (48, ' ');
		out.strcat (cl.sval());
		fout.writeln (out);
	}
	return 0;
}

// ==========================================================================
// METHOD openoreApp::logError
// ==========================================================================
void OpenCoreApp::logError (const string &who, const string &_what)
{
	string what = "%s (%s)" %format (_what, DEBUG.uuid());
	daemon::log (log::error, who, what);
	
	timestamp ts;
	ts = core.time.now ();
	string tstr = ts.format ("%b %e %H:%M:%S");
	
	exclusivesection (errors)
	{
		if (errors.count() > 100) errors.rmindex (0);
		errors.newval() = $("time", tstr) ->
						  $("subsystem", who) ->
						  $("message", what);
	}
}

// ==========================================================================
// METHOD OpenCoreApp::log
// ==========================================================================
void OpenCoreApp::log (log::priority p, const string &who, const string &_what)
{
	if (p == log::error)
	{
		logError (who, _what);
		return;
	}
	
	if (p == log::debug)
	{
		if (DISABLE_DEBUGGING) return;

		if (debugfilter.count() && (! debugfilter.exists (who)))
			return;
	}
	
	string what = "%s (%s)" %format (_what, DEBUG.uuid());
	daemon::log (p, who, what);
}

void OpenCoreApp::log (log::priority p, const string &who, const char *fmt, ...)
{
	string what;
	va_list ap;
	va_start (ap, fmt);
	what.printf_va (fmt, &ap);
	va_end (ap);
	
	log (p, who, what);
}

// ==========================================================================
// METHOD OpenCoreApp::getErrors
// ==========================================================================
value *OpenCoreApp::getErrors (void)
{
	returnclass (value) res retain;
	sharedsection (errors)
	{
		res = errors;
	}
	
	return &res;
}

// ==========================================================================
// METHOD OpenCoreApp::regexp
// ==========================================================================
string *OpenCoreApp::regexp (const string &orig)
{
	if (! orig) return NULL;
	returnclass (string) res retain;

	if ((orig[0]=='[') && (orig[1]==':') && (orig.strlen() > 4))
	{
		statstring rid = orig.mid (2, orig.strlen() - 4);
		bool reloadWanted = false;
		value inf = fs.getinfo ("rsrc:regexpclass.rsrc.xml");
		unsigned int mtime = inf["mtime"];
		
		sharedsection (regexpdb)
		{
			if (regexpdb("mtime").uval() != mtime) reloadWanted = true;
			
			if (regexpdb.exists (rid))
			{
				res = strutil::valueparse (regexpdb[rid], regexpdb);
			}
		}
		
		if (reloadWanted) exclusivesection (regexpdb)
		{
			regexpdb.loadxml ("rsrc:regexpclass.rsrc.xml");
			regexpdb("mtime") = mtime;
		}
	}
	else res = orig;
	return &res;
}
