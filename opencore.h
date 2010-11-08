// This file is part of OpenPanel - The Open Source Control Panel
// The Grace library is free software: you can redistribute it and/or modify it 
// under the terms of the GNU General Public License as published by the Free 
// Software Foundation, either version 3 of the License.
//
// Please note that use of the OpenPanel trademark may be subject to additional 
// restrictions. For more information, please visit the Legal Information 
// section of the OpenPanel website on http://www.openpanel.com/

#ifndef _opencore_H
#define _opencore_H 1
#include <grace/daemon.h>
#include <grace/configdb.h>
#include <grace/terminal.h>

#include "moduledb.h"
#include "session.h"
#include "dbmanager.h"
#include "opencorerpc.h"

//  -------------------------------------------------------------------------
/// Implementation template for application config.
//  -------------------------------------------------------------------------
typedef configdb<class OpenCoreApp> appconfig;

//  -------------------------------------------------------------------------
/// Main daemon class.
//  -------------------------------------------------------------------------
class OpenCoreApp : public daemon
{
public:
						 /// Default constructor.
		 				 OpenCoreApp (void);
		 				 
		 				 /// Destructor.
		 				~OpenCoreApp (void);
	
						 /// Main loop. Initializes all threads and waits
						 /// for exciting things to happen (or currently
						 /// it jumps into the shell).
	int					 main (void);
	
						 /// Emit dancing bears.
	void				 dancingBears (void);
	
	cli<OpenCoreApp>	 shell; ///< Command line shell interpreter.

						 /// Log an error, als keeps a carbon copy
						 /// available for coreclients using the
						 /// OpenCORE:ErrorLog CoreClass.
	void				 logError (const string &who, const string &what);
	
	void				 log (log::priority prio, const string &who,
							  const string &what);
	
	void				 log (log::priority prio, const string &who,
							  const char *fmt, ...);
	
						 /// Get last logged errors.
	value				*getErrors (void);
	
						 /// Resolve a regular expression through
						 /// the regexpdb.
	string				*regexp (const string &from);
	
protected:
						 // Configuration watcher to setup the logging
						 // process
	bool				 confSystem (config::action act, keypath &path,
								     const value &nval, const value &oval);

						 // Configuration watcher to set up 
						 // communication sockets
	bool				 confRpc (config::action act, keypath &path,
								  const value &nval, const value &oval);
	
						 /// Tab-expansion source for a session-id. This
						 /// will go once we go full daemon mode. The
						 /// implementation of configd may make this day
						 /// come sooner rather than later.
	value				*srcSessionId (const value &, int);
	
						 /// Sets up the shell and runs it.
	void				 commandline (void);
	
						 ///{
						 /// CLI handler.
	int					 cmdShowSessions (const value &);
	int					 cmdShowSession (const value &);
	int					 cmdShowVersion (const value &);
	int					 cmdShowThreads (const value &);
	int					 cmdShowClasses (const value &);
						 ///}
						 
						 /// CLI handler: exit all.
	int					 cmdExit (const value &v) { return 1; }
	
						 /// Checks whether authd runs.
						 /// \return False if authd is unavailable.
	bool				 checkAuthDaemon (void);
	
	static void			 memoryLeakHandler (void);
	
	appconfig			 conf; ///< Configuration database.
public: // TODO: temporary hack for module session! FIXME!
	ModuleDB			*mdb; ///< Module manager.
	SessionDB			*sdb; ///< Session manager.
protected:
	OpenCoreRPC			*rpc; ///< RPC manager.
	SessionExpireThread	*sexp; ///< Session expire thread.
	lock<value>			 errors; ///< Logged errors.
	value				 debugfilter; ///< Filter for debug logging.
	lock<value>			 regexpdb; /// < Regular expression class definitions.
};

extern OpenCoreApp *CORE;

#endif
