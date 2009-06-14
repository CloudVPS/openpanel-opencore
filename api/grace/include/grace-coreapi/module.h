#ifndef _GRACE_COREAPI_MODULE_H
#define _GRACE_COREAPI_MODULE_H 1

#include <grace/application.h>
#include <grace/dictionary.h>

class CoreModule : public application
{
public:
					 CoreModule (const string &moduleName);
					~CoreModule (void);
	
	int				 main (void);
	virtual bool	 getConfig (const value &env);
	
	void			 addClass (const statstring &id, class CoreClass *who);
	void			 sendResult (int code, const string &err);
	
	enum			 errorcode {
						E_OK = 0,
						E_VALIDATION = 1,
						E_CLASS = 2,
						E_NOTIMPL = 3,
						E_AUTHD = 4,
						E_OTHER = 5
					 };
					 
	const string	&name (void) { return creator; }

protected:
	dictionary<class CoreClass>	 classes;
};

extern CoreModule *MOD;

THROWS_EXCEPTION (
	unoverloadedConstructorException,
	0x66603400,
	"CoreClass constructor not overloaded");

class CoreClass
{
public:
					 CoreClass (void);
					 CoreClass (const string &className);
					~CoreClass (void);
	
	void			 setParam (const value &p);
	virtual bool	 create (const value &env);
	virtual bool	 update (const value &env);
	virtual bool	 remove (const value &env);
	
	const string	&error (void) { return _error; }
	int				 code (void) { return _code; }
	
	statstring		 name;
	
protected:
	int				 _code;
	string			 _error;
	value			 param;
	string			 id;
	
	value			*listAliases (const value &env);
	void			 alias (const statstring &aliasClass);
	void			 error (int c, const string &e);
};

class AuthDaemon
{
public:
					 AuthDaemon (void);
					~AuthDaemon (void);
					
	bool			 installFile (const string &fname, const string &path);
	bool			 deleteFile (const string &fqpath);

	bool			 addUser (const string &username, const string &shell,
							  const string &pwhash);
	bool			 deleteUser (const string &username);
	bool			 setUserShell (const string &username, const string &shell);
	bool			 setUserPass (const string &username, const string &pwhash);
	bool			 setQuota (const string &username, unsigned int soft,
							   unsigned int hard);

	bool			 startService (const string &svcname);
	bool			 stopService (const string &svcname);
	bool			 reloadService (const string &svcname);
	bool			 setOnBoot (const string &svc, bool enabled);
	
	bool			 runScript (const string &sname, const value &param);
	bool			 runUserScript (const string &sname, const value &param,
									const string &usr);

	bool			 makeDir (const string &dirname);
	bool			 deleteDir (const string &dirname);
	bool			 makeUserDir (const string &user, const string &mode,
								  const string &dirname);
	bool			 deleteUserDir (const string &user, const string &dirname);

	bool			 osUpdate (void);
	string			*getObject (const string &filename);

	bool			 rollback (void);
	void			 quit (void);
	
	string			 error;
					
protected:
	file			 s;
	bool			 connected;
	bool			 call (const string &cmd);
	bool			 call (const string &cmd, const string &p1);
	bool			 call (const string &cmd, const string &p1, const string &p2);
	bool			 call (const string &cmd, const string &p1,
						   const string &p2, const string &p3);
};

extern AuthDaemon authd;					

#define IMPLEMENT(x) APPOBJECT(x)

#endif
