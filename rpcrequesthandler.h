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
#include <grace/xmlschema.h>


//  -------------------------------------------------------------------------
/// Handler for http-rpc requests coming in over the unix domain socket.
//  -------------------------------------------------------------------------
class RPCRequestHandler : public httpdobject
{
public:

			 /// Constructor
			 /// \param papp Used for applications logging functionality
			 /// \param server The http server object
			 /// \param db Link to the SessionDB.
			 /// Note, this http object does listen to all uri's by default
			 RPCRequestHandler (class OpenCoreApp *papp, 
						 httpd &server, 
						 class SessionDB *db);
			 
			 /// Destructor
			~RPCRequestHandler (void) {};


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
	class OpenCoreApp	*app; ///< Link back to application object.
	class SessionDB		*sdb; ///< Link to session database.
};

//  -------------------------------------------------------------------------
/// Handler for http requests in the /icon/ directory. Loads an
/// icon from outside the module directory by class uuid.
//  -------------------------------------------------------------------------
class IconRequestHandler : public httpdobject
{
public:
			 IconRequestHandler (class OpenCoreApp *papp,
			 					 httpd &server);
			 
			~IconRequestHandler (void) {}
			
	int      run (string &uri, string &postbody, value &inhdr,
				  string &out, value &outhdr, value &env,
				  tcpsocket &s);
				  
private:                      
	class OpenCoreApp	*app; ///< Link back to application object.
};

//  -------------------------------------------------------------------------
/// Handler for http requests in the /itemicon/ directory. Loads an
/// icon from outside the module directory by class uuid.
//  -------------------------------------------------------------------------
class ItemIconRequestHandler : public httpdobject
{
public:
			 ItemIconRequestHandler (class OpenCoreApp *papp,
									 httpd &server);
			~ItemIconRequestHandler (void) {}
	int      run (string &uri, string &postbody, value &inhdr,
				  string &out, value &outhdr, value &env,
				  tcpsocket &s);
				  
private:                      
	class OpenCoreApp	*app; ///< Link back to application object.
};
			

//  -------------------------------------------------------------------------
/// Like IconRequestHandler, but for the 128x128 'emblem' version of
/// the icons.
//  -------------------------------------------------------------------------
class EmblemRequestHandler : public httpdobject
{
public:
			 EmblemRequestHandler (class OpenCoreApp *papp,
			 					   httpd &server);
			
			~EmblemRequestHandler (void) {}
			
	int		 run (string &uri, string &postbody, value &inhdr,
				  string &out, value &outhdr, value &env,
				  tcpsocket &s);

private:
	class OpenCoreApp *app;
};

//  -------------------------------------------------------------------------
/// Script parser for the landing page.
//  -------------------------------------------------------------------------
class LandingPageHandler : public httpdobject
{
public:
			 LandingPageHandler (class OpenCoreApp *papp, httpd &srv);
			~LandingPageHandler (void) {}
			
	int		 run (string &uri, string &postbody, value &inhdr,
				  string &out, value &outhdr, value &env,
				  tcpsocket &s);

private:
	class OpenCoreApp *app;
	xmlschema		   schema;
};

//  -------------------------------------------------------------------------
/// Generate javascript code for preloading images.
//  -------------------------------------------------------------------------
class ImagePreloader : public httpdobject
{
public:
				 ImagePreloader (class OpenCoreApp *papp, httpd &x);
				~ImagePreloader (void);
				
	int			 run (string &uri, string &postbody, value &inhdr,
					  string &out, value &outhdr, value &env, tcpsocket &s);
					  
	class OpenCoreApp *app;
};


#endif
