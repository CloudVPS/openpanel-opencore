# This file is part of OpenPanel - The Open Source Control Panel
# The Grace library is free software: you can redistribute it and/or modify it 
# under the terms of the GNU General Public License as published by the Free 
# Software Foundation, either version 3 of the License.
#
# Please note that use of the OpenPanel trademark may be subject to additional 
# restrictions. For more information, please visit the Legal Information 
# section of the OpenPanel website on http://www.openpanel.com/

from OpenPanel import coreclient
import random, sys, time, traceback, os, optparse

randominstance = random.Random()

class PanelTestException(Exception):
    def __init__(self, s, detail):
        self.s = s
        self.detail = detail

    def __str__(self):
        return "%s, details: %s" % (self.s, self.detail)

def randomdomainpart():
    return "".join(map(chr,[randominstance.choice(range(97,97+26)) for i in range(1,8)]))

def cmdtolist(cmd):
    return os.popen(cmd).readlines()

def runtests(deleteuser=True):
    domain = "%s.%s.fake" % (randomdomainpart(), randomdomainpart())
    domainalias = "%s.%s.fake" % (randomdomainpart(), randomdomainpart())
    username = randomdomainpart()
    email = "%s@%s.%s.fake" % (randomdomainpart(), randomdomainpart(), randomdomainpart())
    password = randomdomainpart()

    print "%% Running testsuite with topdomain %s, alias domain %s, username %s, password %s" % (domain, domainalias, username, password)

    c = coreclient.CoreSession()
    c.login()
    print "[%s seconds] (login)" % (c.rpc.lastreqduration)

    print "%% Creating user %s" % (username)
    userid = c.createobject(classid="User", objectid=username, data={"password":password, "name_customer":"paneltesting", "emailaddress":email})
    print "[%s seconds] %s" % (c.rpc.lastreqduration, userid)

    print "% Logging in with new creds"
    c.login(username, password)
    print "[%s seconds] (login)" % (c.rpc.lastreqduration)

    # not allowed, so not tested; TODO: perhaps should test it and expect an exception.
    # print "% Creating shell for User (should he even be allowed to do so on himself?)"
    # shellid = c.createobject(classid="SSH:Shell", parentid=userid, objectid="shell", data={"shell":"/bin/bash"})
    # print "[%s seconds] %s" % (c.rpc.lastreqduration, shellid)
    # 
    # print "% Changing shell for User (should he even be allowed to do so on himself?)"
    # shellid = c.updateobject(classid="SSH:Shell", parentid=userid, objectid="shell", data={"shell":"/bin/sh"})
    # print "[%s seconds] %s" % (c.rpc.lastreqduration, shellid)

    print "% Creating toplevel Domain object"
    domainid = c.createobject(classid="Domain", parentid="", objectid=domain)
    print "[%s seconds] %s" % (c.rpc.lastreqduration, domainid)

    cmdres = cmdtolist("dig +short a www.%s @localhost" % domain)
    if cmdres != []:
        raise PanelTestException, ("unexpected non-empty dig result", cmdres)

    print "% Creating Domain:Alias object"
    domainaliasid = c.createobject(classid="Domain:Alias", parentid=domainid, objectid=domainalias)
    print "[%s seconds] %s" % (c.rpc.lastreqduration, domainaliasid)

    print "% Creating DNSDomain:Master object"
    dnsdomainid = c.createobject(classid="DNSDomain:Master", parentid=domainid, data={"ttl":300}, objectid=domain)
    print "[%s seconds] %s" % (c.rpc.lastreqduration, dnsdomainid)
    cmdres = cmdtolist("dig +short a www.%s @localhost www.%s @localhost" % (domain, domainalias))
    if cmdres == []:
        raise PanelTestException, ("unexpected non-IP dig result", cmdres)
    
    print "% Deleting+re-creating DNSDomain:Master object to test metaid uniqueness with prototype"
    c.deleteobject(dnsdomainid)
    cmdres = cmdtolist("dig +short a www.%s @localhost www.%s @localhost" % (domain, domainalias))
    if cmdres != []:
        raise PanelTestException, ("unexpected non-empty dig result", cmdres)

    dnsdomainid = c.createobject(classid="DNSDomain:Master", parentid=domainid, data={"ttl":300}, objectid=domain)
    print "[%s seconds] %s" % (c.rpc.lastreqduration, dnsdomainid)
    cmdres = cmdtolist("dig +short a www.%s @localhost www.%s @localhost" % (domain, domainalias))
    if cmdres == []:
        raise PanelTestException, ("unexpected non-IP dig result", cmdres)

    print "% Creating DNSDomain:A object"
    time.sleep(1) # serial generation is per-second
    dnsdomainrecordid = c.createobject(classid="DNSDomain:A", parentid=dnsdomainid, data={"name":"flierp", "address":"192.168.2.2"})
    print "[%s seconds] %s" % (c.rpc.lastreqduration, dnsdomainrecordid)
    cmdres = cmdtolist("dig +short a flierp.%s @localhost flierp.%s @localhost" % (domain, domainalias))
    if cmdres != ["192.168.2.2\n", "192.168.2.2\n"]:
        raise PanelTestException, ("unexpected non-192.168.2.2 dig result", cmdres)

    cmdres = cmdtolist("/usr/sbin/postmap -q %s /etc/postfix/openpanel/virtual_mailbox_domains" % domain)
    if cmdres != []:
        raise PanelTestException, ("unexpected non-empty postmap result", cmdres)

    cmdres = cmdtolist("/usr/sbin/postmap -q %s /etc/postfix/openpanel/virtual_mailbox_domains" % domainalias)
    if cmdres != []:
        raise PanelTestException, ("unexpected non-empty postmap result", cmdres)

    print "% Creating Mail object"
    mailid = c.createobject(classid="Mail", parentid=domainid, objectid=domain)
    print "[%s seconds] %s" % (c.rpc.lastreqduration, mailid)
    cmdres = cmdtolist("/usr/sbin/postmap -q %s /etc/postfix/openpanel/virtual_mailbox_domains" % domain)
    if cmdres != ["VIRTUAL\n"]:
        raise PanelTestException, ("unexpected postmap result", cmdres)

    cmdres = cmdtolist("/usr/sbin/postmap -q %s /etc/postfix/openpanel/virtual_mailbox_domains" % domainalias)
    if cmdres != ["VIRTUAL\n"]:
        raise PanelTestException, ("unexpected postmap result", cmdres)

    print "% Deleting+re-creating Mail object to test metaid uniqueness without prototype"
    c.deleteobject(mailid)
    cmdres = cmdtolist("/usr/sbin/postmap -q %s /etc/postfix/openpanel/virtual_mailbox_domains" % domain)
    if cmdres != []:
        raise PanelTestException, ("unexpected non-empty postmap result", cmdres)
    cmdres = cmdtolist("/usr/sbin/postmap -q %s /etc/postfix/openpanel/virtual_mailbox_domains" % domainalias)
    if cmdres != []:
        raise PanelTestException, ("unexpected non-empty postmap result", cmdres)
    mailid = c.createobject(classid="Mail", parentid=domainid, objectid=domain)
    print "[%s seconds] %s" % (c.rpc.lastreqduration, mailid)
    cmdres = cmdtolist("/usr/sbin/postmap -q %s /etc/postfix/openpanel/virtual_mailbox_domains" % domain)
    if cmdres != ["VIRTUAL\n"]:
        raise PanelTestException, ("unexpected postmap result", cmdres)
    cmdres = cmdtolist("/usr/sbin/postmap -q %s /etc/postfix/openpanel/virtual_mailbox_domains" % domainalias)
    if cmdres != ["VIRTUAL\n"]:
        raise PanelTestException, ("unexpected postmap result", cmdres)

    print "% Creating Mail box"
    mailbox = "%s@%s" % (randomdomainpart(), domain)
    mailboxid = c.createobject(classid="Mail:Box", objectid=mailbox, parentid=mailid, data={"password":randomdomainpart()})
    print "[%s seconds] %s" % (c.rpc.lastreqduration, mailboxid)
    cmdres = cmdtolist("/usr/sbin/postmap -q %s /etc/postfix/openpanel/virtual_mailbox" % mailbox)
    if cmdres != ["virtual\n"]:
        raise PanelTestException, ("unexpected postmap result", cmdres)

    print "% Creating Alias box"
    aliasbox = "%s@%s" % (randomdomainpart(), domain)
    aliasboxid = c.createobject (classid="Mail:Alias", objectid=aliasbox, parentid=mailid)
    print "[%s seconds] %s" % (c.rpc.lastreqduration, aliasboxid)
    
    print "% Creating Alias destination"
    destbox = "%s@othersite.local" % (randomdomainpart())
    destboxid = c.createobject (classid="Mail:Destination", parentid=aliasboxid, data={"address":destbox})
    print "[%s seconds] %s" % (c.rpc.lastreqduration, destboxid);
    
    print "% Testing bogus destination"
    try:
        bogusid = c.createobject (classid="Mail:Destination", parentid=aliasboxid, data={})
    except:
        bogusid = ""
        
    if (bogusid != ""):
        raise PanelTestException ("opencore rpc allowed a bogus Mail:Destination record with no data", bogusid)
    
    print "% Enabling Amavis for Mail domain"
    amavisid = c.createobject(classid="Mail:Amavis", parentid=mailid, objectid="amavis", data={"enabled":"true"})
    print "[%s seconds] %s" % (c.rpc.lastreqduration, amavisid)

    print "% Creating vhost in domain"
    vhostname = "www.%s" % domain
    vhostid = c.createobject(classid="Domain:Vhost", objectid=vhostname, parentid=domainid, data={"admin":"peter@example.net"})
    print "[%s seconds] %s" % (c.rpc.lastreqduration, vhostid)

    print "% Creating ftp user on vhost"
    ftpuser = "%s@%s" % (randomdomainpart(),vhostname)
    vhostftpid = c.createobject(classid="FTP:User", objectid=ftpuser, parentid=vhostid, data={"password":randomdomainpart()})
    print "[%s seconds] %s" % (c.rpc.lastreqduration, vhostftpid)

    print "% Enabling webstats on vhost"
    awstatsid = c.createobject(classid="Vhost:AWStats", objectid="webstats", parentid=vhostid, data={"path":"/stats"})
    print "[%s seconds] %s" % (c.rpc.lastreqduration, awstatsid)

    print "% Creating vhost -at- domain"
    vhost2name = "blog.%s" % domain
    vhost2id = c.createobject(classid="Domain:Vhost", objectid=vhost2name, parentid=domainid, data={"admin":"peter@example.net"})
    print "[%s seconds] %s" % (c.rpc.lastreqduration, vhostid)

    print "% Creating ftp user on vhost"
    ftp2user = "%s@%s" % (randomdomainpart(),vhost2name)
    vhost2ftpid = c.createobject(classid="FTP:User", objectid=ftp2user, parentid=vhost2id, data={"password":randomdomainpart()})
    print "[%s seconds] %s" % (c.rpc.lastreqduration, vhost2ftpid)

    mysqldbname = randomdomainpart()
    print "%% Creating MySQL database %s" % mysqldbname
    mysqldbid = c.createobject(classid="MySQL:Database", objectid=mysqldbname)
    print "[%s seconds] %s" % (c.rpc.lastreqduration, mysqldbid)

    print "% Creating same-name user on mysql db"
    mysqluserid = c.createobject(classid="MySQL:DBUser", objectid=mysqldbname, parentid=mysqldbid, data={"password":randomdomainpart(), "permissions":"read-write"})
    print "[%s seconds] %s" % (c.rpc.lastreqduration, mysqluserid)

    print "% Checking quota reporting"
    quotaresult = c.rpc.getrecords(classid="OpenCORE:Quota",parentid=userid)["body"]["data"]["OpenCORE:Quota"]
    quotaexpected = {'Domain': 1, 'FTP:User': 2, 'Domain:Vhost': 2, 'Domain:Alias': 1, 'Mail:Box': 1, 'Mail:Alias': 1, 'Mail:Amavis': 1, 'Mail': 1, 'Vhost:AWStats': 1, 'Mail:Destination': 1}

    quotarep = dict()
    for k in quotaresult:
        quotarep[quotaresult[k]["metaid"]] = quotaresult[k]["usage"]

    for k in quotaexpected:
        if quotarep[k] != quotaexpected [k]:
            raise PanelTestException ("quota mismatch", "%s %d should be %d" % (k, quotarep[k], quotaexpected[k]))

    if deleteuser:
        print "% Attempting to delete User that owns objects"
        c.login()
        print "[%s seconds] (login)" % (c.rpc.lastreqduration)
        c.deleteobject(userid)
    
    print "%%% Checking database consistency"
    dbverify = cmdtolist("/usr/bin/sqlite3 /var/opencore/db/panel/panel.db < /usr/src/wharf/work/opencore/sqlite/VERIFY")
    dbok = True
    for l in dbverify:
        print l.strip()
        if not l.startswith("%"):
            dbok = False

    if not dbok:
        raise PanelTestException ("database inconsistency detected", "")

    print "%%% Remember to run the testsuite again, this may uncover singleton bugs!"

    # automate this warning based on a module or class list fetched from opencore
    print "%%% Note!!! Networking, iptables, softwareupdate have not been tested!"


if __name__ == "__main__":
    parser = optparse.OptionParser(usage="%prog [options]")
    parser.add_option("-n", "--no-delete-user", dest="deleteuser", action="store_false", default=True, help="don't delete User after testing")

    (options, args) = parser.parse_args()

    runtests(options.deleteuser)
