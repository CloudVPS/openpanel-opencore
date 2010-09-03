from xml.dom import minidom
from OpenPanel import error
from OpenPanel.ext import xmltramp
from OpenPanel.exception import CoreException
from xml.sax import saxutils
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

class panelclass(object):
    """docstring for panelclass"""
    def __init__(self, req):
        super(panelclass, self).__init__()
        self.req = req
        
class panelmodule(object):
    """docstring for panelmodule"""
    def __init__(self):
        super(panelmodule, self).__init__()
        self.modname = self.__class__.__name__
        
    def getrequest(self, f = sys.stdin):
        request = modapirequest()
        size = int(f.readline())
        requestxml = f.read(size)
    
        tree = xmltramp.parse(requestxml)
        request.command = str(tree["OpenCORE:Command"])
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

    def sendresult(self, code, text = "", extra=""):
         # TODO: handle extra data (getconfig etc.)
         # TODO: convince xmltramp to do our writing too

         # TODO: this naming difference is silly
         if code == 0:
             text = "OK"

         res = []
         res.append('<?xml version="1.0" encoding="UTF-8"?>')
         res.append( '<dict>')
         res.append(' <dict id="OpenCORE:Result">')
         res.append('  <integer id="error">%s</integer>' % (saxutils.escape(str(code))))
         res.append('  <string id="message">%s</string>' % (saxutils.escape(text)))
         res.append(' </dict>')
         res.append(extra)     # needs to be a dict at the right level!
         res.append('</dict>')
         response = "\n".join(res)
         print "%s\n%s" % (len(response), response)
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
            os.chdir(os.path.join("/var/opencore/conf/staging", self.modname))
        
            self.req = self.getrequest()
        
            workerclass = self.getworkerclass(self.req.classid)
            wrapper = modulecallwrapper(workerclass, self.req)
            worker = getattr(wrapper, self.req.command)
            self.sendresult(0, "OK", worker())
        except:
            try:
                self.sendresult(error.ERR_MODULE_FAILURE, saxutils.escape(''.join(traceback.format_exception(*sys.exc_info()))))
            except:
                sys.__excepthook__(*sys.exc_info())

