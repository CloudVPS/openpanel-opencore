// This file is part of OpenPanel - The Open Source Control Panel
// OpenPanel is free software: you can redistribute it and/or modify it 
// under the terms of the GNU General Public License as published by the Free 
// Software Foundation, using version 3 of the License.
//
// Please note that use of the OpenPanel trademark may be subject to additional 
// restrictions. For more information, please visit the Legal Information 
// section of the OpenPanel website on http://www.openpanel.com/

#ifndef _OPENCORE_RPC
#define _OPENCORE_RPC 1

#include <grace/httpd.h>
#include <grace/str.h>
#include <grace/value.h>
#include <grace/filesystem.h>

//  -------------------------------------------------------------------------
/// This class provides basic handling for all
/// listening sockets
//  -------------------------------------------------------------------------
class OpenCoreRPC
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
				 OpenCoreRPC (class SessionDB *sdb,
				 			  class OpenCoreApp *papp);

				 /// Destructor
				~OpenCoreRPC (void);

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
				 httpsd				 httpdSSL;	///< HTTP TCP Server
				 
		class	 SessionDB			*pdb; ///< Link to the session database.
		class	 OpenCoreApp		*app; ///< Link to the application.
		class 	 RPCRequestHandler	*_huds; ///< Link to the unix domain http handler.
		class	 RPCRequestHandler	*_htcp; ///< Link to http handler for tcp domain.
		
				 /// Handle configuration information.
		bool	 _confcreate (const value &conf, int update);
};

#endif 
