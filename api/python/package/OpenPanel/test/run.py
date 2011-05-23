# This file is part of the OpenPanel API
# OpenPanel is free software: you can redistribute it and/or modify it 
# under the terms of the GNU Lesser General Public License as published by the
# Free Software Foundation, using version 3 of the License.
#
# Please note that use of the OpenPanel trademark may be subject to additional 
# restrictions. For more information, please visit the Legal Information 
# section of the OpenPanel website on http://www.openpanel.com/

from OpenPanel.coreclient import CoreSession
from OpenPanel.exception import CoreException
import OpenPanel.test.modules

import random, string, imp, os
import logging

class Tester(object):
    def __init__(self, c):
        self.conn = c
        # FIXME: .lower() should go, but currently breaks things ;)
        self.prefix = (''.join(random.choice(string.letters) for i in xrange(6))).lower()+'-'
        self.username = ''.join(random.choice(string.letters) for i in xrange(6)).lower()
        self.password = ''.join(random.choice(string.letters) for i in xrange(6))
        
        self.modules = dict()
        self.order = []
        self.failed = []

        logging.basicConfig(level=logging.DEBUG)
        self.mainlogger = logging.getLogger("openpanel-test")

        self.mainlogger.info('Prefix=%s, Username=%s, Password=%s' % (self.prefix, self.username, self.password))
        
    def fail(self, title, desc):
        self.failed.append((title,desc))
        self.logger.error("test %s failed: %s" % (title, desc))
        
    def run(self):
        self.gatherModuleInfo()
        self.mainlogger.debug("modules=%s order=%s" % (self.modules, self.order))
        
        for module in self.order:
            self.mainlogger.info("testing module %s" % module)
            self.logger = logging.getLogger(module)
            self.modules[module].test(self)

        for module in reversed(self.order):
            self.mainlogger.debug("cleaning up module %s" % module)
            self.logger = logging.getLogger(module)
            self.modules[module].cleanup(self)
    
        if(self.failed):
            self.mainlogger.critical("%d tests failed" % len(self.failed))
        else:
            self.mainlogger.info("all tests passed!")
        
        # if not self.testDBManagerDomain():
        #     print "FAIL testDBManagerDomain"
        # if not self.testPostfixCourier():
        #     print "FAIL testPostfixCourier"
        # if not self.DBManagerCleanup():
        #     print "FAIL DBManagerCleanup"
    
    def gatherModuleInfo(self):
        modules = self.conn.rpc.listmodules()['body']['data']['modules']
        prereqs = dict()
        for module in modules:
            name = module.rsplit('.',1)[0]
            self.mainlogger.debug('attempting import of module %s' % name)
            try:
                info = imp.find_module('test',[os.path.join("/var/openpanel/modules",module,"tests")])
                self.modules[name]=imp.load_module('OpenPanel.test.modules.'+name,info[0],info[1],info[2])
                prereqs[name] = getattr(self.modules[name],'requires',[])

            except ImportError:
                pass
        
        import collections

        def c3_linearize(d):
            def c3_linearize_keyfunc(k):
                tree = []
                stack = collections.deque([k])
                while stack:
                    c = stack.popleft()
                    tree.append(c)
                    for dep in d[c]:
                        stack.append(dep)
                result = []
                seen = set()
                for c in reversed(tree):
                    if c in seen:
                        continue
                    seen.add(c)
                    result.append(c)
                return result
            return c3_linearize_keyfunc

        self.order = sorted(prereqs, key=c3_linearize(prereqs))
        

if __name__ == '__main__':
    c = CoreSession()
    c.login()  # will only work as root or openadmin
    t = Tester(c)
    t.run()
