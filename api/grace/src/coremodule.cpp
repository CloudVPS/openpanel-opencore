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
	string spath = "/var/openpanel/conf/staging/%s" %format (modid);
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
		sendResult (CoreModule::E_VALIDATION, "No valid XML found");
		return 1;
	}
	
	statstring cmd = env["OpenCORE:Command"];
	statstring classid = env["OpenCORE:Session"]["classid"];
	
	if (! classes.exists (classid))
	{
		sendResult (CoreModule::E_CLASS, "Unknown class");
		return 1;
	}
	
	CoreClass &C = classes[classid];
	C.setParam (env[C.name]);
	C.setEnv (env);
	
	syslog (LOG_INFO, "Running module cmd=<%s>", cmd.str());
	
	caseselector (cmd)
	{
		incaseof ("create") :
			if (! C.create())
			{
				sendError (C.code(), C.error());
				authd.rollback();
				return 1;
			}
			break;
		
		incaseof ("delete") :
			if (! C.remove ())
			{
				sendError (C.code(), C.error());
				authd.rollback();
				return 1;
			}
			break;
		
		incaseof ("update") :
			if (! C.update ())
			{
				sendError (C.code(), C.error());
				authd.rollback();
				return 1;
			}
			break;
		
		incaseof ("getconfig") :
			if (! getConfig (env)) return 1;
			break;
			
		defaultcase :
			sendError (CoreModule::E_NOTIMPL, "Command not implemented");
			return 1;
	}
	
	syslog (LOG_INFO, "Sending authd.quit");
	authd.quit ();
	
	syslog (LOG_INFO, "Returning OK");
	sendOk();
	return 0;
}

// ==========================================================================
// METHOD CoreModule::getconfig
// ==========================================================================
bool CoreModule::getConfig (const value &env)
{
	sendResult (CoreModule::E_NOTIMPL, "Method not implemented");
	return false;
}

// ==========================================================================
// METHOD CoreModule::addClass
// ==========================================================================
void CoreModule::addClass (const statstring &id, CoreClass *who)
{
	classes.set (id, (CoreClass &) *who);
}

// ==========================================================================
// METHOD CoreModule::sendResult
// ==========================================================================
void CoreModule::sendResult (int code, const string &err)
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
// METHOD CoreModule::sendError
// ==========================================================================
void CoreModule::sendError (int code, const string &err)
{
	if (code)
	{
		sendResult (code,err);
	}
	else
	{
		sendResult (E_OTHER, "Undocumented error in module");
	}
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
	MOD->addClass (name, this);
	_code = 0;
	_error = "OK";
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
	MOD->addClass (aliasClass, this);
}

// ==========================================================================
// METHOD CoreClass::Setparam
// ==========================================================================
void CoreClass::setParam (const value &p)
{
	param = p;
	owner = p("owner");
	id = param["id"];
}

// ==========================================================================
// METHOD CoreClass::setEnv
// ==========================================================================
void CoreClass::setEnv (const value &e)
{
	env = e;
	requestedClass = env["OpenCORE:Session"]["classid"];
}

// ==========================================================================
// METHOD CoreClass::create
// ==========================================================================
bool CoreClass::create (void)
{
	error (CoreModule::E_NOTIMPL, "Method not implemented");
	return false;
}

// ==========================================================================
// METHOD CoreClass::remove
// ==========================================================================
bool CoreClass::remove (void)
{
	error (CoreModule::E_NOTIMPL, "Method not implemented");
	return false;
}

// ==========================================================================
// METHOD CoreClass::update
// ==========================================================================
bool CoreClass::update (void)
{
	error (CoreModule::E_NOTIMPL, "Method not implemented");
	return false;
}

// ==========================================================================
// METHOD CoreClass::listAliases
// ==========================================================================
value *CoreClass::listAliases (const value &env)
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
// METHOD CoreClass::listChildren
// ==========================================================================
const value &CoreClass::listChildren (const statstring &ofclass)
{
	return param[ofclass];
}

// ==========================================================================
// METHOD CoreClass::seterror
// ==========================================================================
void CoreClass::error (int c, const string &e)
{
	_code = c;
	_error = e;
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
// METHOD AuthDaemon::installFile
// ==========================================================================
bool AuthDaemon::installFile (const string &fname, const string &path)
{
	return call ("installfile", fname, path);
}

// ==========================================================================
// METHOD AuthDaemon::installUserFile
// ==========================================================================
bool AuthDaemon::installUserFile (const string &fname, const string &path,
								  const string &user)
{
	return call ("installuserfile", fname, path, user);
}

// ==========================================================================
// METHOD AuthDaemon::deleteFile
// ==========================================================================
bool AuthDaemon::deleteFile (const string &fname)
{
	return call ("deletefile", fname);
}

// ==========================================================================
// METHOD AuthDaemon::addUser
// ==========================================================================
bool AuthDaemon::addUser (const string &uname, const string &sh, const string &pw)
{
	return call ("adduser", uname, sh, pw);
}

// ==========================================================================
// METHOD AuthDaemon::deleteuser
// ==========================================================================
bool AuthDaemon::deleteUser (const string &username)
{
	return call ("deleteUser", username);
}

// ==========================================================================
// METHOD AuthDaemon::setUserShell
// ==========================================================================
bool AuthDaemon::setUserShell (const string &uname, const string &sh)
{
	return call ("setusershell", uname, sh);
}

// ==========================================================================
// METHOD AuthDaemon::setUserPass
// ==========================================================================
bool AuthDaemon::setUserPass (const string &uname, const string &pass)
{
	return call ("setuserpass", uname, pass);
}

// ==========================================================================
// METHOD AuthDaemon::setQuota
// ==========================================================================
bool AuthDaemon::setQuota (const string &uname, unsigned int soft, unsigned int hard)
{
	string cmd = "setquota \"%S\" %u %u" %format (uname, soft, hard);
	return call (cmd);
}

// ==========================================================================
// METHOD AuthDaemon::startservice
// ==========================================================================
bool AuthDaemon::startService (const string &svc)
{
	return call ("startservice", svc);
}

// ==========================================================================
// METHOD AuthDaemon::stopservice
// ==========================================================================
bool AuthDaemon::stopService (const string &svc)
{
	return call ("stopservice", svc);
}

// ==========================================================================
// METHOD AuthDaemon::reloadservice
// ==========================================================================
bool AuthDaemon::reloadService (const string &svc)
{
	return call ("reloadservice", svc);
}

// ==========================================================================
// METHOD AuthDaemon::setonboot
// ==========================================================================
bool AuthDaemon::setOnBoot (const string &svc, bool enabled)
{
	string cmd = "setonboot \"%S\" %i" %format (svc, enabled?1:0);
	return call (cmd);
}

// ==========================================================================
// METHOD AuthDaemon::runscript
// ==========================================================================
bool AuthDaemon::runScript (const string &scriptname, const value &param)
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
bool AuthDaemon::runUserScript (const string &scriptname, const value &params,
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
string *AuthDaemon::getObject (const string &filename)
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
bool AuthDaemon::osUpdate (void)
{
	return call ("osupdate");
}

// ==========================================================================
// METHOD AuthDaemon::makedir
// ==========================================================================
bool AuthDaemon::makeDir (const string &dirname)
{
	return call ("makedir", dirname);
}

// ==========================================================================
// METHOD AuthDaemon::deletedir
// ==========================================================================
bool AuthDaemon::deleteDir (const string &dirname)
{
	return call ("deletedir", dirname);
}

// ==========================================================================
// METHOD AuthDaemon::makeuserdir
// ==========================================================================
bool AuthDaemon::makeUserDir (const string &usr, const string &mod,
							  const string &dirname)
{
	return call ("makeuserdir", usr, mod, dirname);
}

// ==========================================================================
// METHOD AuthDaemon::deleteuserdir
// ==========================================================================
bool AuthDaemon::deleteUserDir (const string &user, const string &dirname)
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
