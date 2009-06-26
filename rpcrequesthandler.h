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

#ifndef _OPENCORE_RPC_UDS
#define _OPENCORE_RPC_UDS 1

#include <grace/httpd.h>
#include <grace/value.h>
#include <grace/str.h>
#include <grace/httpdefs.h>


//  -------------------------------------------------------------------------
/// Handler for http-rpc requests coming in over the unix domain socket.
//  -------------------------------------------------------------------------
class rpcrequesthandler : public httpdobject
{
public:

			 /// Constructor
			 /// \param papp Used for applications logging functionality
			 /// \param server The http server object
			 /// \param db Link to the sessiondb.
			 /// Note, this http object does listen to all uri's by default
			 rpcrequesthandler (class opencoreApp *papp, 
						 httpd &server, 
						 class sessiondb *db);
			 
			 /// Destructor
			~rpcrequesthandler (void) {};


			 /// Httpd Run, This function is called when receiving
			 /// http requests
			 /// \param uri The requested uri.
			 /// \param postbody All posted data.
			 /// \param inhdr Received headers.
			 /// \param out Storage for reply data.
			 /// \param outhdr Storage for outbound headers.
			 /// \param env Webserver environment.
			 /// \param s The raw socket.
	int      run (string &uri, string &postbody, value &inhdr,
				  string &out, value &outhdr, value &env,
				  tcpsocket &s);
				  
private:                      
	class opencoreApp	*app; ///< Link back to application object.
	class sessiondb		*sdb; ///< Link to session database.
};

//  -------------------------------------------------------------------------
/// Handler for http requests in the /icon/ directory. Loads an
/// icon from outside the module directory by class uuid.
//  -------------------------------------------------------------------------
class iconrequesthandler : public httpdobject
{
public:
			 iconrequesthandler (class opencoreApp *papp,
			 					 httpd &server);
			 
			~iconrequesthandler (void) {}
			
	int      run (string &uri, string &postbody, value &inhdr,
				  string &out, value &outhdr, value &env,
				  tcpsocket &s);
				  
private:                      
	class opencoreApp	*app; ///< Link back to application object.
};

//  -------------------------------------------------------------------------
/// Handler for http requests in the /itemicon/ directory. Loads an
/// icon from outside the module directory by class uuid.
//  -------------------------------------------------------------------------
class itemiconrequesthandler : public httpdobject
{
public:
			 itemiconrequesthandler (class opencoreApp *papp,
									 httpd &server);
			~itemiconrequesthandler (void) {}
	int      run (string &uri, string &postbody, value &inhdr,
				  string &out, value &outhdr, value &env,
				  tcpsocket &s);
				  
private:                      
	class opencoreApp	*app; ///< Link back to application object.
};
			

//  -------------------------------------------------------------------------
/// Like iconrequesthandler, but for the 128x128 'emblem' version of
/// the icons.
//  -------------------------------------------------------------------------
class emblemrequesthandler : public httpdobject
{
public:
			 emblemrequesthandler (class opencoreApp *papp,
			 					   httpd &server);
			
			~emblemrequesthandler (void) {}
			
	int		 run (string &uri, string &postbody, value &inhdr,
				  string &out, value &outhdr, value &env,
				  tcpsocket &s);

private:
	class opencoreApp *app;
};
#endif
