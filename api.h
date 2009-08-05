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

#ifndef _API_H
#define _API_H 1

#include <grace/value.h>
#include <grace/process.h>
#include <grace/tcpsocket.h>

//  -------------------------------------------------------------------------
/// Static-only class implements the different API methods to call
/// an OpenCORE module.
//  -------------------------------------------------------------------------
class API
{
public:
	/// Primary method. Will call the proper static API method depending
	/// on the apitype, which is currently one of:
	/// - commandline
	/// - cgi
	/// Obviously this list will be extended.
	/// \param mname The module name.
	/// \param apitype The selected API to use.
	/// \param path The path to a module.
	/// \param cmd The command to execute.
	/// \param in Values to pass to the module.
	/// \param out Data returned from the module.
	static int execute (const string &mname,
						const statstring &apitype,
						const string &path,
						const string &cmd,
						const value &in,
						value &out);
	
	/// Implements the commandline API, where session data is
	/// communicated through command line arguments.
	static int commandline (const string &mname, const string &cmd, const value &in, value &out);
	
	/// Implements the CGI API.
	static int cgi (const string &mname, const string &cmd, const value &in, value &out);
	
	/// Implements the Grace-XML API
	static int grace (const string &nmame, const string &cmd, const value &in, value &out);
	
	/// Turn a tree into a flat namespace by transcribing variable names
	/// using array-notation, i.e. foo[bar];
	static value *flatten (const value &tree);
	
	/// Recursion for the flatten method.
	static void flatten_r (const value &tree, value &into, const string &pfx);
	
	/// Make a connection to the authdaemon.
	/// \param s (inout) The socket to connect.
	/// \param err (out) Error string if an error occurs.
	/// \return true if the connection succeeded.
	static bool connectToAuthDaemon (tcpsocket &s, const string &err);
	
	static void makeShellEnvironment (value &, const string &, const value &);
};

#endif
