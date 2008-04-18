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

#ifndef _OPENCORE_ERROR_H
#define _OPENCORE_ERROR_H 1

// This list of error definitions is leading for all of opencore, authd,
// modules, rpc.
//
// Errors are 16 bits; 0xSSEE in hex. SS denotes the subsystem, allocated
// with a trailing zero for now, to leave ample room for expansion beyond 256
// errors in any subsystem
//
// 0x0000 denotes success; nothing else does.
//
// errors within a subsystem number from 01; use 00 for a non-specific failure
// within that subsystem
//
// The comments after each error code are copied to resources.xml as the
// error text.

// looks weird, can't help that
#define ERR_OK							0x0000 // OK, no error

// core subsystem = 0x00.. - try to put errors in other places before
// considering core
#define ERR_UNKNOWN						0x0001 // Unknown error

// moduledb subsystem = 0x10..
#define ERR_MDB_UNKNOWN_CLASS 			0x1001 // Unknown class
#define ERR_MDB_ACTION_FAILED 			0x1002 // Action failed in module
#define ERR_MDB_INVALID_RETURN 			0x1003 // Invalid module return
#define ERR_MDB_CLASS_MISMATCH			0x1004 // Session class mismatch
#define ERR_MDB_OWNER_MISMATCH			0x1005 // Session owner mismatch
#define ERR_MDB_AUTH_FAILED 			0x1006 // Object authentication failed
#define ERR_MDB_MISSING_CONTEXT			0x1007 // Missing required session context
#define ERR_MDB_KEY_CONFLICT			0x1008 // Object key conflict
#define ERR_MDB_REQUIRE_KEY				0x1009 // Class needs primary key for new object
#define ERR_MDB_MISSING_REQUIRED		0x1010 // Missing required object parameter
#define ERR_MDB_RECURSION				0x1011 // Object recursion error

// dbmanager = 0x20xx
#define ERR_DBMANAGER_FAILURE			0x2000 // Failure in database backend
#define ERR_DBMANAGER_LOGINFAIL			0x2001 // Login failed
#define ERR_DBMANAGER_INITFAIL			0x2002 // Initialization failure
#define ERR_DBMANAGER_NOTFOUND			0x2003 // Object not found
#define ERR_DBMANAGER_NOPERM			0x2004 // Permission denied
#define ERR_DBMANAGER_EXISTS			0x2005 // Object already exists
#define ERR_DBMANAGER_INVAL				0x2006 // Invalid parameters
#define ERR_DBMANAGER_QUOTA				0x2007 // Over quota

// session = 0x30xx
#define ERR_SESSION_INVALID				0x3000 // Invalid session
#define ERR_SESSION_OBJECT_NOT_FOUND 	0x3001 // Object not found
#define ERR_SESSION_CLASS_UNKNOWN		0x3002 // Unknown class
#define ERR_SESSION_CRYPT				0x3003 // Error crypting field
#define ERR_SESSION_CRYPT_ORIG			0x3004 // Empty password not allowed for new object.
#define ERR_SESSION_INDEX				0x3005 // Manual index not allowed for this class
#define ERR_SESSION_VALIDATION			0x3006 // Input validation error
#define ERR_SESSION_NOINDEX				0x3007 // No index provided where required
#define ERR_SESSION_NOTALLOWED			0x3008 // Operation not allowed
#define ERR_SESSION_CHOWN				0x3009 // An object of this class could be created, but its ownership could not be changed to your chosen user.
#define ERR_SESSION_PARENTREALM			0x300a // Class has parent-related indexing constraints, but the parent-id could not be resolved.

// rpc-specific errors = 0x40xx
#define ERR_RPC_UNDEFINED				0x4001 // Undefined item or error
#define	ERR_RPC_INCOMPLETE  			0x4002 // Incomplete request
#define	ERR_RPC_INVALIDLOGIN 			0x4003 // User entered invalid login
#define	ERR_RPC_SERVERINTERNAL			0x4004 // Internal server error
#define	ERR_RPC_NOCONTEXT				0x4005 // No current context
#define	ERR_RPC_INVALIDCMD				0x4006 // Invalid command
#define ERR_RPC_NOSESSION				0x4007 // Command requires a valid session
#define ERR_RPC_METANOMODULENAME		0x4008 // Missing 'module' argument
#define ERR_RPC_METANOMODULEORFNAME		0x4008 // Missing 'module' or 'filename' argument

// authd errors = 0x50xx
#define ERR_AUTHD_FAILURE				0x5000 // Generic authd failure

// templating errors = 0x60xx
#define ERR_TEMPLATE_FAILURE			0x6000 // Generic template failure

// module errors = 0x70xx
#define ERR_MODULE_FAILURE				0x7000 // Generic module failure
#define ERR_MODULE_WRONGCLASS			0x7001 // Wrong class for module

// internal class errors = 0x80xx
#define ERR_ICLASS						0x8000 // Error in internal class handler
#define ERR_API							0x9000 // Error in module API

#endif
