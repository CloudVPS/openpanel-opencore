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
	virtual bool	 getconfig (const value &env);
	
	void			 addclass (const statstring &id, class CoreClass *who);
	void			 senderror (int code, const string &err);
	
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
	
	void			 setparam (const value &p);
	virtual bool	 create (const value &env);
	virtual bool	 update (const value &env);
	virtual bool	 remove (const value &env);
	
	int				 code;
	string			 error;
	statstring		 name;
	
protected:
	value			*listaliases (const value &env);
	value			 param;
	string			 id;
	
	void			 alias (const statstring &aliasClass);
	void			 seterror (int c, const string &e);
};

class AuthDaemon
{
public:
					 AuthDaemon (void);
					~AuthDaemon (void);
					
	bool			 installfile (const string &fname, const string &path);
	bool			 deletefile (const string &fqpath);

	bool			 adduser (const string &username, const string &shell,
							  const string &pwhash);
	bool			 deleteuser (const string &username);
	bool			 setusershell (const string &username, const string &shell);
	bool			 setuserpass (const string &username, const string &pwhash);
	bool			 setquota (const string &username, unsigned int soft,
							   unsigned int hard);

	bool			 startservice (const string &svcname);
	bool			 stopservice (const string &svcname);
	bool			 reloadservice (const string &svcname);
	bool			 setonboot (const string &svc, bool enabled);
	
	bool			 runscript (const string &sname, const value &param);
	bool			 runuserscript (const string &sname, const value &param,
									const string &usr);

	bool			 makedir (const string &dirname);
	bool			 deletedir (const string &dirname);
	bool			 makeuserdir (const string &user, const string &mode,
								  const string &dirname);
	bool			 deleteuserdir (const string &user, const string &dirname);

	bool			 osupdate (void);
	string			*getobject (const string &filename);

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
