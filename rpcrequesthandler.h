// This file is part of OpenPanel - The Open Source Control Panel
// OpenPanel is free software: you can redistribute it and/or modify it 
// under the terms of the GNU General Public License as published by the Free 
// Software Foundation, using version 3 of the License.
//
// Please note that use of the OpenPanel trademark may be subject to additional 
// restrictions. For more information, please visit the Legal Information 
// section of the OpenPanel website on http://www.openpanel.com/

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
/// Handler for sending module-specific files
//  -------------------------------------------------------------------------
class ModuleFileHandler : public httpdobject
{
public:
			 ModuleFileHandler (class OpenCoreApp *papp, httpd &srv,
			 					class SessionDB *db);
			~ModuleFileHandler (void);

	int		 run (string &uri, string &postbody, value &inhdr,
				  string &out, value &outhdr, value &env,
				  tcpsocket &s);

protected:
	class OpenCoreApp	*app;
	class SessionDB		*sdb; ///< Link to session database.
};			

//  -------------------------------------------------------------------------
/// Script parser for the landing page.
//  -------------------------------------------------------------------------
class LandingPageHandler : public httpdobject
{
public:
			 LandingPageHandler (class OpenCoreApp *papp, httpd &srv,class SessionDB *db);
			~LandingPageHandler (void) {}
			
	int		 run (string &uri, string &postbody, value &inhdr,
				  string &out, value &outhdr, value &env,
				  tcpsocket &s);

private:
	lock<value>		   rsscache;
	class OpenCoreApp *app;
	class SessionDB		*sdb; ///< Link to session database.
	xmlschema		   schema;
	
	value			  *getRSS (const string &url);
};

//  -------------------------------------------------------------------------
/// Dynamic resolution of backdrop wallpaper.
//  -------------------------------------------------------------------------
class WallpaperHandler : public httpdobject
{
public:
				 WallpaperHandler (class OpenCoreApp *papp, httpd &x);
				~WallpaperHandler (void) {}

	int			 run (string &uri, string &postbody, value &inhdr,
					  string &out, value &outhdr, value &env,
					  tcpsocket &s);

private:
	class OpenCoreApp *app;
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
