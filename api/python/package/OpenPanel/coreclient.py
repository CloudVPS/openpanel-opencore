from ext import simplejson
from OpenPanel.exception import CoreException

import httplib, socket
import threading, thread, time

__all__ = ['CoreException','CoreSession','CoreRPCClient']

# TODO: create a CoreObject class that enforces attributes as far as possible,
# checks type of parent object, basically all or at least most of the checks
# opencore would do too

class UHTTPConnection(httplib.HTTPConnection):
    """Subclass of Python library HTTPConnection that uses a unix-domain socket.
    """

    def __init__(self, path):
        httplib.HTTPConnection.__init__(self, 'localhost')
        self.path = path

    def connect(self):
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        sock.connect(self.path)
        self.sock = sock
        
class PingerThread(threading.Thread):
    """The PingerThread class."""
    def __init__(self, rpc):
        threading.Thread.__init__(self)
        self.rpc = rpc
        self.setDaemon(1)
        self.start()
    
    def run(self):
        while True:
            if self.rpc._sessionid:
                self.rpc.ping()
            time.sleep(60)

class CoreRPCClient(object):
    """The CoreRPCClient class."""
    def __init__(self, sessionid=None, host=None):
        if host:
            self._conn = httplib.HTTPSConnection(host, 4089)
        else:
            self._conn = UHTTPConnection("/var/opencore/sockets/opencore.sock")
            
        self._connmutex = thread.allocate_lock()
        self._ser = simplejson.dumps
        self._unser = simplejson.loads
        self._sessionid = sessionid
        self._pinger = PingerThread(self)
        
    def __getattr__(self, attr):
        def wrapper(**args):
            return self._call(attr, args)
        return wrapper
    
    def _call(self, method, args):
        # print "calling method %s with arg %s" % (method, arg)
        req = {}
        req["header"] = {}
        req["header"]["command"] = method
        if self._sessionid != None:
            req["header"]["session_id"] = self._sessionid
            
        req["body"] = args
        
        headers = {}
        headers["Content-type"] = "application/json"
        headers["X-OpenCORE"] = "coreclient.py"
        
        try:
            try:
                t1 = time.time()
                self._connmutex.acquire()
                self._conn.request("POST", "/json", self._ser(req), headers)
                response = self._conn.getresponse()
                if method != "ping": # concurrency hack!
                    self.lastreqduration = time.time() - t1
                result = self._unser(response.read())
            except:
                self._conn.close()
                raise
        finally:
            self._connmutex.release()
    
        # print result
        try:
            self._sessionid = result["header"]["session_id"]
            # print "grabbed sessionid %s" % self._sessionid
        except KeyError:
            pass
        
        if result["header"]["errorid"]:
            raise CoreException, (result["header"]["errorid"],result["header"]["error"])
            
        return result
        
class CoreSession(object):
    """The CoreSession class."""
    def __init__(self, sessionid=None, host=None):
        self.rpc = CoreRPCClient(sessionid, host)

    def login(self, user=None, password=None):
        if user:
            return self.rpc.bind(classid="User", id=user, data={"id":password})
        else:
            return self.rpc.bind(classid="User")

    def createobject(self, **args):
         return self.rpc.create(**args)["body"]["data"]["objid"]

    def updateobject(self, **args):
         self.rpc.update(**args)

    def deleteobject(self, objectid):
         self.rpc.delete(objectid=objectid)

if __name__ == "__main__":
    import pprint
    c = CoreSession()
    c.login("admin@example.net", "foobar")
    pprint.pprint(c.rpc.getrecords(classid="Domain", parentid=""))
