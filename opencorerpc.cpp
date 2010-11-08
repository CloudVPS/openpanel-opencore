// This file is part of OpenPanel - The Open Source Control Panel
// The Grace library is free software: you can redistribute it and/or modify it 
// under the terms of the GNU General Public License as published by the Free 
// Software Foundation, either version 3 of the License.
//
// Please note that use of the OpenPanel trademark may be subject to additional 
// restrictions. For more information, please visit the Legal Information 
// section of the OpenPanel website on http://www.openpanel.com/

// Class header
#include "opencorerpc.h"

// Application
#include "opencore.h"
#include "session.h"

// Http handler object's
#include "rpcrequesthandler.h"

#include <grace/filesystem.h>

//	=========================================================================
///	Constructor
//	=========================================================================
OpenCoreRPC::OpenCoreRPC (SessionDB *sdb, OpenCoreApp *papp)
{
	pdb	= sdb;	///< Session db pointer
	app	= papp;	///< Application pointer
}
	
	
//	=========================================================================
///	Destructor
/// Send a gracefull shutdown to all server objects
//	=========================================================================
OpenCoreRPC::~OpenCoreRPC (void)
{
	// Finish all activities and stop accepting 
	// new connections.
	httpdUds.shutdown();
}


//	=========================================================================
///	Check given configuration
/// \param conf Object configuration
/// \return true on ok, false on fail
//	=========================================================================
bool OpenCoreRPC::confcheck  (const value &conf)
{
	string 	fName;
	string	fBase;
	
	// Flush the error string
	error.flush();

	// Check min max values and if the socket file exists
	if( conf["unixsocket"]["minthreads"].ival() == 0 ||
		conf["unixsocket"]["maxthreads"].ival() == 0 ||
		(conf["unixsocket"]["minthreads"].ival() > 
		 conf["unixsocket"]["maxthreads"].ival()))
	{
		error = "Invalid number of threads assigned in rpc::unixsocket";
		return false;
	}
	
	// Check if we can create the given unix socket
	fName = PATH_RPCSOCK;
	fBase = fName.cutatlast ('/');
	
	// Check if the base path to the given socket exists
	if (! fs.exists (fBase))
	{
		 error = "Path to socket does not exists %s" %format (fBase);
		 return 0;
	}
	
	// Try to remove the sock file if exists
	if (fs.exists (PATH_RPCSOCK))
		fs.rm (PATH_RPCSOCK);
	
	// Check min max values and if there is a port number given
	if( conf["httpsocket"]["minthreads"].ival() == 0 ||
		conf["httpsocket"]["maxthreads"].ival() == 0 ||
		(conf["httpsocket"]["minthreads"].ival() > 
		 conf["httpsocket"]["maxthreads"].ival()))
	{
		error = "Invalid number of threads assigned in rpc::httpsocket";
		return false;
	}
	
	// Conifg seems to be ok
	return true;
}


//	=========================================================================
///	Configure server objects with given config
/// \param conf Object configuration
/// \return true on ok, false on fail
//	=========================================================================
bool OpenCoreRPC::_confcreate (const value &conf, int update)
{
	// Try setting up the httpd daemon using a
	// Unix Domain Socket
	try
	{	
		// Set up the Http Daemon with a unxi domain socket
		httpdUds.listento (PATH_RPCSOCK);
		httpdUds.minthreads (conf["unixsocket"]["minthreads"].ival());
		httpdUds.maxthreads (conf["unixsocket"]["maxthreads"].ival());

		CORE->log (log::info, "RPC", "Setting up unix-rpc at <%s>",
				   PATH_RPCSOCK);
		
		// Initiate a new Unix domain socket handler
		if (! update)
			_huds = new RPCRequestHandler (app, httpdUds, pdb);
			
		// Start the server
		httpdUds.start();
		fs.chmod (PATH_RPCSOCK, 0666);
		
		ipaddress localhost = "127.0.0.1";
		httpdTcp.listento (localhost, 4088);
		CORE->log (log::info, "RPC", "Setting up tcp-rpc on 127.0.0.1:4088");
		
		if (! update)
		{
			_htcp = new RPCRequestHandler (app, httpdTcp, pdb);
			httpdTcp.systempath ("/var/openpanel");
			new httpdlogger (httpdTcp, "/var/opencore/log/opencore.access.log");
			new IconRequestHandler (app, httpdTcp);
			new ItemIconRequestHandler (app, httpdTcp);
			new EmblemRequestHandler (app, httpdTcp);
			new ImagePreloader (app, httpdTcp);
			new LandingPageHandler (app, httpdTcp);
			new httpdfileshare (httpdTcp, "*", "/var/openpanel/http");
		}
		
		httpdTcp.start();
	}
	catch (exception e)
	{
		CORE->log (log::critical, "RPC", "Exception setting up unix-rpc: %s",
				   e.description);
		error = "Fatal exception during setup of UnixDomainSocket";
			
		return false;	
	}
	
	// Everything was OK!		
	return true;
}

//	=========================================================================
///	Update running configuration
/// \param conf Object configuration
/// \return true on ok, false on fail
//	=========================================================================
bool OpenCoreRPC::confupdate (const value &conf)
{
	try
	{	
		// Do a graceful shutdown of the http daemons
		httpdUds.shutdown();
		httpdTcp.shutdown();
		
		// Restart deamons, if failes there should alread been
		// set an error string
		if(! _confcreate (conf, true)) return false;
	}
	catch (exception e)
	{	
		// Still there could have been trown an 
		// exeption outside the confcreate
		error = "Fatal exception during re-initialization: ";
		error.strcat (e.description);
		return false;
	}

	return true;
}

