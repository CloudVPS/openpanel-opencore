# This file is part of the OpenPanel API
# OpenPanel is free software: you can redistribute it and/or modify it 
# under the terms of the GNU Lesser General Public License as published by the
# Free Software Foundation, using version 3 of the License.
#
# Please note that use of the OpenPanel trademark may be subject to additional 
# restrictions. For more information, please visit the Legal Information 
# section of the OpenPanel website on http://www.openpanel.com/

try:
        import simplejson as json
except:
        import json

from OpenPanel import error
from OpenPanel.exception import CoreException
import sys, os, traceback

class modapirequest:
    pass

class modulecallwrapper(object):
    """every valid opencore api command should have a method in this class"""
    def __init__(self, workerclass, req):
        super(modulecallwrapper, self).__init__()
        self.worker = workerclass(req)
        self.req = req

    def listobjects(self):
        return self.worker.listobjects(
          parent=self.req.fulltree["OpenCORE:Session"]["parentid"],
          count=int(str(self.req.fulltree["OpenCORE:Session"]["count"])),
          offset=int(str(self.req.fulltree["OpenCORE:Session"]["offset"]))
        )

    def create(self):
        return self.worker.create(
          objectid=self.req.objectid,
          object=self.req.fulltree[self.req.classid],
          tree=self.req.fulltree
        )

    def update(self):
        return self.worker.update(
            objectid=self.req.objectid,
            object=self.req.fulltree[self.req.classid],
            tree=self.req.fulltree
        )

    def delete(self):
        return self.worker.delete(
          objectid=self.req.objectid,
          tree=self.req.fulltree
        )

class panelclass(object):
    """docstring for panelclass"""
    def __init__(self, req):
        super(panelclass, self).__init__()
        self.req = req
        
class panelmodule(object):
    """docstring for panelmodule"""
    def __init__(self):
        super(panelmodule, self).__init__()
        self.modname = self.__class__.__name__[:-6] # strip 'Module' suffix
        
    def getrequest(self, f = sys.stdin):
        request = modapirequest()
        size = int(f.readline())
        requestjson = f.read(size)
    
        tree = json.loads(requestjson)
        request.fulltree = tree
        request.command = str(tree["OpenCORE:Command"])

        if request.command == "getconfig":
            return request
        if request.command == "listobjects":
            request.context = str(tree["OpenCORE:Session"]["parentid"])
        else:
            request.context = str(tree["OpenCORE:Context"])
        request.classid = str(tree["OpenCORE:Session"]["classid"])

        try:
            request.objectid = str(tree["OpenCORE:Session"]["objectid"])
        except KeyError:
            request.objectid = ""
    
        request.fulltree = tree
    
        return request

    def sendresult(self, code, text = "", extra=None):
         # TODO: handle extra data (getconfig etc.)
         # TODO: convince xmltramp to do our writing too

         # TODO: this naming difference is silly
         if code == 0:
             text = "OK"

         res= {
             'OpenCORE:Result':
               {
                 'error': code,
                 'message': text
               }
           }
         if(extra):
             res.update(extra)
         jres=json.dumps(res)

         print "%s\n%s" % (len(jres), jres)
         sys.exit(0)         # TODO: replace the whole shebang with an exception-based implementation

    def getworkerclass(self, pclass):
        c = self
        try:
            for s in pclass.split(":"):
                c = getattr(c, s)
        except AttributeError:
            self.sendresult(error.ERR_MODULE_WRONGCLASS)

        return c

    def run(self):
        try:
            os.chdir(os.path.join("/var/openpanel/conf/staging", self.modname))
        
            self.req = self.getrequest()
        
            if self.req.command == "getconfig":
                self.sendresult(0, "OK", extra=self.getconfig())
                return
            
            if self.req.command == "updateok":
            	if self.updateok(self.fulltree["OpenCORE:Session"]["currentversion"]):
            		self.sendresult(0, "OK")
            	else:
            		self.sendresult(error.ERR_MODULE_UPDATE, "Cannot update")

            workerclass = self.getworkerclass(self.req.classid)
            wrapper = modulecallwrapper(workerclass, self.req)
            worker = getattr(wrapper, self.req.command)
            result = worker()
            self.sendresult(0, "OK", result)
        except:
            try:
                self.sendresult(error.ERR_MODULE_FAILURE, ''.join(traceback.format_exception(*sys.exc_info())))
            except:
                sys.__excepthook__(*sys.exc_info())

