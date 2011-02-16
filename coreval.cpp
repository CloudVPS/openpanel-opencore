#include "coreval.h"
#include <grace/strutil.h>

APPOBJECT(corevalApp);

//  =========================================================================
/// Main method.
//  =========================================================================
int corevalApp::main (void)
{
	if (! argv["*"].count()) return 1;
	if (argv["*"][0] == "--loop")
	{
		argv["*"].rmindex (0);
		loop ();
		return 0;
	}

	value subst;
	subst["_"] = "_usc_";
	subst["."] = "_dot_";
	subst[":"] = "_sub_";
	subst["/"] = "_sla_";
	subst[" "] = "_spc_";
	subst["'"] = "_fqu_";
	subst["`"] = "_bqu_";
	subst["\""]= "_dqu_";
	subst["="] = "_equ_";
	subst["+"] = "_plu_";
	subst["-"] = "_min_";

	string valname;

	foreach (val, argv["*"])
	{
		string str = val;
		str.replace (subst);
		
		if (! valname.strlen())
		{
			valname = "CV_";
		}
		else
		{
			valname.strcat ("_val_");
		}
		valname.strcat (str);
	}
	
	fout.printf ("%s\n", env[valname].cval());
	
	return 0;
}

void corevalApp::loop (void)
{
	value v;
	value subst;
	subst["_dot_"] = ".";
	subst["_sub_"] = ":";
	subst["_sla_"] = "/";
	subst["_spc_"] = " ";
	subst["_fqu_"] = "'";
	subst["_bqu_"] = "`";
	subst["_dqu_"] = "\"";
	subst["_equ_"] = "=";
	subst["_plu_"] = "+";
	subst["_min_"] = "-";
	subst["_usc_"] = "_";
	
	foreach (val, env)
	{
		string val_id = val.id().sval();
		if (val_id.strncmp ("CV_", 3) == 0)
		{
			val_id = val_id.mid (3);
			value elements = strutil::split (val_id, "_val_");
			visitor<value> crsr (v);
			foreach (elm, elements)
			{
				string elmid = elm.sval();
				elmid.replace (subst);
				if (! crsr.exists (elmid)) crsr.obj()[elmid];
				crsr.enter (elmid);
			}
			
			if (crsr.obj().count() == 0) crsr.obj() = val.sval();
		}
	}

	visitor<value> vi (v);
	
	foreach (pathelm, argv["*"])
	{
		if (! vi.enter (pathelm.sval()))
		{
			return;
		}
	}
	
	string out;
	
	foreach (node, vi.obj())
	{
		fout.printf ("%s\n", node.id().cval());
	}
}
