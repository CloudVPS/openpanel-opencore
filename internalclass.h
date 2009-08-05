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
class InternalClass
{
public:
					 InternalClass (void);
	virtual			~InternalClass (void);
	
					 /// Should return an object list keyed
					 /// by uuid. All subclasses should override
					 /// this.
	virtual value	*listObjects (CoreSession *s,
								  const statstring &parentid);

					 /// Should return a specific object instance
					 /// in the same format as CoreSession::getObject().
					 /// If you don't override this method, the base
					 /// class will use your listObjects() and
					 /// filter it for the specific id.
	virtual value	*getObject (CoreSession *s,
								const statstring &parentid,
								const statstring &id);
					
					 /// If your CoreClass implements it, override
					 /// this method to handle a create event.
	virtual string	*createObject (CoreSession *s,
								   const statstring &parentid,
								   const value &withparam,
								   const statstring &withid="");
							   
					 /// If your CoreClass implements it, override
					 /// this method to handle a delete event.
	virtual bool	 deleteObject (CoreSession *s,
								   const statstring &parentid,
								   const statstring &withid);
	
					 /// If your CoreClass implements it, override
					 /// this method to handle an update event.
	virtual bool	 updateObject (CoreSession *s,
								   const statstring &parentid,
								   const statstring &withid,
								   const value &withparam);
	
					 /// Return last reported error string.
	const string	&error (void) { return err; }
	
protected:
					 /// Set an error condition.
					 /// \param s The error string.
	void			 setError (const string &s) { err = s; }
	
					 /// Translate a metaid/parentid combination
					 /// to a uuid that is guaranteed to be
					 /// static for the lifetime of this
					 /// instance. Use this function to generate
					 /// uuids in listObjects(), reuse them
					 /// in other methods through resolveUUID() or
					 /// metaid().
					 /// \param parentid The parent-id.
					 /// \param metaid The meta-id.
	const string	&getUUID (const statstring &parentid,
							  const statstring &metaid);
	
					 /// Resolve a raw uuid to its parent-/metaid.
					 /// \param uuid The uuid.
					 /// \return Reference to a 2 element array of
					 ///         [parentid,metaid].
	const value		&resolveUUID (const statstring &uuid);
	
					 /// Resolve a uuid to its metaid.
	const string	&getMetaID (const statstring &uuid);
	
					 /// Resolve a uuid to its parentid.
	const string	&getParentID (const statstring &uuid);
	
	value			 uuidmap;
	value			 metamap;
	string			 err;
};

typedef dictionary<InternalClass> InternalClassdb;

//  -------------------------------------------------------------------------
/// Implementation of the OpenCORE:Quota core class.
//  -------------------------------------------------------------------------
class QuotaClass : public InternalClass
{
public:
					 QuotaClass (void);
					~QuotaClass (void);
					
	value			*listObjects (CoreSession *s, const statstring &pid);
	
	value			*getObject (CoreSession *s,
								const statstring &parentid,
								const statstring &id);
								
	bool			 updateObject (CoreSession *s,
								   const statstring &parentid,
								   const statstring &withid,
								   const value &withparam);
};

//  -------------------------------------------------------------------------
/// Synthetic root singleton used to keep the
/// OpenCORE:ErrorLog and OpenCORE:ActiveSession objects.
//  -------------------------------------------------------------------------
class CoreSystemClass : public InternalClass
{
public:
					 CoreSystemClass (void);
					~CoreSystemClass (void);
					
	value			*listObjects (CoreSession *s, const statstring &pid);

protected:
	string			 sysuuid;
};

//  -------------------------------------------------------------------------
/// Impleentation of the OpenCORE:ActiveSession core class.
//  -------------------------------------------------------------------------
class SessionListClass : public InternalClass
{
public:
					 SessionListClass (void);
					~SessionListClass (void);
					
	value			*listObjects (CoreSession *s, const statstring &pid);
};

//  -------------------------------------------------------------------------
/// Implementation of the OpenCORE:ErrorLog CoreClass.
//  -------------------------------------------------------------------------
class ErrorLogClass : public InternalClass
{
public:
					 ErrorLogClass (void);
					~ErrorLogClass (void);
					
	value			*listObjects (CoreSession *s, const statstring &pid);
};

//  -------------------------------------------------------------------------
/// Implementation of the OpenCORE:ClassList CoreClass.
//  -------------------------------------------------------------------------
class ClassListClass : public InternalClass
{
public:
					 ClassListClass (void);
					~ClassListClass (void);
					
	value			*listObjects (CoreSession *s, const statstring &pid);
};


#endif
