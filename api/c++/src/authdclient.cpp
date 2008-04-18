#include "../include/authdclient.h"
#include "../../../error.h"

#include <grace/filesystem.h>

//	===========================================================================
//	METHOD: authdclient::setlasterror
//	===========================================================================
void authdclient::setlasterror (string &result)
{
	delete result.cutat (':');
	errorcode 	= result.toint ();
	errortext 	= result.cutafter (':');
}


//	===========================================================================
//	METHOD: authdclient::executecmd
//	===========================================================================
int authdclient::executecmd (const string &command)
{
	string 	result;
	bool   	success 	= false;


	s.puts (command);
	
	// Wait for result
	try
	{
		result = s.gets ();
	}
	catch (exception e)
	{
		syslog (LOG_ERR, "Error talking to authd: %s", e.description);
		result = e.description;
		setlasterror (result);
		return ERR_AUTHD_FAILURE;
	}
	
	// Get Results from string
	success 	= (result[0] == '+');
	
	if (! success)
	{
		string rawcmd;
		rawcmd = command;
		rawcmd.cropat (' ');
		
		syslog (LOG_ERR, "Error on command <%s>: %s", rawcmd.str(),
				result.str());
		setlasterror (result);
		
		return ERR_AUTHD_FAILURE;
	}
	
	return ERR_OK;
}


//	===========================================================================
//	METHOD: authdclient::installfile
//	===========================================================================
int authdclient::installfile (const string &source, const string &dest)
{
	string command;

	command.printf ("installfile \"%S\" \"%S\"\n", source.str(), dest.str());
	
	return executecmd (command);
}


//	===========================================================================
//	METHOD: authdclient::deletefile
//	===========================================================================
int authdclient::deletefile  (const string &conffile)
{
	string command;

	command.printf ("deletefile \"%S\"\n", conffile.str());
	
	return executecmd (command);
}


//	===========================================================================
//	METHOD: authdclient::adduser
//	===========================================================================
int authdclient::adduser (const string &username,
				  		const string &shell, const string &passwd)
{
	string command;

	command.printf ("createuser \"%S\" \"%S\"\n", username.str(), passwd.str());
	
	return executecmd (command);
}
				  

//	===========================================================================
//	METHOD: authdclient::deluser
//	===========================================================================
int authdclient::deluser (const string &username)
{
	string command;

	command.printf ("deleteuser \"%S\"\n", username.str());
	
	return executecmd (command);
}


//	===========================================================================
//	METHOD: authdclient::setusershell
//	===========================================================================
int authdclient::setusershell (const string &username, const string &shell)
{
	string command;

	command.printf ("setusershell \"%S\" \"%S\"\n", username.str(), 
				    shell.str());
			  
	return executecmd (command);
}

//	===========================================================================
//	METHOD: authdclient::setusershell
//	===========================================================================
int authdclient::setuserpass (const string &username, const string &password)
{
	string command;

	command.printf ("setuserpass \"%S\" \"%S\"\n", username.str(), 
				    password.str());
			  
	return executecmd (command);
}


//	===========================================================================
//	METHOD: authdclient::setquota
//	===========================================================================
int authdclient::setquota (const string &username, unsigned int softlimit, 
						 unsigned int hardlimit)
{
	string command;

	command.printf ("setquota \"%S\" %u %u\n", username.str(), 
				    softlimit, hardlimit);
			  
	return executecmd (command);
}


//	===========================================================================
//	METHOD: authdclient::startservice
//	===========================================================================
int authdclient::startservice (const string &servicename)
{
	string command;

	command.printf ("startservice \"%S\"\n", servicename.str());
			  
	return executecmd (command);
}


//	===========================================================================
//	METHOD: authdclient::stopservice
//	===========================================================================
int authdclient::stopservice (const string &servicename)
{
	string command;

	command.printf ("stopservice \"%S\"\n", servicename.str());
			  
	return executecmd (command);
}


//	===========================================================================
//	METHOD: authdclient::reloadservice
//	===========================================================================
int authdclient::reloadservice (const string &servicename)
{	
	string command;

	command.printf ("reloadservice \"%S\"\n", servicename.str());
			  
	return executecmd (command);
}

//	===========================================================================
//	METHOD: authdclient::setserviceonboot
//	===========================================================================
int authdclient::setserviceonboot (const string &servicename, bool enabled)
{
	string command;

	command.printf ("setonboot \"%S\" %d\n", servicename.str(), (int) enabled);
			  
	return executecmd (command);
}


//	===========================================================================
//	METHOD: authdclient::runscript
//	===========================================================================
int authdclient::runscript (const string &scriptname, const value &params)
{
	string command;

	command.printf ("runscript \"%S\"", scriptname.str());
	
	foreach (p, params)
	{
		command.printf (" \"%S\"", p.cval());
	}
	command.strcat ('\n');
			  
	return executecmd (command);
}

int authdclient::runuserscript (const string &scriptname, const value &params,
								const string &username)
{
	string cmd;
	
	cmd.printf ("runuserscript \"%S\" \"%S\"", username.str(),
				scriptname.str());
	foreach (p, params)
	{
		cmd.printf (" \"%S\"", p.cval());
	}
	cmd.strcat ('\n');
	
	fs.save ("/tmp/runuserscript-cmd", cmd);
	
	return executecmd (cmd);
}

//	===========================================================================
//	METHOD: authdclient::getfiledata
//	===========================================================================
string *authdclient::getfiledata (const string &filename)
{
	string 	result;
	bool   	success 	= false;
	string  command;
	string	size;
	string 	*ret = new string;

	command.printf ("getobject %s\n", filename.str());

	s.puts (command);
	
	// Wait for result
	result = s.gets ();
	
	// Get Results from string
	success 	= (result[0] == '+');
	if (success)
	{	
		size		= result.cutat (' ');	
		(*ret) = s.read (result.toint(), 10);	
	}
		
	return ret;
}

int authdclient::osupdate (void)
{
	string command;
	command.printf ("osupdate\n");
	return executecmd (command);
}
//	===========================================================================
//	METHOD: authdclient::makedir
//	===========================================================================
int	authdclient::makedir (const string &dirname)
{
	string command;

	command.printf ("makedir \"%S\"\n", dirname.str());
			  
	return executecmd (command);
}

//	===========================================================================
//	METHOD: authdclient::deletedir
//	===========================================================================
int	authdclient::deletedir (const string &dirname)
{
	string command;

	command.printf ("deletedir \"%S\"\n", dirname.str());
			  
	return executecmd (command);
}



//	===========================================================================
//	METHOD: authdclient::makeuserdir
//	===========================================================================
int	authdclient::makeuserdir (const string &user, const string &mode, 
							  const string &dirname)
{
	string command;

	command.printf ("makeuserdir %S %S \"%S\"\n", user.str(), mode.str(),
					dirname.str());
			  
	return executecmd (command);
}


//	===========================================================================
//	METHOD: authdclient::deleteuserdir
//	===========================================================================
int	authdclient::deleteuserdir (const string &user, const string &dirname)
{
	string command;

	command.printf ("deleteuserdir %S \"%S\"\n", user.str(), dirname.str());
			  
	return executecmd (command);
}


//	===========================================================================
//	METHOD: authdclient::rollback
//	===========================================================================
int authdclient::authdclient::rollback (void)
{
	string command;

	command.printf ("rollback\n");
			  
	return executecmd (command);
}


//	===========================================================================
//	METHOD: authdclient::quit
//	===========================================================================
int authdclient::authdclient::quit (void)
{
	string command;

	command.printf ("quit\n");
			  
	executecmd (command);

	return ERR_OK;
}
