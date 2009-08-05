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

#ifndef _OPENCORE_INTERNALCLASS_H
#define _OPENCORE_INTERNALCLASS_H 1

#include "session.h"

#include <grace/value.h>
#include <grace/dictionary.h>

//  -------------------------------------------------------------------------
/// Base class for creating internal OpenCORE class impelementations.
/// This can be useful for exposing a class that needs internal knowledge
/// of OpenCORE's structures.
//  -------------------------------------------------------------------------
class internalclass
{
public:
					 internalclass (void);
	virtual			~internalclass (void);
	
					 /// Should return an object list keyed
					 /// by uuid. All subclasses should override
					 /// this.
	virtual value	*listObjects (coresession *s,
								  const statstring &parentid);

					 /// Should return a specific object instance
					 /// in the same format as coresession::getobject().
					 /// If you don't override this method, the base
					 /// class will use your listObjects() and
					 /// filter it for the specific id.
	virtual value	*getobject (coresession *s,
								const statstring &parentid,
								const statstring &id);
					
					 /// If your coreclass implements it, override
					 /// this method to handle a create event.
	virtual string	*createObject (coresession *s,
								   const statstring &parentid,
								   const value &withparam,
								   const statstring &withid="");
							   
					 /// If your coreclass implements it, override
					 /// this method to handle a delete event.
	virtual bool	 deleteObject (coresession *s,
								   const statstring &parentid,
								   const statstring &withid);
	
					 /// If your coreclass implements it, override
					 /// this method to handle an update event.
	virtual bool	 updateObject (coresession *s,
								   const statstring &parentid,
								   const statstring &withid,
								   const value &withparam);
	
					 /// Return last reported error string.
	const string	&error (void) { return err; }
	
protected:
					 /// Set an error condition.
					 /// \param s The error string.
	void			 seterror (const string &s) { err = s; }
	
					 /// Translate a metaid/parentid combination
					 /// to a uuid that is guaranteed to be
					 /// static for the lifetime of this
					 /// instance. Use this function to generate
					 /// uuids in listObjects(), reuse them
					 /// in other methods through resolveuuid() or
					 /// metaid().
					 /// \param parentid The parent-id.
					 /// \param metaid The meta-id.
	const string	&getuuid (const statstring &parentid,
							  const statstring &metaid);
	
					 /// Resolve a raw uuid to its parent-/metaid.
					 /// \param uuid The uuid.
					 /// \return Reference to a 2 element array of
					 ///         [parentid,metaid].
	const value		&resolveuuid (const statstring &uuid);
	
					 /// Resolve a uuid to its metaid.
	const string	&getmetaid (const statstring &uuid);
	
					 /// Resolve a uuid to its parentid.
	const string	&getparentid (const statstring &uuid);
	
	value			 uuidmap;
	value			 metamap;
	string			 err;
};

typedef dictionary<internalclass> internalclassdb;

//  -------------------------------------------------------------------------
/// Implementation of the OpenCORE:Quota core class.
//  -------------------------------------------------------------------------
class quotaclass : public internalclass
{
public:
					 quotaclass (void);
					~quotaclass (void);
					
	value			*listObjects (coresession *s, const statstring &pid);
	
	value			*getobject (coresession *s,
								const statstring &parentid,
								const statstring &id);
								
	bool			 updateObject (coresession *s,
								   const statstring &parentid,
								   const statstring &withid,
								   const value &withparam);
};

//  -------------------------------------------------------------------------
/// Synthetic root singleton used to keep the
/// OpenCORE:ErrorLog and OpenCORE:ActiveSession objects.
//  -------------------------------------------------------------------------
class coresystemclass : public internalclass
{
public:
					 coresystemclass (void);
					~coresystemclass (void);
					
	value			*listObjects (coresession *s, const statstring &pid);

protected:
	string			 sysuuid;
};

//  -------------------------------------------------------------------------
/// Impleentation of the OpenCORE:ActiveSession core class.
//  -------------------------------------------------------------------------
class sessionlistclass : public internalclass
{
public:
					 sessionlistclass (void);
					~sessionlistclass (void);
					
	value			*listObjects (coresession *s, const statstring &pid);
};

//  -------------------------------------------------------------------------
/// Implementation of the OpenCORE:ErrorLog coreclass.
//  -------------------------------------------------------------------------
class errorlogclass : public internalclass
{
public:
					 errorlogclass (void);
					~errorlogclass (void);
					
	value			*listObjects (coresession *s, const statstring &pid);
};

//  -------------------------------------------------------------------------
/// Implementation of the OpenCORE:ClassList coreclass.
//  -------------------------------------------------------------------------
class classlistclass : public internalclass
{
public:
					 classlistclass (void);
					~classlistclass (void);
					
	value			*listObjects (coresession *s, const statstring &pid);
};


#endif
