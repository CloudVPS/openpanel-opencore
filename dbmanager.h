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

#ifndef _DBMANAGER_H
#define _DBMANAGER_H

#include <grace/str.h>
#include <grace/xmlschema.h>
#include <sqlite3.h>

#include "paths.h"

void _dbmanager_sqlite3_trace_rcvr(void *ignore, const char *query); // namespace?

//  -------------------------------------------------------------------------
/// Database manager class for OpenCORE. Offers abstract functions
/// pertaining to classes, objects and versioning of same. Currently
/// employs SQLite for backend storage.
//  -------------------------------------------------------------------------
class DBManager
{
public:
					/// Constructor
                     DBManager (void);
					/// Destructor; closes sqlite database
                    ~DBManager (void);

					/// opens sqlite database etc.
					bool init (const char *dbfile = PATH_DB);
					
					/// close database file
					void deinit ();

					/// authenticate user, give him permissions
					bool login(const statstring &username, const statstring &password);
					
					/// log in user based on externally verified credentials
					bool userLogin(const statstring &username);
					
					/// deauthenticate, take all permissions away
					void logout(void);
					
					/// fetch reason for last error return
					string &getLastError(void);

					/// fetch numeric code for last error return
					int getLastErrorCode(void);
					
		// NON-TRANSACTION METHODS
					/// find an object, returns uuid
					string *findObject(const statstring &parent,
					                   const statstring &ofclass = nokey,
					                   const statstring &withuuid = nokey,
					                   const statstring &withmetaid = nokey);
					
					/// return uuid of parent based on child uuid
					string *findParent(const statstring &uuid);
					
					/// list objects (of a certain class), within the current context
					/// \verbinclude db_listObjects.format
					bool listObjects(value &into, const statstring &parent=nokey, const value &ofclass=nokey, bool formodule = false, int count=-1, int offset=0);
					
					/// replace a complete set of objects identified by class and perhaps parent
          bool replaceObjects (value &newobjs, const statstring &parent=nokey, const statstring &ofclass=nokey);
        	
					/// list a whole object subtree, leaf-first
					bool listObjectTree (value &into, const statstring &uuid);

					/// filter a listObjects resultset according to a whitelist
					bool applyFieldWhiteList (value &objs, value &whitel);
					
					/// fetch an object by uuid
					bool fetchObject(value &into, const statstring &uuid, bool formodule=false);

					/// create object, possibly in the current uniqueness context
					/// returns value with items "uuid" and "version"
					string *createObject(const statstring &parent,
					                    const value &withmembers,
					                    const statstring &ofclass,
					                    const statstring &metaid = nokey,
					                    bool permcheck = false,
					                    bool immediate = false);
					                    
					/// determine whether an object can be deleted.
					bool isDeleteable(const statstring &uuid);

					/// mark object deleted
					bool deleteObject(const statstring &uuid, bool immediate=false, bool asgod=false);

					/// update an object - ALL members are replaced.
					/// uuid is mandatory
					bool updateObject(const value &withmembers, const statstring &uuid, bool immediate=false, bool deleted=false, bool asgod=false);
					
					/// checks quota for logged-in user or another user
					/// returns -2 for failure, -1 for unlimited, 0..MAXINT for actual limit
					int getUserQuota(const statstring &ofclass, const statstring &useruuid=nokey, int *usage=NULL);
					
					/// sets quota for a user
					bool setUserQuota(const statstring &ofclass, int count, const statstring &useruuid=nokey);
					
					/// change owner of object
					bool chown(const statstring &objectuuid, const statstring &useruuid);

					/// find the class, given the UUID of the class or an instance of it
					statstring *classNameFromUUID(const statstring &uuid);
					
					/// mark object as reality
					bool reportSuccess(const statstring &uuid);
					
					/// delete pending object (this is realtime and interactive for the user)
					bool reportFailure(const statstring &uuid);
					
					/// mark object as postponed, with reason (asynchronous!)
					// TODO: implement
					bool reportPostponed(const statstring &uuid, const string &reason);
					
					/// register a class from a module
					/// details like uuid, name and version are all in the &classdata
					bool registerClass(const value &classdata);

					/// Get a non-object quota number for a specific tag/user.					
					int getSpecialQuota (const statstring &tag, const statstring &useruuid);

					/// Get recursively accumulated usage for a specific tag/user.
					int getSpecialQuotaUsage (const statstring &tag, const statstring &useruuid);
					
					/// Get the quota warning level for a specific tag/user.
					int getSpecialQuotaWarning (const statstring &tag, const statstring &useruuid);
				
					/// Set a non-object quota number for a specific tag/user.
					bool setSpecialQuota (const statstring &tag, const statstring &useruuid, int quota, int warning, value &phys);
					
					/// Set non-recursive(!) usage for a specific tag/user.
					bool setSpecialQuotaUsage (const statstring &tag, const statstring &useruuid, int amt);
					
					/// Return a list of tags that are in use
                    value *listSpecialQuota();
					
					/// crush you like windshield!
					void enableGodMode(void);
					
                    void getCredentials(value &creds);
                    void setCredentials(const value &creds);
protected:
          /// did someone delete/change our user while we were logged in?
          bool userisgone();

  				/// helper function for markaswanted, markasreality
					bool markcolumn(const statstring &column, const statstring &uuid, int version);

					/// local id of authenticated user
					int userid;
					
					/// validates the database schema
					bool checkschema(void);

					/// escapes and joins values for usage in UPDATE/SELECT/DELETE
					string *escapeforsql (const statstring &glue, const statstring &sep, value &vars);

					/// escapes and joins values for usage in INSERT
					string *escapeforinsert (const value &vars);
					
					/// execute query with sqlite, return stuff as value object
					value *dosqlite (const statstring &query);
					
					/// implementation of dosqlite
					value *_dosqlite (const statstring &query);

					/// find version of previous metaid use
					int findObjectDeletedVersion (int uc, const statstring &withmetaid);
					
					/// find local ID for a class, -1 if not found (because Class itself is 0)
					int findclassid(const statstring &classname);
					
					/// list a subtree recursively leaf-first
					bool _listObjectTree (value &into, int localid);
					
					/// insert a relation between A and B
					bool relate(int A, const statstring &relation, int B);
					
					/// copy a relation based on localid
					bool copyrelation(int P, int Q);
					
					/// find full classname from a local class id
					string *_classNameFromUUID(const int classid);
					
					/// find metaid for a local object id
					string *_findmetaid(const int id);
					
					/// check owner-wise power over local object id
					bool haspower(int oid, int uid);
					
					/// checks class right for (logged-in) user
					bool _getClassRight(int classid, int uid, const statstring &right);

					/// finds all class right for (logged-in) user
					bool _getClassRights(value &into, int uid);
					
					/// sets/unsets class right for logged-in user
					bool _setclassright(int classid, int uid, const statstring &right, bool allowed);
					
					/// bulk-set class rights for logged-in user
					bool _setclassrights(int uid, const value &rights);

					/// check for a specific class attribute
					bool classhasattrib(int classid, const statstring attrib);

					/// get a specific class attribute
					string *classgetattrib(int classid, const statstring attrib);

					/// get classdata as value; with caching
					value *getClassData(int classid);
					
					/// find the local id for a given uuid
					int findlocalid(const statstring &uuid);
					
					/// fill the ancestry mirror for a userid
					bool setpowermirror(int aid);

					/// fill the ancestry mirror for a userid
					bool _setpowermirror(int aid);

					/// to our short xml format
					string *serialize(const value &members);
					
					/// from our short xml format
					value *deserialize(const string &content);

					/// resolve all references
					bool deref(value &members, int localclassid);

					/// handle 'privateformodule' attribute
					value *filter(const value &members, int localclassid);

					/// replace passwords with emptiness
					value *hidepasswords(const value &members, int localclassid, bool tagonly=false);
				
					/// correctly escape a string for usage in SQL queries
					string *sqlstringescape(const string &s);
					
					/// validate fieldlist against class on create/update
					bool checkfieldlist(value &members, int classid);
					
					/// checks whether one metaid fits into another domainwise
					bool _checkdomainsuffix(const string &child, const string &parent, const char sep);
					
					/// mark object as reality
					bool _reportSuccess(const statstring &uuid);

					/// copy tree from prototype, return uuid of copy root
					string *copyprototype(int fromid, int parentid, int ownerid, value &repl, bool rootobj = true, const value &members = emptyvalue);

					/// storage for last error condition
					string lasterror;
					
					/// storage for last error code
					int errorcode;
					
					// god boolean
					bool god;
};

#endif

