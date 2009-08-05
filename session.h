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

#ifndef _OPENCORE_SESSION_H
#define _OPENCORE_SESSION_H 1

#include <grace/value.h>
#include <grace/thread.h>
#include "api.h"
#include "dbmanager.h"

$exception (sqliteInitException, "Error initializing sqlite");
$exception (sessionUnlinkException, "Error unlinking session node");

//  -------------------------------------------------------------------------
/// Collection class for coresession objects.
/// This class is responsible for creating new sessions and giving
/// access to sessions created earlier through their unique session-id.
//  -------------------------------------------------------------------------
class sessiondb
{
public:
									 /// Constructor.
									 /// \param pmdb Reference to a
									 ///             moduledb object
									 ///             that will be used
									 ///             by the session
									 ///             objects.
									 /// \param ptmdp Reference to the
									 ///              template database.
									 sessiondb (class moduledb &pmdb);
									 
									 /// Destructor.
									~sessiondb (void);

									 /// Find a coresession by its
									 /// session-id. A pointer is
									 /// returned, but sessiondb remains
									 /// fully responsible for the
									 /// allocation of any session
									 /// objects. Before discarding
									 /// the pointer, either 
									 /// the release() or expire()
									 /// method should be called.
									 /// \param id The session-id (uuid)
									 /// \return Pointer to the session
									 ///         or NULL if not found.
	class coresession				*get (const statstring &id);
	
									 /// Create a new session.
									 /// The pointer returned should be
									 /// reported back through the
									 /// remove() or expire() method.
									 /// \param meta Any session metadata
									 ///             that should be added.
									 /// \return Pointer to the session.
	class coresession 				*create (const value &meta);
	
									 /// Release a claim on a session.
									 /// The session will be kept in
									 /// the database until it expires,
									 /// with the timer for expiry reset
									 /// to the current moment. A new
									 /// call to get() and a subsequent
									 /// call to this method would
									 /// buy the session more time.
									 /// \param s Pointer to the session
									 ///          to release.
	void							 release (class coresession *s);
	
									 /// Permanently remove a session
									 /// from the database. Should be
									 /// called to explicitly log a user
									 /// out.
									 /// \param s Pointer to the session
									 ///          to remove.
	void							 remove (class coresession *s);
	
									 /// Expire any unlocked sessions that
									 /// are past their due time, or
									 /// locked sessions that have been
									 /// in a locked state beyond reasonable
									 /// limits.
									 /// \return Number of sessions
									 ///         cleaned.
	int								 expire (void);
	
									 /// Report if a session exists.
									 /// \param id The session-id.
									 /// \return True if session exists.
	bool							 exists (const statstring &id);
	
									 /// Generate a list of sessions.
									 /// \return A new value object with
									 ///         the active sessions.
	value							*list (void);
	
protected:
									 /// Find a coresession by id,
									 /// returns NULL if not found.
									 /// \param id The session-id.
									 /// \param noreport If true, don't
									 ///        report an error on failure.
	class coresession				*find (const statstring &id,
										   bool noreport = false);
	
									 /// Find or create a coresession.
									 /// \param id The session-id.
	class coresession				*demand (const statstring &id);
	
									 /// Add a created session to the
									 /// linked list.
	void							 link (class coresession *);
	lock<int>						 lck; //< Lock on the dictionary.
	class coresession				*first; ///< Linked list
	class coresession				*last; ///< Linked list.
	class moduledb					&mdb; ///< Reference to the global moduledb.
};

//  -------------------------------------------------------------------------
/// A thread that does timed expires on the session database.
//  -------------------------------------------------------------------------
class sessionexpire : public thread
{
public:
				 /// Constructor.
				 /// \param psdb The sessiondb to weed.
				 sessionexpire (sessiondb *psdb)
				 	: thread ("sessionexpire")
				 {
				 	sdb = psdb;
				 	spawn ();
				 }
				 
				 /// Destructor.
				~sessionexpire (void)
				 {
				 }
				 
				 /// Run-method. Does a timed sweep every 60 seconds,
				 /// accepts cmd="die" events.
	void		 run (void);
	
				 /// Shut down the thread. Waits for the thread to
				 /// finish.
	void		 shutdown (void)
				 {
				 	sendevent ("shutdown");
				 	shutdownCondition.wait();
				 }
protected:
	conditional	 shutdownCondition; ///< Will raise on thread shutdown.
	sessiondb	*sdb; ///< Link back to the session database.
};

//  -------------------------------------------------------------------------
/// Keeps track of a session between a user interface and the opencore
/// system. This object is kept around to have a unique security context
/// for an authenticated user. Actual session-like context is left entirely
/// to the implementing layer. This class contains methods for
/// manipulating the opencore object space. It leans on the moduledb and
/// dbmanager classes to perform the necessary tasks at the back end.
///
/// Most calls to coresession need a parentid context. The parentid
/// acts as a cursor inside the object space. At the root level, parentid
/// is set to nokey. To look at objects inside another object, you
/// use its instance uuid as a parentid argument.
//  -------------------------------------------------------------------------
class coresession
{
friend class sessiondb;
friend class quotaclass;
friend class classlistclass;
public:
						 /// Constructor. The session-id is generated
						 /// by the sessiondb.
						 /// \param myid The session-id (should be UUID).
						 /// \param mdb Reference to the moduledb object
						 ///            that we'll be using as a punch bag
						 ///            for our calls.
						 coresession (const statstring &myid,
						 			  class moduledb &mdb);
					 	
					 	 /// Destructor.
						~coresession (void);
	
						 /// Authenticate the session as a specific
						 /// user.
						 /// \param user The username.
						 /// \param pass The plaintext password.
						 /// \param superuser Client is a recognized unix superuser.
	bool				 login (const string &user, const string &pass, bool superuser=false);
	
	                     /// Set credentials (usually copied from another session).
	                     /// \param creds Credential specification
    void                 setCredentials (const value &creds);
	
	                     /// Get credentials for copying.
	                     /// \param creds Value object for storing credentials
    void                 getCredentials (value &creds);

						 /// Authenticate the session as a specific
						 /// pre-validated user.
						 /// \param user The user name.
						 /// \param pass The plaintext password.
	bool				 userlogin (const string &user);
	
	
						 /// Give new parent-id provided we bound
						 /// to another context.
						 /// \param parentid The original parentid, if
						 ///                 applicable (nokey for root).
						 /// \param ofclass The class of the object to bind
						 ///                to.
						 /// \param withkey The uuid or metaid of the object
						 ///                to bind to.
						 /// \return The new 'parentid' key to use if
						 ///         switching context to this object, or
						 ///         NULL/nokey if this is not possible.
	statstring			*bindcontext (const statstring &parentid,
									  const statstring &ofclass,
									  const statstring &withkey);
	
						 /// Back up a context-id.
						 /// \param parentid The original parentid.
						 /// \return The new parentid, or NULL/nokey if
						 ///         this would move us to the root.
	statstring			*upcontext (const statstring &parentid);
	
						 /// Get a list of available object
						 /// instances within the current
						 /// context. Returns a tree in the
						 /// /$classname/$instanceid/$instancevar
						 /// layout.
						 /// \param parentid The context, nokey for root.
						 /// \param ofclass Restrict to a specific class
						 /// \param offset An offset if you want a range.
						 /// \param count Maximum size of resultset.
						 /// \return data in the following format:
						 /// \verbinclude db_listObjects.format
	value				*listObjects (const statstring &parentid,
										const statstring &ofclass = nokey,
										int offset=0, int count=-1);
	
						 /// Filter a listObjects resultset through a fieldname
						 /// whitelist
						 /// \param objs listObjects result
						 /// \param whitel list of allowed fieldnames
	bool				 applyFieldWhiteLabel (value &objs, value &whitel);
	
						 /// Get a particular object.
						 /// \param parentid The object parent's uuid, or
						 ///                 nokey if the object is at the
						 ///                 root level.
						 /// \param ofclass The object's class.
						 /// \param withkey The object's uuid or metaid.
	value				*getobject (const statstring &parentid,
									const statstring &ofclass,
									const statstring &withkey);

						 /// Get the classname for an instance or its parent
	statstring			*getclass (const statstring &parentid);
	
						 /// Remove a specific object.
						 /// \param parentid The object parent's uuid, or
						 ///                 nokey if the object is at the
						 ///                 root level.
						 /// \param ofclass The opencore class of the object.
						 /// \param withkey The id of the instance to nuke.
						 /// \return true if the delete succeeded.
	bool				 deleteObject (const statstring &parentid,
									   const statstring &ofclass,
									   const statstring &withkey,
									   bool immediate = false);
									 
						 /// Create an object with an automatic
						 /// index key.
						 /// \param parentid The object parent's uuid, or
						 ///                 nokey if the object is at the
						 ///                 root level.
						 /// \param ofclass The opencore class.
						 /// \param withparams Instance records.
						 /// \return Instance id on success, empty
						 ///         string or NULL on failure.
	string				*createObject (const statstring &parentid,
									   const statstring &ofclass,
									   const value &withparams,
									   const statstring &withid = nokey,
									   bool immediate = false);
						 
						 /// Update records for an instance.
						 /// \param parentid The object parent's uuid, or
						 ///                 nokey if the object is at the
						 ///                 root level.
						 /// \param ofclass The opencore class.
						 /// \param withid The requested id.
						 /// \param withparam Instance records.
	bool				 updateObject (const statstring &parentid,
									   const statstring &ofclass,
									   const statstring &withid,
									   const value &withparam,
									   bool immediate = false);
						
						 /// Add an opencore error to the session error
						 /// list. See error.h and rsrc/resources.xml at
						 /// the /errors/$LANGUAGE key.
						 /// \param code The error code.
						 /// \param msg The error message.
	void				 seterror (unsigned int code, const string &msg);
						
						 /// Add an opencore error to the session error
						 /// list. See error.h and rsrc/resources.xml at
						 /// the /errors/$LANGUAGE key.
						 /// \param code The error code.
	void				 seterror (unsigned int code);

						 /// \return The number of errors.
	int					 errorcount (void) { return errors.exists("code"); }

                         /// Return error. The value object
                         /// contains three keys:
                         /// - code (i_unsigned)
                         /// - message (i_string)
                         /// \return const reference to the error value.
    const value         &error (void) { return errors; }

						 /// Call a method.
						 /// \param parentid The object's parentid, or
						 ///                 nokey for root object or
						 ///                 class methods.
						 /// \param ofclass The class of the object.
						 /// \param withid The id of the object instance,
						 ///               or nokey for a class method.
						 /// \param method The method name.
						 /// \param withparam Optional parameters.
	value				*callmethod (const statstring &parentid,
									 const statstring &ofclass,
									 const statstring &withid,
									 const statstring &method,
									 const value &withparam);
	
						 /// Gathers the classinfo for a specific
						 /// object uuid. The argument gets resolved
						 /// to its class and getclassinfo() is called.
	value				*getclasses (const statstring &puuid)
						 {
						 	// TODO get class name from context
						 	string uuid;
						 	uuid = puuid.sval();
						 	
						 	value obj;
						 	
						 	if (db.fetchObject(obj, uuid, false))
								return getclassinfo (obj["class"]);
						 	
						 	return NULL;
						 }
						 
						 /// Get class-related metadata in a manageable
						 /// format. A glorified proxy for the
						 /// moduledb::getclassinfo() call.
						 /// \param forclass The class involved.
	value				*getclassinfo (const string &forclass);
	
						 /// Returns a dictionary of records
						 /// for each available class indexed by its
						 /// classname. The records are in the format
						 /// returned by moduledb::getclassinfo().
	value				*getworld (void);

						 /// Returns quota limits for a class/user
						 /// combinations
						 /// \return FIXME
	value				*getUserQuota (const statstring &useruuid = nokey);
										
						 /// Sets quota limit for a specific class/user
						 /// combination, self if user is not given.
	bool				 setUserQuota (const statstring &ofclass,
									    int count,
										const statstring &useruuid = nokey);

						 /// Changes owning user of specific object.
	bool				 chown(const statstring &ofobject,
							   const statstring &user);

						 /// Proxy for moduledb::listparamsformethod()
	value				*listparamsformethod (const statstring &parentid,
											  const statstring &ofclass,
											  const statstring &withid,
											  const statstring &methodname);

						 /// Proxy for moduledb::listmodules().
	value				*listmodules (void);
	
						 /// Proxy for moduledb::listclasses();
	value				*listclasses (void);
	
						 /// Resolve a class name to a coremodule.
						 /// \param cl The class name.
	class coremodule	*getmoduleforclass (const statstring &cl);
	
						 /// Resolve a module name to its coremodule
						 /// objects.
						 /// \param mname The module name.
	class coremodule	*getmodulebyname (const statstring &mname);
	
						 /// Verify that a class is registered with
						 /// opencore.
						 /// \param cl The class name.
	bool				 classexists (const statstring &cl);
	
						 /// Get an array of languages.
	value				*getlanguages (void);
	
						 /// Handle a cascading update for a class
						 /// that was indicated with 'childrendeps'.
						 /// Goes over all other objects of the
						 /// parent and sends them update messages
						 /// with their old data, but including
						 /// a new embedded list with the changed
						 /// depends.
						 /// \param parentid The id of the object's parents.
						 /// \param ofclass The class of the child.
						 /// \param withid The uuid of the child.
	void				 handlecascade (const statstring &parentid,
										const statstring &ofclass,
										const string &withid);

						 /// Resolve an object uuid to its metaid.
	statstring			*uuidtometa (const statstring &uuid);

	statstring			 id; ///< The session-id
	value				 meta; ///< Meta data from sessiondb::create.

	coresession			*next; ///< List link.
	coresession			*prev; ///< List link.
	coresession			*higher; ///< Hash link.
	coresession			*lower; ///< Hash link.
	
	void				 mlockr (void); ///< Lock module access.
	void				 mlockw (const string &plocker); /// Write-lock module access.
	void				 munlock (void); ///< Unlock access.
	
protected:
						 /// Evaluates crypted fields, calling the
						 /// module's crypt() method. If a crypted
						 /// field is empty, the old data is loaded.
						 /// Otherwise the crypt is applied.
						 /// \param parentid The object's parentid.
						 /// \param ofclass The object's class.
						 /// \param withid the object-id.
						 /// \param param (in/out) object parameters.
						 /// \return False on failure.
	bool				 handlecrypts (const statstring &parentid,
									   const statstring &ofclass,
									   const statstring &withid,
									   value &param);

						 /// Handles synchronization of dynamic classes.
						 /// These get their object list through the
						 /// module API, which is synchronized against
						 /// the sqlite database (to facilitate other
						 /// forms of magic).
						 /// \fixme Ideally we should not need to
						 ///        synchronize with the database. Figure
						 ///        out if we can deal with dynamic objects
						 ///        in much the same way as internalclass
						 ///        objects.
						 /// \param parentid The parent-id.
						 /// \param ofclass The class name.
						 /// \param offset Query offset (may be broken).
						 /// \param count Max rows (may be broken).
	bool				 syncdynamicobjects (const statstring &parentid,
											 const statstring &ofclass,
											 int offset, int count);

						 /// Gather a mixed list of real objects that
						 /// inherit from a meta class.
						 /// \param parentid The parent-id.
						 /// \param ofclass The class name.
						 /// \param offset Query offset (broken).
						 /// \param count Max rows (broken).
	value				*listmeta (const statstring &praentid,
								   const statstring &ofmetaclass,
								   int offset, int count);

						 /// Create or retrieve an artificial UUID for a
						 /// specific user/metaid combination. The
						 /// OpenCORE:Quota class is completely synthetic
						 /// so we keep a cached map inside the quotamap
						 /// variable connected to the session.
	statstring			*getquotauuid (const statstring &userid,
									   const statstring &metaid);

	class moduledb		&mdb; ///< Link to the moduledb.
	class DBManager		 db; ///< Local DBManager instance.
	value				 errors; ///< Details of error.
	time_t				 heartbeat; ///< Timeout tracker.
	int					 inuse; ///< Flags status.
	string				 locker; ///< Tag owning the moduledb write lock.
	lock<bool>			 spinlock; ///< Serialize access to each session.
	value				 quotamap; ///< Mapping between generated uuids and quotas.
};

#endif
