#include <grace-coreapi/module.h>
#include <grace/filesystem.h>

#include <syslog.h>
#include <stdarg.h>

CoreModule *MOD;

// ==========================================================================
// CONSTRUCTOR CoreModule
// ==========================================================================
CoreModule::CoreModule (const string &moduleName)
	: application (moduleName)
{
	string modid = moduleName.copyuntil (".module");
	string spath = "/var/opencore/conf/staging/%s" %format (modid);
	fs.cd (spath);
	chdir (spath);
	
	MOD = this;
	openlog (name().str(), LOG_PID, LOG_DAEMON);
}

// ==========================================================================
// DESTRUCTOR CoreModule
// ==========================================================================
CoreModule::~CoreModule (void)
{
}

// ==========================================================================
// METHOD CoreModule::main
// ==========================================================================
int CoreModule::main (void)
{
	int sz;
	string buf;
	string xml;
	value env;
	
	buf = fin.gets ();
	sz = buf.toint ();
	buf = fin.read (sz);
	
	if (! env.fromxml (buf))
	{
		senderror (CoreModule::E_VALIDATION, "No valid XML found");
		return 1;
	}
	
	statstring cmd = env["OpenCORE:Command"];
	statstring classid = env["OpenCORE:Session"]["classid"];
	
	if (! classes.exists (classid))
	{
		senderror (CoreModule::E_CLASS, "Unknown class");
		return 1;
	}
	
	CoreClass &C = classes[classid];
	C.setparam (env[C.name]);
	
	syslog (LOG_INFO, "Running module cmd=<%s>", cmd.str());
	
	caseselector (cmd)
	{
		incaseof ("create") :
			if (! C.create (env))
			{
				senderror (C.code, C.error);
				return 1;
			}
			break;
		
		incaseof ("delete") :
			if (! C.remove (env))
			{
				senderror (C.code, C.error);
				return 1;
			}
			break;
		
		incaseof ("update") :
			if (! C.update (env))
			{
				senderror (C.code, C.error);
				return 1;
			}
			break;
		
		incaseof ("getconfig") :
			if (! getconfig (env)) return 1;
			break;
			
		defaultcase :
			senderror (CoreModule::E_NOTIMPL, "Command not implemented");
			return 1;
	}
	
	syslog (LOG_INFO, "Sending authd.quit");
	authd.quit ();
	
	syslog (LOG_INFO, "Returning OK");
	senderror (CoreModule::E_OK, "OK");
	return 0;
}

// ==========================================================================
// METHOD CoreModule::getconfig
// ==========================================================================
bool CoreModule::getconfig (const value &env)
{
	senderror (CoreModule::E_NOTIMPL, "Method not implemented");
	return false;
}

// ==========================================================================
// METHOD CoreModule::addclass
// ==========================================================================
void CoreModule::addclass (const statstring &id, CoreClass *who)
{
	classes.set (id, (CoreClass &) *who);
}

// ==========================================================================
// METHOD CoreModule::senderror
// ==========================================================================
void CoreModule::senderror (int code, const string &err)
{
	value out;
	out.type ("opencore.module.%s" %format (creator.copyuntil (".module")));
	out["OpenCORE:Result"]["error"] = code;
	out[-1]["message"] = err;
	
	string outstr = out.toxml ();
	fout.writeln ("%i" %format (outstr.strlen()));
	fout.puts (outstr);
}

// ==========================================================================
// DEFAULT CONSTRUCTOR CoreClass
// ==========================================================================
CoreClass::CoreClass (void)
{
	throw (unoverloadedConstructorException());
}

// ==========================================================================
// CONSTRUCTOR CoreClass
// ==========================================================================
CoreClass::CoreClass (const string &className)
{
	name = className;
	MOD->addclass (name, this);
	code = 0;
	error = "OK";
}

// ==========================================================================
// DESTRUCTOR CoreClass
// ==========================================================================
CoreClass::~CoreClass (void)
{
}

// ==========================================================================
// METHOD CoreClass::alias
// ==========================================================================
void CoreClass::alias (const statstring &aliasClass)
{
	MOD->addclass (aliasClass, this);
}

// ==========================================================================
// METHOD CoreClass::Setparam
// ==========================================================================
void CoreClass::setparam (const value &p)
{
	param = p;
	id = param["id"];
}

// ==========================================================================
// METHOD CoreClass::create
// ==========================================================================
bool CoreClass::create (const value &env)
{
	code = CoreModule::E_NOTIMPL;
	error = "Method not implemented";
	return false;
}

// ==========================================================================
// METHOD CoreClass::remove
// ==========================================================================
bool CoreClass::remove (const value &env)
{
	code = CoreModule::E_NOTIMPL;
	error = "Method not implemented";
	return false;
}

// ==========================================================================
// METHOD CoreClass::update
// ==========================================================================
bool CoreClass::update (const value &env)
{
	code = CoreModule::E_NOTIMPL;
	error = "Method not implemented";
	return false;
}

// ==========================================================================
// METHOD CoreClass::listaliases
// ==========================================================================
value *CoreClass::listaliases (const value &env)
{
	returnclass (value) res retain;
	string realdomain = env["Domain"]["id"];
	
	int offs = id.strstr (realdomain);
	if (offs<0) return &res;
	if ((id.strlen() - offs) != realdomain.strlen()) return &res;
	
	string prefix = id.left (offs);
	
	foreach (node, env["Domain"]["Domain:Alias"])
	{
		string aliasdomain = node.id();
		res.newval() = "%s%s" %format (prefix, aliasdomain);
	}
	
	return &res;
}

// ==========================================================================
// METHOD CoreClass::seterror
// ==========================================================================
void CoreClass::seterror (int c, const string &e)
{
	code = c;
	error = e;
}

// ==========================================================================
// CONSTRUCTOR AuthDaemon
// ==========================================================================
AuthDaemon::AuthDaemon (void)
{
	s.openread (3);
}

// ==========================================================================
// DESTRUCTOR AuthDaemon
// ==========================================================================
AuthDaemon::~AuthDaemon (void)
{
	s.close ();
	closelog ();
}

// ==========================================================================
// METHOD AuthDaemon::installfile
// ==========================================================================
bool AuthDaemon::installfile (const string &fname, const string &path)
{
	return call ("installfile", fname, path);
}

// ==========================================================================
// METHOD AuthDaemon::deletefile
// ==========================================================================
bool AuthDaemon::deletefile (const string &fname)
{
	return call ("deletefile", fname);
}

// ==========================================================================
// METHOD AuthDaemon::adduser
// ==========================================================================
bool AuthDaemon::adduser (const string &uname, const string &sh, const string &pw)
{
	return call ("adduser", uname, sh, pw);
}

// ==========================================================================
// METHOD AuthDaemon::deleteuser
// ==========================================================================
bool AuthDaemon::deleteuser (const string &username)
{
	return call ("deleteuser", username);
}

// ==========================================================================
// METHOD AuthDaemon::setusershell
// ==========================================================================
bool AuthDaemon::setusershell (const string &uname, const string &sh)
{
	return call ("setusershell", uname, sh);
}

// ==========================================================================
// METHOD AuthDaemon::setuserpass
// ==========================================================================
bool AuthDaemon::setuserpass (const string &uname, const string &pass)
{
	return call ("setuserpass", uname, pass);
}

// ==========================================================================
// METHOD AuthDaemon::setquota
// ==========================================================================
bool AuthDaemon::setquota (const string &uname, unsigned int soft, unsigned int hard)
{
	string cmd = "setquota \"%S\" %u %u" %format (uname, soft, hard);
	return call (cmd);
}

// ==========================================================================
// METHOD AuthDaemon::startservice
// ==========================================================================
bool AuthDaemon::startservice (const string &svc)
{
	return call ("startservice", svc);
}

// ==========================================================================
// METHOD AuthDaemon::stopservice
// ==========================================================================
bool AuthDaemon::stopservice (const string &svc)
{
	return call ("stopservice", svc);
}

// ==========================================================================
// METHOD AuthDaemon::reloadservice
// ==========================================================================
bool AuthDaemon::reloadservice (const string &svc)
{
	return call ("reloadservice", svc);
}

// ==========================================================================
// METHOD AuthDaemon::setonboot
// ==========================================================================
bool AuthDaemon::setonboot (const string &svc, bool enabled)
{
	string cmd = "setonboot \"%S\" %i" %format (svc, enabled?1:0);
	return call (cmd);
}

// ==========================================================================
// METHOD AuthDaemon::runscript
// ==========================================================================
bool AuthDaemon::runscript (const string &scriptname, const value &param)
{
	string cmd = "runscript \"%S\"" %format (scriptname);
	
	foreach (p, param)
	{
		cmd.strcat (" \"%S\"" %format (p));
	}
	
	return call (cmd);
}

// ==========================================================================
// METHOD AuthDaemon::runuserscript
// ==========================================================================
bool AuthDaemon::runuserscript (const string &scriptname, const value &params,
								const string &username)
{
	string cmd = "runuserscript \"%S\" \"%S\"" %format (scriptname, username);
	foreach (p, params)
	{
		cmd.strcat (" \"%S\"" %format (p));
	}
	
	return call (cmd);	
}

// ==========================================================================
// METHOD AuthDaemon::getobject
// ==========================================================================
string *AuthDaemon::getobject (const string &filename)
{
	returnclass (string) res retain;
	
	string line, stmp;
	string cmd = "getobject \"%S\"" %format (filename);
	
	line = s.gets ();
	if (line[0] != '+') return &res;
	stmp = line.cutat (' ');
	res = s.read (line.toint(), 10);
	return &res;
}

// ==========================================================================
// METHOD AuthDaemon::osupdate
// ==========================================================================
bool AuthDaemon::osupdate (void)
{
	return call ("osupdate");
}

// ==========================================================================
// METHOD AuthDaemon::makedir
// ==========================================================================
bool AuthDaemon::makedir (const string &dirname)
{
	return call ("makedir", dirname);
}

// ==========================================================================
// METHOD AuthDaemon::deletedir
// ==========================================================================
bool AuthDaemon::deletedir (const string &dirname)
{
	return call ("deletedir", dirname);
}

// ==========================================================================
// METHOD AuthDaemon::makeuserdir
// ==========================================================================
bool AuthDaemon::makeuserdir (const string &usr, const string &mod,
							  const string &dirname)
{
	return call ("makeuserdir", usr, mod, dirname);
}

// ==========================================================================
// METHOD AuthDaemon::deleteuserdir
// ==========================================================================
bool AuthDaemon::deleteuserdir (const string &user, const string &dirname)
{
	return call ("deleteuserdir", user, dirname);
}

// ==========================================================================
// METHOD AuthDaemon::rollback
// ==========================================================================
bool AuthDaemon::rollback (void)
{
	return call ("rollback");
}

// ==========================================================================
// METHOD AuthDaemon::quit
// ==========================================================================
void AuthDaemon::quit (void)
{
	call ("quit");
	s.close ();
}

// ==========================================================================
// METHOD AuthDaemon::call
// ==========================================================================
bool AuthDaemon::call (const string &cmd)
{
	string line;
	s.writeln (cmd);
	line = s.gets ();
	if (line[0] != '+')
	{
		error = line.mid (1);
		return false;
	}
	return true;
}

bool AuthDaemon::call (const string &cmd, const string &p1,
					   const string &p2, const string &p3)
{
	string c = "%s \"%S\" \"%S\" \"%S\"" %format (cmd, p1, p2, p3);
	return call (c);
}

bool AuthDaemon::call (const string &cmd, const string &p1,
					   const string &p2)
{
	string c = "%s \"%S\" \"%S\"" %format (cmd, p1, p2);
	return call (c);
}

bool AuthDaemon::call (const string &cmd, const string &p1)
{
	string c = "%s \"%S\"" %format (cmd, p1);
	return call (c);
}

AuthDaemon authd;
