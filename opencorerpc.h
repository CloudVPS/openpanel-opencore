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

#ifndef _OPENCORE_RPC
#define _OPENCORE_RPC 1

#include <grace/httpd.h>
#include <grace/str.h>
#include <grace/value.h>
#include <grace/filesystem.h>

class imagepreloader : public httpdobject
{
public:
				 imagepreloader (httpd &x);
				~imagepreloader (void);
				
	int			 run (string &uri, string &postbody, value &inhdr,
					  string &out, value &outhdr, value &env, tcpsocket &s);
};

//  -------------------------------------------------------------------------
/// This class provides basic handling for all
/// listening sockets
//  -------------------------------------------------------------------------
class opencorerpc
{
	public:
				 /// Constructor
				 /// \config Configuration parameters
				 ///
				 /// config:
				 /// - /unixsocket/sockpath   (string) path to socket file
				 /// - /unixsocket/minthreads (int)
				 /// - /unixsocket/maxthreads (int)
				 /// - /httpsocket/listenport (int) tcp port to bind
				 /// - /httpsocket/minthreads (int)
				 /// - /httpsocket/maxthreads (int)
				 ///
				 /// \param sdb Link to the session database.
				 /// \param papp Link to the application object.
				 opencorerpc (class sessiondb *sdb,
				 			  class opencoreApp *papp);

				 /// Destructor
				~opencorerpc (void);

				 /// Configuration checker for configdb.
		bool	 confcheck  (const value &conf);
		
				 /// Configuration activator for configdb.
		bool	 confcreate (const value &conf)
				 {
				 	return _confcreate (conf, false);
				 }
				 
				 /// Configuration updater for configdb.
		bool	 confupdate (const value &conf);
				 
		string	 error; ///< String containing the last error
				
	private:
				 httpd				 httpdUds;	///< HTTP Unix Domain Socket server
				 httpd				 httpdTcp;	///< HTTP TCP Server
				 
		class	 sessiondb			*pdb; ///< Link to the session database.
		class	 opencoreApp		*app; ///< Link to the application.
		class 	 rpcrequesthandler	*_huds; ///< Link to the unix domain http handler.
		class	 rpcrequesthandler	*_htcp; ///< Link to http handler for tcp domain.
		
				 /// Handle configuration information.
		bool	 _confcreate (const value &conf, int update);
};

#endif 
