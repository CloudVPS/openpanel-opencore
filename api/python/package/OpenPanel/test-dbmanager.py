from OpenPanel.coreclient import CoreSession
from OpenPanel.exception import CoreException

import random, string

def run(c):
    """run dbmanager tests. pass a logged-in openadmin coreclient instance."""
    prefix = (''.join(random.choice(string.letters) for i in xrange(6))).lower()+'-'
    dom1 = prefix+'dom1.example.com'
    doms = c.rpc.getrecords(classid="Domain")
    if dom1 in doms['body']['data'].get('Domain',{}).keys():
        print "found Domain %s before create" % dom1
        return False
    dom1id = c.rpc.create(classid="Domain", objectid=dom1)['body']['data']['objid']
    print 'created Domain %s (%s)' % (dom1, dom1id)
    
    doms = c.rpc.getrecords(classid="Domain")
    
    if dom1 not in doms['body']['data'].get('Domain',{}).keys():
        print "did not find Domain %s after create" % dom1
        return False

    doms = c.rpc.getrecords(classid="Domain")
    if doms['body']['data']['Domain'][dom1]['uuid'] != dom1id:
        print "UUID mismatch for %s (%s!=%s)" % (
            dom1,
            doms['body']['data']['Domain'][dom1]['uuid'])
        return False
    
    c.rpc.delete(classid="Domain", objectid=dom1)
    print "deleted Domain %s by name" % dom1
    
    doms = c.rpc.getrecords(classid="Domain")
    if dom1 in doms['body']['data'].get('Domain',{}).keys():
        print "found Domain %s after delete/before new create" % dom1
        return False
    dom1id2 = c.rpc.create(classid="Domain", objectid=dom1)['body']['data']['objid']
    print 'created Domain %s (%s)' % (dom1, dom1id2)
    if dom1id == dom1id2:
        print 'UUID reuse detected'
        return False
    
    doms = c.rpc.getrecords(classid="Domain")
    
    if dom1 not in doms['body']['data'].get('Domain',{}).keys():
        print "did not find Domain %s after create" % dom1
        return False

    doms = c.rpc.getrecords(classid="Domain")
    if doms['body']['data']['Domain'][dom1]['uuid'] != dom1id2:
        print "UUID mismatch for %s (%s!=%s)" % (
            dom1,
            doms['body']['data']['Domain'][dom1]['uuid'])
        return False
    
    c.rpc.delete(classid="Domain", objectid=dom1id2)
    print "deleted Domain %s by uuid" % dom1
    
    doms = c.rpc.getrecords(classid="Domain")
    if dom1 in doms['body']['data'].get('Domain',{}).keys():
        print "found Domain %s after delete" % dom1
        return False

    dom1id3 = c.rpc.create(classid="Domain", objectid=dom1)['body']['data']['objid']
    print 'created Domain %s (%s)' % (dom1, dom1id)
    success=False
    try:
        dom1id3b = c.rpc.create(classid="Domain", objectid=dom1)['body']['data']['objid']
    except CoreException:
        success=True
    print 'create Domain %s correctly refused' % (dom1)

if __name__ == '__main__':
    c = CoreSession()
    c.login()  # will only work as root or openadmin
    run(c)