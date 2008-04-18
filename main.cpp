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

#include "opencore.h"
#include "version.h"
#include "debug.h"
#include "alerts.h"

#include <grace/thread.h>

APPOBJECT(opencoreApp);

opencoreApp *CORE = NULL;
bool APP_SHOULDRUN;

// ==========================================================================
// CONSTRUCTOR opencoreApp
// ==========================================================================
opencoreApp::opencoreApp (void)
	: daemon ("com.openpanel.svc.opencore"),
	  conf (this),
	  shell (this, fin, fout)
{
	CORE = this;
}

// ==========================================================================
// DESTRUCTOR opencoreApp
// ==========================================================================
opencoreApp::~opencoreApp (void)
{
}

#define PRINTERR(x) ferr.printf (x ": (%04i) %s\n", \
				s->errorcount() ? s->error(-1)["code"].ival() : 0, \
				s->errorcount() ? s->error(-1)["message"].cval() : \
				"Unknown error")

// Import from debug.cpp
extern bool DISABLE_DEBUGGING;

// ==========================================================================
// METHOD opencoreApp::main
// ==========================================================================
int opencoreApp::main (void)
{
	
	if (argv.exists ("--emit-dancing-bears"))
	{
		ferr.printf ("    _--_     _--_    _--_     _--_     _--_     _--_     _--_     _--_\n");
		ferr.printf ("   (    )~~~(    )  (    )~~~(    )   (    )~~~(    )   (    )~~~(    )\n");
		ferr.printf ("    \\           /    \\           /     \\           /     \\           /\n");
		ferr.printf ("     (  ' _ `  )      (  ' _ `  )       (  ' _ `  )       (  ' _ `  )\n");
		ferr.printf ("      \\       /        \\       /         \\       /         \\       /\n");
		ferr.printf ("    .__( `-' )          ( `-' )           ( `-' )        .__( `-' )  ___\n");
		ferr.printf ("   / !  `---' \\      _--'`---_          .--`---'\\       /   /`---'`-'   \\\n");
		ferr.printf ("  /  \\         !    /         \\___     /        _>\\    /   /          ._/   __\n");
		ferr.printf (" !   /\\        )   /   /       !  \\   /  /-___-'   ) /'   /.-----\\___/     /  )\n");
		ferr.printf (" !   !_\\       ). (   <        !__/ /'  (        _/  \\___//          `----'   !\n");
		ferr.printf ("  \\    \\       ! \\ \\   \\      /\\    \\___/`------' )       \\            ______/\n");
		ferr.printf ("   \\___/   )  /__/  \\--/   \\ /  \\  ._    \\      `<         `--_____----'\n");
		ferr.printf ("     \\    /   !       `.    )-   \\/  ) ___>-_     \\   /-\\    \\    /\n");
		ferr.printf ("     /   !   /         !   !  `.    / /      `-_   `-/  /    !   !\n");
		ferr.printf ("    !   /__ /___       /  /__   \\__/ (  \\---__/ `-_    /     /  /__\n");
		ferr.printf ("    (______)____)     (______)        \\__)         `-_/     (______)\n");
	}
	
	string conferr; // Error return from configuration class.
	bool isdebugging = argv.exists ("--debugging-console");
	bool nodebugging = false;
	
	if (isdebugging)
	{
		setforeground(); // No daemonizing to background for now.
	}
	else if (argv.exists ("--disable-debugging"))
	{
		DISABLE_DEBUGGING = true;
	}
	
	if (argv.exists ("--debug"))
	{
		value tval = strutil::split (argv["--debug"], ',');
		foreach (v, tval) debugfilter[v] = true;
		DEBUG.setfilter (debugfilter);
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
	pw = kernel.userdb.getpwnam ("opencore");
	if (! pw)
	{
		ferr.printf ("Could not find opencore user\n");
		return false;
	}
	uid_opencore = pw["uid"].uval();
	gid_opencore = pw["gid"].uval();
	
	// Find the authd group
	gw = kernel.userdb.getgrnam ("authd");
	if (! gw)
	{
		ferr.printf ("Could not find authd group\n");
		return false;
	}
	gid_authd = gw["gid"].uval();
	
	// Fiddle the credentials we should get.
	settargetgid (gid_opencore, gid_authd);
	settargetuid (uid_opencore);

	// Set up collection objects.
	mdb = new moduledb;
	sdb = new sessiondb (*mdb);
	rpc = NULL; // will be initialized by confRpc.
		
	// Add watcher value for event log. System will daemonize after
	// configuration was validated.
	conf.addwatcher ("system", &opencoreApp::confSystem);
	
	// Add confwatcher for the daemons RPC configuration
	conf.addwatcher ("rpc", &opencoreApp::confRpc);
	
	// Load will fail if watchers did not valiate.
	if (! conf.load ("com.openpanel.svc.opencore", conferr))
	{
		ferr.printf ("%% Error loading configuration: %s\n", conferr.str());
		return 1;
	}

	log (log::info, "main", "Application starting");
	
	exclusivesection (regexpdb)
	{
		regexpdb.loadxml ("rsrc:regexpclass.rsrc.xml");
	}

	// Set up alert and session expire threads.
	ALERT = new alerthandler (conf["alert"]);
	sexp = new sessionexpire (sdb);

	// Get the list of modules that should be reinitialized through their
	// getconfig.
	value initlist;
	if (argv.exists ("--reinitialize-module-config"))
	{
		initlist = strutil::split (argv["--reinitialize-module-config"],',');
	}

	// Load modules.
	mdb->init (initlist);
	
	if (! isdebugging)
	{
		log (log::debug, "main", "Detaching parent");
		delayedexitok ();
	}
	log (log::info, "main", "All subsystems initialized");
	
	if (argv.exists ("--debugging-console"))
	{
		commandline (); // This will go :)
	}
	else
	{
		APP_SHOULDRUN = true;
		while (APP_SHOULDRUN)
		{
			value ev = waitevent ();
			if (ev.type() == "shutdown") APP_SHOULDRUN = false;
		}
	}
	
	log (log::info, "main", "Shutting down");

	sexp->shutdown();
	ALERT->shutdown();
	stoplog();
	return 0;
}

// ==========================================================================
// METHOD opencoreApp::checkAuthDaemon
// ==========================================================================
bool opencoreApp::checkAuthDaemon (void)
{
	tcpsocket sauth;
	
	if (! sauth.uconnect ("/var/opencore/sockets/authd/authd.sock"))
	{
		ferr.printf ("%% Error connecting to authd socket\n");
		return false;
	}
	
	try
	{
		sauth.writeln ("hello opencore");
		string line;
		line = sauth.gets();
		if (! line.strlen())
		{
			ferr.printf ("%% Error getting reply from authd socket\n");
			return false;
		}
		if (line[0] != '+')
		{
			ferr.printf ("%% Error from authd: %s\n", line.str());
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
// METHOD opencoreApp::confSystem
// ==========================================================================
bool opencoreApp::confSystem (config::action act, keypath &kp,
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
				ferr.printf ("%% Event log path %s does not exist\n",
							 tstr.str());
				return false;
			}
			if (! nval.exists ("debuglog")) return true;
			tstr = strutil::makepath (nval["debuglog"].sval());
			if (! tstr.strlen()) return true;
			if (! fs.exists (tstr))
			{
				ferr.printf ("%% Debug log path %s does not exist\n",
							 tstr.str());
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
// METHOD opencoreApp::confRpc
// ==========================================================================
bool opencoreApp::confRpc (config::action act, keypath &kp,
						   const value &nval, const value &oval)
{
	if (! rpc) rpc = new opencorerpc (sdb, this);

	switch (act)
	{
		// Validate given configuration
		case config::isvalid:
			if (! rpc->confcheck (nval))
			{
				log (log::error, "openrpc ", "RPC config error: %s",
					 rpc->error.cval());
				return false;
			}
			return true;

			
		// Configure instances, called at startup
		case config::create:
			if (! rpc->confcreate (nval))
			{
				log (log::critical, "openrpc ", "RPC setup failed: %s",
					 rpc->error.cval());
				return false;
			}
			return true;

			
		// Update configuration, alled at runtime
		case config::change:
			if (! rpc->confupdate (nval))
			{
				log (log::error, "openrpc ", "RPC reconfig failed: %s",
					 rpc->error.cval());
				return false;
			}
			return true;
	}
	
	return false;
}

// ==========================================================================
// METHOD opencoreApp::commandline
// ==========================================================================
void opencoreApp::commandline (void)
{
	bool shouldrun = true;
	
	shell.addsrc    ("@sessionid", &opencoreApp::srcSessionId);
	
	shell.addsyntax ("show classes", &opencoreApp::cmdShowClasses);
	shell.addsyntax ("show session", &opencoreApp::cmdShowSessions);
	shell.addsyntax ("show session @sessionid", &opencoreApp::cmdShowSession);
	
	shell.addsyntax ("show threads", &opencoreApp::cmdShowThreads);
	shell.addsyntax ("show version", &opencoreApp::cmdShowVersion);
	shell.addsyntax ("exit", &opencoreApp::cmdExit);
	
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
// METHOD opencoreApp::cmdShowSession
// ==========================================================================
int opencoreApp::cmdShowSession (const value &cmdata)
{
	if (cmdata.count() != 3)
	{
		ferr.printf ("%% Wuh?\n");
		return 0;
	}
	
	coresession *s;
	statstring sid;
	
	sid = cmdata[2].sval();
	
	s = sdb->get (sid);
	if (! s)
	{
		ferr.printf ("%% Unknown session\n");
		return 0;
	}
	
	string out;
	value dat;
	
	dat["OpenCORE:Session"]["id"] = sid.sval();
	
	out = dat.encode();
	fout.puts (out);
	
	sdb->release (s);
	return 0;
}

// ==========================================================================
// METHOD opencoreApp::srcSessionId
// ==========================================================================
value *opencoreApp::srcSessionId (const value &cmd, int p)
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
// METHOD opencoreApp::cmdShowSessions
// ==========================================================================
int opencoreApp::cmdShowSessions (const value &cmdata)
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
		fout.printf ("\033[0m[%s]\n", org.str());
		
		fout.printf ("   %s\n", row.id().str());
	}
	return 0;
}

// ==========================================================================
// METHOD opencoreApp::cmdShowVersion
// ==========================================================================
int opencoreApp::cmdShowVersion (const value &cmdata)
{
	fout.printf ("OpenCORE version %s - %s (%s@%s)\n",
				 version::release.str(),
				 version::date.str(),
				 version::user.str(),
				 version::hostname.str());
				 
	fout.printf ("Available under the GNU General Public License\n");
	fout.printf ("Copyright (C) 2007 PanelSix\n");
	return 0;
}

// ==========================================================================
// METHOD opencoreApp::cmdShowThreads
// ==========================================================================
int opencoreApp::cmdShowThreads (const value &cmdata)
{
	value v;
	string out;
	v = thread::getlist ();
	out = v.encode();
	fout.writeln (out);
	return 0;
}

// ==========================================================================
// METHOD opencoreApp::cmdShowClasses
// ==========================================================================
int opencoreApp::cmdShowClasses (const value &cmdata)
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
// METHOD openoreApp::logerror
// ==========================================================================
void opencoreApp::logerror (const string &who, const string &what)
{
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
// METHOD opencoreApp::log
// ==========================================================================
void opencoreApp::log (log::priority p, const string &who, const string &what)
{
	if (p == log::error)
	{
		logerror (who, what);
		return;
	}
	
	if (p == log::debug)
	{
		if (debugfilter.count() && (! debugfilter.exists (who)))
			return;
		
		if (DISABLE_DEBUGGING) return;
	}
	
	daemon::log (p, who, what);
}

void opencoreApp::log (log::priority p, const string &who, const char *fmt, ...)
{
	string what;
	va_list ap;
	va_start (ap, fmt);
	what.printf_va (fmt, &ap);
	va_end (ap);
	
	log (p, who, what);
}

// ==========================================================================
// METHOD opencoreApp::geterrors
// ==========================================================================
value *opencoreApp::geterrors (void)
{
	returnclass (value) res retain;
	sharedsection (errors)
	{
		res = errors;
	}
	
	return &res;
}

// ==========================================================================
// METHOD opencoreApp::regexp
// ==========================================================================
string *opencoreApp::regexp (const string &orig)
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
