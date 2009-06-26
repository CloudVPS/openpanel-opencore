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

#ifndef _OPENCORE_MODULE_H
#define _OPENCORE_MODULE_H 1

#include <grace/exception.h>
#include "session.h"
#include "status.h"

$exception (invalidClassAccessException, "Invalid class");

//  -------------------------------------------------------------------------
/// An abstract representation of a class as defined by a coremodule.
//  -------------------------------------------------------------------------
class coreclass
{
public:
					 /// Default constructor. The dictionary class needs
					 /// this to exist (it will normally attempt to
					 /// automatically create an object if it doesn't
					 /// exist yet). Used initialized this would wreak
					 /// havoc, though, so this constructor will throw
					 /// an exception.
					 coreclass (void);
					 
					 /// The actual useful constructor.
					 /// \param imeta The data for this class from module.xml
					 ///              in the new format as of 2006-08-29.
					 coreclass (const value &imeta, class coremodule *parent);
					 
					 /// Destructor.
					~coreclass (void);
	
	string			 name; ///< The class name.
	string			 icon; ///< The icon file name.
	string			 shortname; ///< The short name (for the CLI).
	string			 title; /// The title name (for the GUI).
	string			 description; ///< The class description.
	string			 emptytext; ///< Text to show when there are no objects.
	int				 sortindex; ///< The sorting index.
	int				 gridheight; ///< Height of the display grid (if applicable).
	bool			 hidegrid; ///< True if grid should be hidden if it contains 1 item.
	int				 formmargin; ///< Top margin of the class form in GUI.
	string			 menuclass; ///< To wich guiclass does this class belong
	string			 requires; ///< If set, what parent class is required.
	int				 version; ///< The class version.
	statstring		 uuid; ///< The class uuid (should not change over generations).
	bool			 hasprototype; ///< True if the class supports prototypes.
	
					 /// The 'magic prefix' for nodes that should be hidden.
	string			 magicdelimiter; 
	
					 /// The magic meta-id of an object that should act as
					 /// a prototype for new objects created of this class.
	string			 prototype;
	
					 /// Indexing flag. if true, the class uses a metaid for
					 /// its key.
	bool			 manualindex;
	
					 /// If true, the module expects updates of ofbjects
					 /// of this class (or one of its children) to come
					 /// as a single block with all records.
	bool			 allchildren;
	
					 /// If true, this class is a meta-baseclass that
					 /// represents a grouping of objects of other classes
					 /// that have <metatype>derived</metatype> in their
					 /// XML definition and this class set as the
					 /// <metabase/> parent.
	bool			 ismetabase;
	
					 /// If the class is derived in the context of
					 /// coreclass::ismetabase, this string will
					 /// contain the name of the meta-baseclass.
	string			 metabaseclass;
	string			 metadescription;
	
					 /// HTML-formatted explanation of the class,
					 /// if it exists.
	string			 explanation;
	
					 /// If true, the uniqueness context of the metaid
					 /// is enforced within the context of the parent
					 /// object, so that two objects of this class can
					 /// have the same meta-id as long as they are
					 /// not children of the same parent object.
					 /// If set to false, uniqueness of the metaid is
					 /// enforced globally for all objects of (this) class.
	bool			 uniqueinparent;
	
					 /// Restriction on child metaid relative to parent metaid
	string			 parentrealm;

					 /// For the uniqueinparent=false case, we can override
					 /// the class that defines the uniqueness context
	string			 uniqueclass;
	
					 /// A variant of allchildren, that applies to a specific
					 /// class of children objects, and applies even over
					 /// module boundaries.
	string			 childrendep;

					 /// Maximum number of instances. A value of 0
					 /// indicates an infinite supply.
	int				 maxinstances;
	
					 /// Metaid of singleton object of this class.
	string			 singleton;
	
					 /// If true, updates to objects of this class
					 /// should cascade towards siblings. This attribute
					 /// is implied from the 'childrendep' setting of
					 /// the parent class and filled in by
					 /// moduledb::loadmodule.
	bool			 cascades;

					 /// True if the objects should be world readable.
	bool			 worldreadable;
	
					 /// If true, the list of objects of this class is
					 /// actually dynamic; The module should be consulted
					 /// for an up-to-date list of objects.
	bool			 dynamic;
	
					 /// Verify a list of parameters against the rules
					 /// set out by the parameter and layout data. Any
					 /// default values are filled in.
					 /// \param param (in/out) The parameters as passed from
					 ///              the user interface / rpc layer.
					 /// \param error (out) An error string.
					 /// \return False on failure.
	bool			 normalize (value &param, string &error);
	
					 /// Set up all of the data needed by the rpc layer
					 /// for handling the classinfo-call. This presents
					 /// the following tree:
					 /// - structure/parameters: the member list.
					 /// - capabilities: The capabilities as boolean values.
					 /// - class: uuid, name and description of this class.
					 /// The data flowing from here is missing the information
					 /// about parent and child classes, this will be filled
					 /// in en route towards rpcinterface::classinfo().
					 /// 
					 /// \return A value object in the following format:
					 /// \verbinclude classinfo.format
	value			*makeclassinfo (void);
	
					 /// Get class registration information in the format
					 /// expected by dbmanager::registerclass().
					 /// \return A value object in the following format:
					 /// \verbinclude classregistration.format
	value			*getregistration (void);

	value			 param; ///< The member definition.
	value			 enums; ///< The enum definitions.
	value			 capabilities; ///< The capabilities.
	
					 /// The method definitions. As loaded from the
					 /// module.xml, this is an array of method names
					 /// containing param-definitions in the same
					 /// format as those in param.
	value			 methods;

	class coremodule&module; ///< Link to parent module.
protected:

					 /// Inner loop method for checking the definition of
					 /// an 'option' node in the layout definition against
					 /// the supplied data. Called from normalize().
					 /// \param p (in) Cursor into the layout.
					 /// \param md (in/out) The provided record.
					 /// \param error (out) Error string.
					 /// \return False on failure.
	bool			 normalizelayoutnode (value &p, value &md, string &error);
	
					 /// Return a 'flattened' version of the parameters
					 /// block, with child nodes instead of attributes.
	value			*flattenparam (void);
	
					 /// Return a 'flattened' version of the methods
					 /// block, with child nodes instead of attributes.
	value			*flattenmethods (void);
	
					 /// Return a 'flattened' version of any enums.
	value			*flattenenums (void);
	
					 /// Check if a value is in 'range' for a given enum.
					 /// \param i The enum-id.
					 /// \param v The value.
	bool			 checkenum (const statstring &i, const string &v);
};

typedef dictionary<coreclass> classdict;

//  -------------------------------------------------------------------------
/// Regulates access to a core module as it exists on disk.
/// The moduledb class should wrap up the context to send to
/// the module, together with the coresession object involved.
//  -------------------------------------------------------------------------
class coremodule
{
public:
					 /// Constructor. Read the metadata.
					 /// \param mpath The path to the module directory.
					 /// \param mname The name of the module.
					 coremodule (const string &mpath, const string &mname,
					 			 class moduledb *pparent);
					 
					 /// Destructor.
					~coremodule (void);
					
					 /// Perform the module's verify command, which
					 /// should perform a reasonable check to see if
					 /// the module will be able to perform its
					 /// function.
	bool			 verify (void);
	
					 /// Perform an action command.
					 /// \param command Action-type (create,update,delete)
					 /// \param classname The class name.
					 /// \param param The session and parameter data.
					 /// \param returndata Return data, if provided
					 ///        by the module.
	corestatus_t	 action (const statstring &command,
							 const statstring &classname,
							 const value &param,
							 value &returndata);
	
					 /// Report resource usage for a specific instance
					 /// of the module's main class. Returns a list
					 /// of resource names and units used.
					 /// \param id The id of the object.
					 /// \param month The month to report for, 0 for
					 ///              the current month.
					 /// \param year The year to report for, 0 for
					 ///             the current year.
	value			*reportusage (const statstring &id,
								  int month = 0,
								  int year = 0);
	
					 /// Get a list of total available resources.
					 /// (dormant)
	value			*getresources (void);
	
					 /// If applicable, gathers the running system
					 /// configuration from the actual host system.
					 /// \return value object in the following format:
					 /// \verbinclude getconfig.format
	value			*getcurrentconfig (void);
	
					 /// If, on start-up, opencore detects an update of the
					 /// module version, it will run an "updateok" call
					 /// past the module. A negative result on this from
					 /// the module will stop opencore from starting.
	bool			 updateok (int currentversion);
	
	void             getcredentials(value &creds);
    void             setcredentials(const value &creds);
    
    string			 name; ///< The module's name.
	string			 path; ///< Path to the module.
	statstring		 primaryclass; ///< Track primary class. \todo Do we need this?
	value			 meta; ///< The module.xml data.
					 ///{
					 /// Linked list pointers.
	coremodule		*next, *prev;
					 ///}
	classdict		 classes; ///< Dictionary of the module's coreclass objects.
	classdict		 classesuuid; ///< Coreclass objects indexed by uuid.
	value			 languages; ///< Index of supported languages.
	statstring		 apitype; ///< API type (from meta)
	string			 license; ///< License (from meta)
	string			 author; ///< Author (from meta)
	string			 url; ///< URL (from meta)
	class moduledb	&mdb;
	
protected:
    lock<int>        serlock; ///< Module access serialization lock.
};

#endif
