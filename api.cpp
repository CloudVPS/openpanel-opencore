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

#include "api.h"
#include "opencore.h"
#include "debug.h"
#include "error.h"

// ==========================================================================
// METHOD API::execute
// ==========================================================================
int API::execute (const string &mname, const statstring &apitype,
				  const string &path, const string &cmd, const value &in,
				  value &out)
{
	string fullcmd;
	fullcmd = "%s/%s" %format (path, cmd);
	
	if (! fs.exists (fullcmd))
	{
		log::write (log::error, "API", "Error executing '%S': not found"
				    %format (fullcmd));
		return status_failed;
	}
	
	caseselector (apitype)
	{
		incaseof ("commandline") : return commandline (mname, fullcmd, in, out);
		incaseof ("cgi") : return cgi (mname, fullcmd, in, out);
		incaseof ("grace") : return grace (mname, fullcmd, in, out);
		incaseof ("xml") : return grace (mname, fullcmd, in, out);
		defaultcase : return status_failed;
	}
}

// ==========================================================================
// METHOD API::makeShellEnvironment
// ==========================================================================
void API::makeShellEnvironment (value &env, const string &pfx, const value &src)
{
	value subst = $("_", "_usc_") ->
				  $(".", "_dot_") ->
				  $(":", "_sub_") ->
				  $("/", "_sla_") ->
				  $(" ", "_spc_") ->
				  $("'", "_fqu_") ->
				  $("`", "_bqu_") ->
				  $("\"", "_dqu_") ->
				  $("=", "_equ_") ->
				  $("+", "_plu_") ->
				  $("-", "_min_");

	foreach (node, src)
	{
		string prefix = pfx;
		string label = node.id().sval();
		label.replace (subst);
		
		if (! prefix.strlen())
		{
			prefix = "CV_";
		}
		else
		{
			prefix.strcat ("_val_");
		}
		
		prefix.strcat (label);
			
		if (node.count())
		{
			makeShellEnvironment (env, prefix, node);
		}
		else
		{
			env[prefix] = node.sval();
		}
	}
}

// ==========================================================================
// METHOD API::commandline (defunct)
// ==========================================================================
int API::commandline (const string &mname, const string &fullcmd,
					  const value &in, value &out)
{
	int i,j;
	value argv;
	value env;
	value rset;
	rset["_"] = "__";
	rset[":"] = "_sub_";
	
	argv.newval() = fullcmd;
	env = $("COMMAND",   in["OpenCORE:Command"]) ->
		  $("SESSIONID", in["OpenCORE:Session"]["sessionid"]) ->
		  $("CLASSID",   in["OpenCORE:Session"]["classid"]) ->
		  $("OBJECTID",  in["OpenCORE:Session"]["objectid"]) ->
		  $("PATH",      "/bin:/usr/bin:/sbin:/usr/sbin:/usr/local/bin:"
		  			     "/usr/local/sbin:/var/opencore/tools");

	makeShellEnvironment (env, "", in);
	
	DEBUG.storeFile ("API", "env", env, "commandline");
	tcpsocket s;
	
	pid_t t = kernel.proc.self();
	systemprocess proc (argv, env, false);
	
	// Connect to authd in child context.
	if (t != kernel.proc.self())
	{
		if (! connectToAuthDaemon (s, mname))
			exit (187);
	}
	
	proc.run();
	string blk;
	string dt;
	
	try
	{
		while (! proc.eof())
		{
			blk = proc.read (4096);
			if (blk.strlen()) dt.strcat (blk);
		}
	}
	catch (...)
	{
	}
	
	DEBUG.storeFile ("API", "result", dt, "commandline");
	
	if (dt.strlen())
	{
		out.fromxml (dt);
	}
	
	if (! out.count())
	{
		out = $("OpenCORE:Result",
					$("error", ERR_API) ->
					$("message", dt));
	}
	
	proc.serialize();
	i = proc.retval();
	
	if (i == 187)
	{
		log::write (log::critical, "API", "Error connecting to authd");
	}
	else
	{
		log::write (log::debug, "API", "Return-value: %i" %format (i));
	}
	return i;
}

// ==========================================================================
// METHOD API::connectToAuthDaemon
// ==========================================================================
bool API::connectToAuthDaemon (tcpsocket &s, const string &mname)
{
	if (! s.uconnect ("/var/opencore/sockets/authd/authd.sock"))
	{
		return false;
	}
	
	value pw;
	pw = kernel.userdb.getpwnam ("opencore");
	
	gid_t gid = pw["gid"].uval();
	uid_t uid = pw["uid"].uval();

	::setregid (gid, gid);
	::setreuid (uid, uid);

	try
	{
		s.printf ("hello %s\n", mname.str());
		string line;
		line = s.gets();
		if (line[0] != '+')
		{
			s.close();
			return false;
		}
		return true;
	}
	catch (...)
	{
	}
	
	s.close ();
	return false;
}

// ==========================================================================
// METHOD API::cgi (defunct)
// ==========================================================================
int API::cgi (const string &mname, const string &fullcmd, const value &in, value &out)
{
	value argv;
	value env;
	
	env["REQUEST_METHOD"] = "POST";
	
	return 1;
}

// ==========================================================================
// METHOD API::grace
// ==========================================================================
int API::grace (const string &mname, const string &fullcmd, const value &in, value &out)
{
	value argv;
	string outdat;
	
	DEBUG.storeFile ("API", "parm", in, "grace");
	
	outdat = in.toxml();
	
	argv.newval() = fullcmd;
	tcpsocket s;
	
	// Note that systemprocess' constructor already does the fork(),
	// allowing us to interject by connecting with authd and resetting
	// group permissions before running the script. To find out in which
	// branch of the fork we are during this window of opportunity, we
	// need to compare the current pid to the one known to be that of the
	// parent process.
	pid_t t = kernel.proc.self();
	systemprocess proc (argv, false);
	if (t != kernel.proc.self())
	{
		if (! connectToAuthDaemon (s, mname))
			exit (1);
	}
	
	// Start the actual script.
	proc.run();
	
	// Send length header and data.
	proc.printf ("%i\n", outdat.strlen());
	proc.puts (outdat);
	
	string blk;
	string dt;
	size_t expectedsize;
	
	try
	{
		// Get a length header.
		blk = proc.gets();
		expectedsize = blk.toint();
		
		log::write (log::debug, "API", "Expecting %i bytes" %format((int)expectedsize));
		
		while (true)
		{
			blk = proc.read (expectedsize - dt.strlen());
			if (blk.strlen()) dt.strcat (blk);
			log::write (log::debug, "API", "Read %i bytes" %format (dt.strlen()));
			
			if (expectedsize <= dt.strlen())
			{
				proc.close();
				break;
			}
		}
		log::write (log::debug, "API", "proc.eof");
	}
	catch (exception e)
	{
		log::write (log::debug, "API", "Process exception: %s"
					%format (e.description));
	}
	
	proc.serialize();

	DEBUG.storeFile ("API", "result", dt, "grace");
	
	if (dt.strlen())
	{
		// Give a warning moan on size mismatches.
		if (dt.strlen() != expectedsize)
		{
			log::write (log::error, "API", "Call to Grace-API module "
					    "script <%s> return data size mismatch" %format (fullcmd));
		}
		
		// Decode the xml in any case.
		out.fromxml (dt);
	}
	
	// Look for a result block.
	if (! out.exists ("OpenCORE:Result"))
	{
		// Not found, bitch & whine.
		log::write (log::error, "API", "Call to Grace-API module "
				    "with no result-block");
		
		DEBUG.storeFile ("API","no-result",out,"grace");
		return 1;
	}
	
	// No error? Fine!
	if (out["OpenCORE:Result"]["error"] == 0) return 0;
	
	// Report the error.
	string errmsg = out["OpenCORE:Result"]["message"];
	errmsg = strutil::regexp (errmsg, "s/\n/ -- /g");
	
	log::write (log::error, "API", "Module error %i from module %s: %s"
				%format (out["OpenCORE:Result"]["error"],mname,errmsg));
	return 1;
}

// ==========================================================================
// METHOD API::flatten
// ==========================================================================
value *API::flatten (const value &tree)
{
	returnclass (value) res retain;
	
	foreach (node, tree)
	{
		if (node.count())
		{
			string path;
			path.printf ("%s", node.id().str());
			flatten_r (node, res, path);
		}
		else
		{
			res[node.id()] = node;
		}
	}
	return &res;
}

// ==========================================================================
// METHOD API::flatten_r
// ==========================================================================
void API::flatten_r (const value &tree, value &into, const string &path)
{
	int count = 0;
	foreach (node, tree)
	{
		string nkey;
		if (node.id())
		{
			nkey.printf ("%s[%s]", path.str(), node.id().str());
		}
		else
		{
			nkey.printf ("%s[%i]", path.str(), count);
		}
		
		if (node.count())
		{
			flatten_r (node, into, nkey);
		}
		else
		{
			into[nkey] = node;
		}
		
		++count;
	}
}
