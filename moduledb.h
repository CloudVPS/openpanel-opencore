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

#ifndef _OPENCORE_MODULEDB_H
#define _OPENCORE_MODULEDB_H 1

#include "module.h"
#include "session.h"
#include "dbmanager.h"
#include "internalclass.h"

$exception (CoreClassNotFoundException, "Core class not found");
typedef dictionary<class CoreModule*> moduledict;

//  -------------------------------------------------------------------------
/// Represents the collection of loaded modules.
/// Moderates access to the CoreModule objects through calls that
/// will be made by a CoreSession.
//  -------------------------------------------------------------------------
class ModuleDB
{
public:
						 /// Constructor. Initializes linked list.
						 ModuleDB (void);
						 
						 /// Destructor. Clean up modules.
						~ModuleDB (void);
						
						 /// Initialize modules from the module
						 /// directory.
						 //// \param forcereloadmodules An array with
						 ///            names of modules that should
						 ///            go through a new round of
						 ///            getconfig initialization.
	void				 init (const value &forcereloadmodules = emptyvalue);
							
						 /// Command a CoreModule to create an object.
						 /// \param ofclass The object's class.
						 /// \param withid The object id (nokey for uuid)
						 /// \param parm The member data.
						 /// \param err (out) Error text
						 /// \return status_ok/status_failed/status_postponed
	corestatus_t		 createObject (const statstring &ofclass,
									   const statstring &withid,
									   const value &parm,
									   string &err);
	
						 /// Command a CoreModule to update an object.
						 /// \param ofclass The object's class.
						 /// \param withid The object metaid or uuid.
						 /// \param parm The new member data.
						 /// \param err (out) Error text
						 /// \return status_ok/status_failed/status_postponed
	corestatus_t		 updateObject (const statstring &ofclass,
									   const statstring &withid,
									   const value &parm,
									   string &err);
	
						 /// Command a CoreModule to delete an object.
						 /// \param ofclass The object's class.
						 /// \param withid The object metaid or uuid.
						 /// \param parm The new member data.
						 /// \param err (out) Error text
						 /// \return status_ok/status_failed/status_postponed
	corestatus_t		 deleteObject (const statstring &ofclass,
									   const statstring &withid,
									   const value &parm,
									   string &err);
	
	                     /// Update physical quotas (like unix fs quota)
	                     /// \param tag The physical quota tag (classname slash type)
	                     /// \param quota A keyed list of quota to set
	                     /// \param err (out) error text
	                     /// \return status_ok/status_failed/status_postponed 
    corestatus_t         setSpecialPhysicalQuota (const statstring &tag,
                                                  const value &quota,
                                                  string &err);

						 /// Calls the module with the 'crypt' command
						 /// to convert a specific field from plain text
						 /// to the module's native crypting method.
						 /// \param ofclass The class name involved.
						 /// \param fieldid The field name.
						 /// \param plaintext The plain text password.
						 /// \param crypted (out) The crypted password.
						 /// \return status_ok if everything worked out.
	corestatus_t		 makeCrypt (const statstring &ofclass,
									const statstring &fieldid,
									const string &plaintext,
									string &crypted);

						 /// Call an object method.
						 /// \param parentid Key of parent-object (nokey for root)
						 /// \param ofclass Name of the class.
						 /// \param withid Id of the object (or nokey for class-method)
						 /// \param method The method name.
						 /// \param param Arguments for calling.
						 /// \param returnp (out) return data.
						 /// \param err (out) Error text
	corestatus_t		 callMethod (const statstring &parentid,
									 const statstring &ofclass,
									 const statstring &withid,
									 const statstring &method,
									 const value &param,
									 value &returnp,
									 string &err);
									 
						 /// Get relevant meta-information about a class.
						 /// Most of the juice is loaded through the
						 /// CoreClass::makeClassInfo() with extra
						 /// information added about the parent class
						 /// and possible child classes.
						 /// \param classname The class id.
						 /// \param foradmin True if requester has admin rights.
						 /// \return The metadata:
						 /// \verbinclude classinfo.format
	value				*getClassInfo (const statstring &classname,
									   bool foradmin = true);
	
						 /// Get the raw meta-information for the
						 /// module carrying a specific class. This
						 /// call is deprecated and callers should expect
						 /// this method to start throwing nasty exceptions
						 /// and fielding obscene phone calls with your
						 /// parents.
						 /// \param forclass The class to use.
	value				*getMeta (const statstring &forclass);
	
						 /// Validate and normalize member data for
						 /// a specific class.
						 /// \param forclass (in) The class name.
						 /// \param data (in/out) The object's data.
						 /// \param err (out) Error description.
						 /// \return False on error.
	bool				 normalize (const statstring &forclass,
									value &data, string &err);
	
						 /// Check if a class is admin-only.
	bool				 isAdminClass (const statstring &clid);
	
						 /// Check if a class is registered.
						 /// \param clid The class name.
	bool				 classExists (const statstring &clid)
						 {
						 	return byclass.exists (clid);
						 }
						 
	bool				 classExistsUUID (const statstring &uuid)
						 {
						 	return byclassuuid.exists (uuid);
						 }
						 
						 /// Check if a class is a meta-baseclass.
						 /// See CoreClass::ismetabase .
	bool				 classIsMetaBase (const statstring &clid)
						 {
						 	if (clid == "ROOT") return false;
						 	CoreClass &c = getClass (clid);
						 	return c.ismetabase;
						 }
						 
						 /// Check if a specific module is loaded.
						 /// \param modname The name of the module.
	bool				 moduleExists (const statstring &modname)
						 {
						 	return byname.exists (modname);
						 }
	
						 /// Get a direct reference to a core class.
						 /// If you call this without going through the
						 /// classExists() method you will be taken out
						 /// back and shot.
	CoreClass			&getClass (const statstring &classname)
						 {
						 	CoreModule *r = NULL;
						 	sharedsection (modlock)
						 	{
						 		r = byclass[classname];
						 	}
						 	
						 	if (! r) throw (CoreClassNotFoundException());
						 	return r->classes[classname];
						 }
						 
						 /// Get a direct reference to a core class
						 /// from its uuid. No pets. No women callers.
						 /// Will throw a fit if the class uuid does
						 /// not exist.
	CoreClass			&getClassUUID (const statstring &classuuid)
						 {
						 	CoreModule *r = NULL;
						 	sharedsection (modlock)
						 	{
						 		if (byclassuuid.exists (classuuid))
							 		r = byclassuuid[classuuid];
						 	}
						 	
						 	if (! r) throw (CoreClassNotFoundException());
						 	if (! r->classesuuid.exists (classuuid))
						 	{
						 		throw (CoreClassNotFoundException());
						 	}
						 	return r->classesuuid[classuuid];
						 }
						 
						 /// Get an array of child classes that have
						 /// a given class as their required parent.
						 /// \param forclass The class to probe.
						 /// \return A reference to the value-object
						 ///         containing the list of child
						 ///         classes.
	const value			&getClasses (const statstring &forclass)
						 {
						 	return byparent[forclass];
						 }
	
						 /// Find the module for a given class.
						 /// \param classname The class to find.
						 /// \return Pointer to the CoreModule object or NULL.
	CoreModule			*getModuleForClass (const statstring &classname)
						 {
						 	CoreModule *r = NULL;
						 	sharedsection (modlock)
						 	{
						 		if (byclass.exists (classname))
						 			r = byclass[classname];
						 	}
						 	return r;
						 }
						 
						 /// Find a module by its name.
						 /// \param modname The module name.
						 /// \return pointer to the CoreModule object or NULL.
	CoreModule			*getModuleByName (const statstring &modname)
						 {
						 	CoreModule *r = NULL;
						 	sharedsection (modlock)
						 	{
						 		if (byname.exists (modname))
						 			r = byname[modname];
						 	}
						 	return r;
						 }
						 
						 /// Find a module handling the back-end of
						 /// a specific specialquota tag.
	CoreModule			*getModuleBySpecialQuota (const statstring &tag)
						 {
						 	CoreModule *r = NULL;
						 	sharedsection (modlock)
						 	{
						 		if (byquota.exists (tag))
						 			r = byquota[tag];
						 	}
						 	return r;
						 }
						 
						 /// Get a list of available modules.
						 /// \return retainable value object with an array.
	value				*listModules (void);
	
						 /// Get a list of available classes.
						 /// \return retainable value object with an array.
	value				*listClasses (void);
	
						 /// Use the getconfig mechanism to get an initial
						 /// configuration dump for a module.
						 /// \param primaryclass The module's primary class.
						 /// \todo Why are we looking this up by class?
						 /// \return The module's return data formatted
						 ///         in a way that makes dbmanager all
						 ///         warm and fuzzy, like this:
						 /// \verbinclude getconfig.format
	value				*getCurrentConfig (const statstring &primaryclass);


						 /// Checks if the class is defined as dynamic.
						 /// \param forclass Class name.
	bool				 classIsDynamic (const statstring &forclass);
	
						 /// If a class is defined with dynamic=true in
						 /// its module.xml file, the module will be
						 /// called with the 'listObjects' command.
						 /// The resulting data (put in /objects of the
						 /// module-result xml) is returned.
						 /// \param parentid The parent-id (nokey for root).
						 /// \param ofclass The dynamic class.
						 /// \param count Number of rows to return, -1 for
						 ///              all rows.
						 /// \param offset Offset in the result set.
						 /// \param err Output parameter for error.
						 /// \return Data in this format:
						 /// \verbinclude db_listObjects.format
	value				*listDynamicObjects (const statstring &parentid,
											 const statstring &mparentid,
											 const statstring &ofclass,
											 string &err,
										 	 int count=-1, int offset=0);

						 /// Get a parameter definition for a method
						 /// that is defined object-dynamic (that is,
						 /// the requested parameters for the method
						 /// depend on data inside the actual instance
						 /// that is only knowable by the module).
						 /// \param parentid The parent-id (nokey for root).
						 /// \param ofclass The dynamic class.
						 /// \param withid The instance's objectid.
						 /// \param methodname The name of the method.
	value				*listParamsForMethod (const statstring &parentid,
											  const statstring &ofclass,
											  const statstring &withid,
											  const statstring &methodname);
	
						 /// Trigger live synchronization for a specific
						 /// class.
						 /// \param ofclass The class involved
						 /// \param withserial The last known serial number.
	unsigned int		 synchronizeClass (const statstring &ofclass,
										   unsigned int withserial);

						 /// Public property, useful for debugging.
	value				 classlist;
	
						 /// Get a list of supported languages.
						 /// This is provisional, this method should
						 /// do smart things finding a common denominator
						 /// in languages supported by all modules.
	value				*getLanguages (void)
						 {
						 	returnclass (value) res retain;
						 	
						 	foreach (lang, languages)
						 	{
						 		res.newval() = lang.id().sval();
						 	}
						 	return &res;
						 }
						 
						 // Register the fact that a certain class 
						 // declared itself to be derived of a specific
						 // meta-class. The implication is that it
						 // should be included in any requested lists
						 // for objects of that metaclass.
	void				 registerMetaSubClass (const statstring &derivedid,
											   const statstring &baseid);
	
	const value			&getMetaClassChildren (const statstring &baseid);

						 /// Checks whether a class is handled by
						 /// opencore internally.
	bool				 isInternalClass (const statstring &classid)
						 {
						 	return InternalClasses.exists (classid);
						 }
						 
						 /// Get a reference to an InternalClass
						 /// object.
	InternalClass		&getInternalClass (const statstring &classid)
						 {
						 	return InternalClasses[classid];
						 }

protected:
						 /// Helper function for init.
	void				 loadModule (const string &mname, value &cache,
									 class DBManager &db);

						 /// Helper function for handling modules we see
						 /// for the first time.
	void				 handleGetConfig (const string &mname, value &cache,
										  class DBManager &db,
										  CoreModule *m);

	bool				 checkModuleCache (const string &mname, value &cache,
										   CoreModule *m);
	
	void				 registerClasses (const string &mname, value &cache,
										  class DBManager &db, CoreModule *m);

	void				 loadDependencies (const string &mname, value &cache,
										   class DBManager &db, CoreModule *m);

	void				 registerQuotas (CoreModule *m);
	
	void				 createStagingDirectory (const string &mname);
	
						 ///{
						 /// Linked list pointers
	CoreModule			*first, *last;
						 ///}
						 
	lock<int>			 modlock; ///< Lock on the ModuleDB.
	
						 /// Keeps a pointer to the CoreModules,
						 /// indexed by the classes they implement.
	moduledict			 byclass;
	
						 /// Coremodules indexed by the uuid of classes
						 /// they implement.
	moduledict			 byclassuuid;
	
						 /// Pointer collection for CoreModules,
						 /// indexed by module name.
	moduledict			 byname;
	
						 /// Another pointer collection, indexed by
						 /// specialquota tag.
	moduledict			 byquota;
	
						 /// For each class, an array of classnames
						 /// that have it as a required parent.
	value				 byparent;
		
						 /// Internal method for flattening trees.
	void				 parsetree (const value &, value &, int);
	
						 /// List of supported languages consolidated from
						 /// the modules.
	value				 languages;
	
						 /// Links meta-baseclassids to their possible
						 /// children.
	value				 metachildren;

						 /// Dictionary of pointers to internally handled
						 /// classes.
	InternalClassdb		 InternalClasses;
};

#endif
