// This file is part of OpenPanel - The Open Source Control Panel
// OpenPanel is free software: you can redistribute it and/or modify it 
// under the terms of the GNU General Public License as published by the Free 
// Software Foundation, using version 3 of the License.
//
// Please note that use of the OpenPanel trademark may be subject to additional 
// restrictions. For more information, please visit the Legal Information 
// section of the OpenPanel website on http://www.openpanel.com/

#include "mkmodulexml.h"
#include <grace/strutil.h>
#include <grace/xmlschema.h>
#include <grace/validator.h>

APPOBJECT(mkmodulexmlApp);

//  =========================================================================
/// Main method.
//  =========================================================================
int mkmodulexmlApp::main (void)
{
	value x;
	visitor<value> vis (x);
	bool insub = false;
	
	statstring distro = "unknown";

	value makeinc;
	makeinc.loadini ("makeinclude");
	if (! makeinc.exists ("CXXFLAGS"))
	{
		ferr.writeln ("WARN: No CXXFLAGS found in makeinclude");
		ferr.writeln ("      Distribution-specific macros will fail");
	}
	else
	{
		value cflags = strutil::splitspace (makeinc["CXXFLAGS"]);
		foreach (fl, cflags)
		{
			if (fl == "-D__FLAVOR_LINUX_REDHAT") distro = "redhat";
			if (fl == "-D__FLAVOR_LINUX_DEBIAN") distro = "debian";
		}
	}
	
	while (!fin. eof())
	{
		string line = fin.gets ();
		line.cropat ('#');
		string chkline = line.trim (" \t");
		if (chkline.strlen() == 0) continue;
		
		value v = strutil::splitquoted (line, '<');
		string attribvalue = v[1];
		statstring attribname = attribvalue.cutat (' ');
		attribvalue = attribvalue.trim (" \t");

		string v0 = v[0];
		v0 = v0.trim (" \t");
		v.clear ();
		
		string nodevalue, nodeidlist;
		
		for (int i=1; i<v0.strlen(); ++i)
		{
			if ((v0[i] == ':') &&
			    (! isalpha (v0[i-1])) &&
			    (! isdigit (v0[i-1])))
			{
				nodeidlist = v0.left (i);
				nodeidlist = nodeidlist.trim (" \t");
				i++;
				while ((i < v0.strlen()) && isspace (v0[i])) i++;
				if (i<v0.strlen()) nodevalue = v0.mid (i);
				break;
			}
		}
		
		if (! nodeidlist) nodeidlist = v0.trim (" \t");
		value nodeid = strutil::splitspace (nodeidlist);
		
		if (nodeid.count())
		{
			if (line.strstr (nodeid[0].sval()) == 0)
			{
				vis.root ();
				insub = false;
				foreach (node, nodeid)
				{
					vis.obj()[node.sval()];
					vis.enter (node.sval());
				}
			}
			else
			{
				if (nodeid.count() < 2)
				{
					nodeid = $("value")->$(nodeid[0]);
				}
				if (insub) vis.up ();
				insub = true;
				vis.obj()[nodeid[1]];
				vis.enter (nodeid[1]);
				vis.obj().type (nodeid[0]);
				vis.obj()("type") = nodeid[0];
			}
		}
		
		if (nodevalue) vis.obj() = nodevalue;
		if (attribname)
		{
			vis.obj()(attribname) = attribvalue;
		}
	}
	
	string corepath = "../opencore/rsrc";
	if (! fs.exists (corepath))
	{
		corepath = "/var/opencore/bin/opencore.app/Contents/Schemas";
	}
	if (! fs.exists (corepath))
	{
		corepath = "/usr/lib/opencore/schemas";
	}
	if (! fs.exists (corepath)) 
	{
	    corepath = ".";
	}
	
	corepath.strcat ("/com.openpanel.opencore.module");
	
	string schemapath = "%s.schema.xml" %format (corepath);
	string validatorpath = "%s.validator.xml" %format (corepath);
	
	xmlschema S (schemapath);
	validator V;
	if (! V.load (validatorpath))
	{
		ferr.writeln ("Error loading validator");
	}
	
	value out;
	
	value &refModule = x["module"][0];
	out["name"] = "%s.module" %format (refModule.id());
	out["uuid"] = refModule("uuid");
	out["version"] = refModule("version");
	out["license"] = refModule("license");
	out["author"] = refModule("author");
	out["url"] = refModule("url");
	if (refModule.attribexists ("requires")) out["requires"] = refModule("requires");
	
	value splitLanguages = strutil::splitspace (refModule("languages"));
	foreach (lang, splitLanguages)
	{
		if (lang.sval()) out["languages"](lang.sval()) = "";
	}
	
	out["implementation"]["apitype"] = refModule("apitype");
	out["implementation"]["getconfig"] = refModule("getconfig").bval();
	
	foreach (theclass, x["class"])
	{
		value listview;
		value required;
		value &outclass = out["classes"][theclass.id()];
		outclass.type ("class");
		foreach (attr, theclass.attributes())
		{
			if (attr.id() == "capabilities")
			{
				value capsplit = strutil::splitspace (attr);
				foreach (cap, capsplit)
				{
					outclass["capabilities"](cap) = true;
				}
			}
			else if (attr.id() == "listview")
			{
				value v = strutil::splitspace (attr.sval());
				foreach (nod, v)
				{
					listview[nod.sval()] = true;
				}
			}
			else if (attr.id() == "required")
			{
				value v = strutil::splitspace (attr.sval());
				foreach (nod, v)
				{
					required[nod.sval()] = true;
				}
			}
			else
			{
				outclass[attr.id()] = attr;
			}
		}
		
		foreach (P, theclass)
		{
			if (P("type") == "method")
			{
				P.rmattrib ("type");
				outclass["methods"][P.id()]("description") = P.sval();
				outclass["methods"][P.id()]("args") = P("args");
				outclass["methods"][P.id()].type ("method");
				continue;
			}
			value &outp = outclass["parameters"][P.id()];
			
			outp = P.sval();
			
			if (P.attribexists ("flags"))
			{
				value flags = strutil::splitspace (P("flags"));
				foreach (f, flags)
				{
					outp(f) = true;
				}
			}
			else
			{
				outp("enabled") = true;
				outp("visible") = true;
				outp("required") = true;
			}

			if (listview.count() && (! listview.exists (P.id())))
			{
				outp("clihide") = true;
			}
			
			if (required.count() && (! required.exists (P.id())))
			{
				outp.rmattrib ("required");
			}
						
			foreach (attr, P.attributes())
			{
				caseselector (attr.id())
				{
					incaseof ("flags") : break;
					incaseof ("regexp") :
						outp ("regexp") = attr; break;
					
					incaseof ("example") :
						outp ("example") = attr; break;
					
					incaseof ("default") :
						outp ("default") = attr.sval(); break;
					
					defaultcase :
						outp(attr.id()) = attr;
						break;
				}
			}
			
			outp.type ("p");
		}
	}
	
	foreach (E, x["enum"])
	{
		value &o = out["enums"][E.id()];
		o.type ("enum");
		foreach (V, E)
		{
			V.rmattrib ("type");
			if (! V.attribexists ("val")) V("val") = V.id();
			o[V.id()] = V;
		}
	}
	
	foreach (fop, x["authd"]["fileops"])
	{
		fop.rmattrib ("type");
		fop.type ("fileop");
		out["authdops"]["fileops"][fop.id()] = fop;
	}
	
	foreach (com, x["authd"]["commands"])
	{
		com.rmattrib ("type");
		out["authdops"]["commands"][com.id()] = com;
	}
	
	foreach (com, x["authd"]["commandclasses"])
	{
		com.rmattrib ("type");
		out["authdops"]["commandclasses"][com.id()] = com;
	}
	
	foreach (svc, x["authd"]["services"])
	{
		svc.rmattrib ("type");
		out["authdops"]["services"][svc.id()] = svc;
	}
	
	foreach (scr, x["authd"]["scripts"])
	{
		scr.rmattrib ("type");
		out["authdops"]["scripts"][scr.id()] = scr;
	}
	
	foreach (obj, x["authd"]["objects"])
	{
		obj.rmattrib ("type");
		out["authdops"]["objects"][obj.id()] = obj;
	}
	
	foreach (obj, x["module"][0])
	{
		if (obj("type") == "quota")
		{
			obj.rmattrib ("type");
			out["quotas"][obj.id()] = obj;
		}
		else
		{
			ferr.writeln ("Unknown module property: %s" %format (obj("type")));
		}
	}
	
	string asxml = out.toxml (value::nocompact, S);
	
	if (x["distro"].exists (distro))
	{
		foreach (macro, x["distro"][distro])
		{
			statstring marker = "%%%s%%" %format (macro.id());
			asxml.replace ($(marker,macro.sval()));
		}
	}
	
	value rback;
	rback.fromxml (asxml, S);
	
	
	string verr;
	if (! V.check (rback, verr))
	{
		ferr.writeln ("Schema error: %s" %format (verr));
		return 1;
	}
	
	fout.puts (asxml);
	return 0;
}

