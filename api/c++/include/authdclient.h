// This file is part of the OpenPanel API
// OpenPanel is free software: you can redistribute it and/or modify it 
// under the terms of the GNU Lesser General Public License as published by the
// Free Software Foundation, using version 3 of the License.
//
// Please note that use of the OpenPanel trademark may be subject to additional 
// restrictions. For more information, please visit the Legal Information 
// section of the OpenPanel website on http://www.openpanel.com/


#ifndef _SYS_DAEMON_H
#define _SYS_DAEMON_H

#include <grace/str.h>
#include <grace/tcpsocket.h>

#include <syslog.h>
#include <stdarg.h>

/// Authdaemon client class
class authdclient
{

  public:

				 /// Constructor
				 /// \param modulename Module identification (future use)
				 authdclient (const string &modulename) 
				 {
					_modulename = modulename;
					openlog (modulename.str(), LOG_PID, LOG_DAEMON);
					s.openread (3);
				 }
			
				 /// Destructor
				~authdclient (void) 
				 {
				 	closelog ();
				 }

				 /// Install a system configuration file
				 /// \param source
				 /// \param dest
				 /// \return ...
	int		 	 installfile (const string &, const string &);
	
				 /// Delete a system configuration file
				 /// \param conffile
				 /// \return sysd::<result identidier>
	int		 	 deletefile  (const string &);
	
				 /// Add a system user
				 /// \param username
				 /// \param shell
				 /// \param passwd
				 /// \return sysd::<result identidier>
	int 	 	 adduser (const string &, 
						  const string &, const string &);
					  
				 /// Delete a system user
				 /// \param username
				 /// \return sysd::<result identidier>
	int		 	 deluser (const string &);
	
	
				 /// Set the system user's shell
				 /// \param username
				 /// \param shell 
	int 		 setusershell (const string &, const string &);

				 /// Update a unix user password
				 /// \param username
				 /// \param password 
	int 		 setuserpass (const string &, const string &);


				 /// Set users quota
				 /// \param username
				 /// \param softlimit
				 /// \param hardlimit
	int		 	 setquota (const string &, unsigned int, unsigned int);
	
		
				 /// Start a system service
				 /// \param servicename
				 /// \return sysd::<result identidier>
	int		 	 startservice (const string &);

				 /// Stop a system service
				 /// \param servicename
				 /// \return sysd::<result identidier>
	int		 	 stopservice (const string &);
	
				 /// Reload a system service
				 /// \param servicename
				 /// \return sysd::<result identidier>
	int			 reloadservice (const string &);
	
			 	 /// Set service on boot
				 /// \param servicename
				 /// \param enabled true/false
	int 		 setserviceonboot (const string &, bool);
	
				 /// Run a single script
				 /// \param s scriptname
				 /// \param p params
				 /// \return sysd::<result identidier>
	int		 	 runscript (const string &s, const value &p);
	
	int			 runuserscript (const string &s, const value &parm,
								const string &usr);
	
				 /// Make a directory 
				 /// \param dirname Directory to create
				 /// \return sysd::<result identidier>
	int			 makedir (const string &);

				 /// Delete a directory 
				 /// \param dirname Directory to create
				 /// \return sysd::<result identidier>
	int			 deletedir (const string &);

	
				 /// Perform an OS update through swupd.
				 /// \return sysd::<result identidier>
	int			 osupdate (void);
	
				 /// Make a user directory
				 /// \param username
				 /// \param mode
				 /// \param directory name
	int			 makeuserdir (const string &, const string &, const string &);
	
				 /// Delete a user directory
				 /// \param username
				 /// \param directory name
	int			 deleteuserdir (const string &, const string &);	
	
				 /// Get data from a file on on disk which
				 /// requires root permissions to read
				 /// \param filename name of file to read
				 /// \return data from file in string format
	string		*getfiledata (const string &filename);
	
				 /// Rollback all changes done in this session
			 	 /// Rollbacks all actions since opencore
			 	 /// said hello to the authdaemon
	int 	 	 rollback (void);
	
				 /// Send quit to the authdaemon to tell you are 
				 /// finished doing jobs, else there will be done
				 /// a rollback
	int			 quit (void);
	
				 /// Get last error
				 /// \return value object with error information
	value 		*getlasterror (void)
			 	 {
			 		returnclass (value) rval retain;
			 	
			 		rval["code"] 		= errorcode;
			 		rval["resultmsg"]	= errortext;
			 		
			 		return &rval;
			 	 }


  protected:
  			 	 /// Set last error
  			 	 /// \param result the result from authd
	inline
	void 	 	 setlasterror (string &result);
	
	int 		 executecmd (const string &command);

  	string	 	 _modulename;
  	tcpsocket	 s;


  private:
    string		 errortext;
  	int			 errorcode;

};

#endif
