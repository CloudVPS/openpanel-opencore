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

import random, string, subprocess, smtplib

# FIXME: migrating this code to py-unittest would probably be cool

class Tester(object):
    def __init__(self, c):
        self.conn = c
        # FIXME: .lower() should go, but currently breaks things ;)
        self.prefix = (''.join(random.choice(string.letters) for i in xrange(6))).lower()+'-'
        self.username = ''.join(random.choice(string.letters) for i in xrange(6)).lower()
        self.password = ''.join(random.choice(string.letters) for i in xrange(6))
        
        print 'Prefix: %s' % self.prefix
        print 'Username: %s' % self.username
        print 'Password: %s' % self.password
        
    def run(self):
        if not self.testDBManagerDomain():
            print "FAIL testDBManagerDomain"
        if not self.testPostfixCourier():
            print "FAIL testPostfixCourier"
        if not self.DBManagerCleanup():
            print "FAIL DBManagerCleanup"
    
    def postmapq(self, mapname, key):
        return subprocess.Popen([
            'postmap',
            '-q',
            key,
            '/etc/postfix/openpanel/'+mapname
            ], stdout=subprocess.PIPE, stderr=subprocess.PIPE).communicate()
    
    def smtpauthcheck(self, box, password):
        s = smtplib.SMTP()
        s.connect()
        try:
            print s.login(box,password)
        except smtplib.SMTPAuthenticationError:
            s.quit()
            return False
            
        s.quit()
        return True
    
    def testPostfixCourier(self):
        try:
            self.conn.rpc.classinfo(classid='Mail')
        except CoreException:
            print 'Mail class not found, skipping tests'
            return
    
        if self.postmapq('virtual_mailbox_domains', self.domain)[0]:
            print 'Mail domain already present, ERROR'
            return False
            
        mailuuid = c.rpc.create(classid="Mail", objectid=self.domain, parentid=self.domainuuid)['body']['data']['objid']
        print 'created Mail %s (%s)' % (self.domain, mailuuid)

        if self.postmapq('virtual_mailbox_domains', self.domain)[0] == 'VIRTUAL\n':
            print 'Mail %s spotted in postfix config' % (self.domain)
        else:
            print 'Mail %s missing in postfix config' % (self.domain)
            return False
            
        box=self.username+'@'+self.domain
        alias='a_'+self.username+'+alias@'+self.domain
        
        if self.smtpauthcheck(box, self.password):
            print 'smtp auth works before creation of box, ERROR'
            return False

        boxuuid = c.rpc.create(classid="Mail:Box", objectid=box, parentid=mailuuid, data=dict(
            password=self.password,
            allowrelay=True))['body']['data']['objid']
        print 'created Mail:Box %s (%s)' % (box, boxuuid)
    
        if not self.smtpauthcheck(box, self.password):
            print 'smtp auth does not work, ERROR'
            return False

        aliasuuid = c.rpc.create(classid="Mail:Alias", objectid=alias, parentid=mailuuid, data=dict(pridest='test@example.com'))['body']['data']['objid']
        c.rpc.delete(classid="Mail:Alias", objectid=aliasuuid)

        if self.postmapq('virtual_alias', alias)[1]:
            print 'create+delete of alias leaves entry behind, ERROR'
            return False
        
        return True
        
    def testDBManagerDomain(self):
        """run dbmanager tests. relies on Domain.module. leaves a Domain object behind."""
        dom1 = self.prefix+'dom1.example.com'
        self.domain = dom1
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
        self.domainuuid=dom1id3
        print 'created Domain %s (%s)' % (dom1, dom1id)
        success=False
        try:
            dom1id3b = c.rpc.create(classid="Domain", objectid=dom1)['body']['data']['objid']
        except CoreException:
            success=True
        print 'create Domain %s correctly refused' % (dom1)
        
        return True
    
    def DBManagerCleanup(self):
        c.rpc.delete(classid="Domain", objectid=self.domain)
        print "deleted Domain %s by name" % self.domain
        return True

if __name__ == '__main__':
    c = CoreSession()
    c.login()  # will only work as root or openadmin
    t = Tester(c)
    t.run()
