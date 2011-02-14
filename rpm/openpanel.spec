%define 	pkgname	opencore

Name: 		openpanel-core
Version: 	1.0.7
Release: 	1%{?dist}
Summary:  	OpenPanel main files
License: 	GPLv3
Group: 		Applications/Internet
Source: 	%{pkgname}-%{version}.tar.bz2
Source1:	openpaneld.init
Patch0:		opencore-1.0.7-cert-path.patch
Requires:	/usr/sbin/adduser
Requires: 	openssl
Requires:	grace
Requires: 	openpanel-authd
BuildRequires: 	openssl
BuildRequires:	grace-devel
%if %{with suggest_tags}
Suggests: 	openpanel-authd
Suggests: 	openpanel-mod-user
Suggests: 	openpanel-cli
%endif
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
Obsoletes: 	openpanel-ssl

%description
The OpenPanel core

%package devel
Summary:        OpenPanel development files
Group:          Development/Libraries
Requires:       openpanel-core = %{version}

%description devel
OpenPanel module development libraries and includes

%package api-sh
Summary:        OpenPanel shell bindings
Group:          Application/Internet
Requires:       openpanel-core = %{version}

%description api-sh
OpenPanel shell bindings

%package api-python
Summary:        OpenPanel python bindings
Group:          Application/Internet
Requires:       openpanel-core = %{version}

%description api-python
OpenPanel python bindings

%package suggested
Summary:        OpenPanel with a suggested set of modules
Group:          Application/Internet
Requires:       openpanel-core = %{version}
Requires: 	openpanel-mod-user
Requires: 	openpanel-gui
Requires: 	openpanel-cli
Requires: 	openpanel-mod-domain
Requires: 	openpanel-mod-postfixcourier
Requires: 	openpanel-mod-apache2
Requires: 	openpanel-mod-apacheforward
Requires: 	openpanel-mod-dnsdomain
Requires: 	openpanel-mod-ftp
Requires: 	openpanel-mod-iptables
Requires: 	openpanel-mod-mysql
Requires: 	openpanel-mod-softwareupdate
Requires: 	openpanel-mod-ssh

%description suggested
OpenPanel with a suggested set of modules

%package minimal
Summary:        OpenPanel with a suggested set of modules
Group:          Application/Internet
Requires:       openpanel-core = %{version}
Requires: 	openpanel-mod-user
Requires: 	openpanel-gui
Requires: 	openpanel-cli
Requires: 	openpanel-mod-domain
Requires:	openpanel-authd

%description minimal
OpenPanel with a minimal set of modules


%prep
%setup -q -n %{pkgname}
%patch0 -p0
./configure --prefix=%{_prefix} --exec-prefix=%{_bindir} \
            --lib-prefix=%{_libdir} --conf-prefix=%{_sysconfdir} \
	    --include-prefix=%{_includedir}

%build
make

%install
rm -rf %{buildroot}
%makeinstall DESTDIR=%{buildroot}
mv %{buildroot}%{_localstatedir}/openpanel/db/panel/panel.db.setup %{buildroot}%{_localstatedir}/openpanel/db/panel/panel.db
cp -a %SOURCE1 %{buildroot}/etc/init.d/openpaneld

# Wipe Debian-specific stuff
rm -f %{buildroot}/etc/init.d/openpanel
rm -f %{buildroot}/etc/init.d/openpanel-core


# Crap. It wants a snakeoil SSL cert. Do it ourselves
answers() {
        echo --
        echo SomeState
        echo SomeCity
        echo SomeOrganization
        echo SomeOrganizationalUnit
        echo localhost.localdomain
        echo root@localhost.localdomain
}

certdir=%{buildroot}%{_sysconfdir}/pki/tls/openpanel/
mkdir -p $certdir

t1=`/bin/mktemp /tmp/openpanel.XXXXXX`
t2=`/bin/mktemp /tmp/openpanel.XXXXXX`

answers | /usr/bin/openssl req -newkey rsa:1024 -keyout $t1 -nodes -x509 -days 365 -out $t2
cat $t1 >> $certdir/openpanel.pem
echo "" >> $certdir/openpanel.pem
cat $t2 >> $certdir/openpanel.pem
rm -f $t1 $t2

%clean
rm -rf $RPM_BUILD_ROOT

%pre
/usr/sbin/groupadd -f -r openpanel-core >/dev/null 2>&1 || :
/usr/sbin/useradd -M -r -d / -s /sbin/nologin \
	-c "OpenPanel Daemon" -g openpanel-core openpanel-core >/dev/null 2>&1 || :
/usr/sbin/groupadd -f -r openpaneluser >/dev/null 2>&1 || :
/usr/sbin/useradd -M -d / -s /sbin/nologin \
	-c "OpenPanel Admin" -g openpaneluser openpanel-admin >/dev/null 2>&1 || :

%post
/sbin/chkconfig --add openpaneld

%preun
if [ $1 = 0 ]; then
	service openpaneld stop >/dev/null 2>&1
	/sbin/chkconfig --del openpanel-core
	/sbin/chkconfig --del openpanel
fi

%postun
if [ $1 = 0 ]; then
	/usr/sbin/userdel openpanel-core >/dev/null 2>&1 || :
	/usr/sbin/userdel openpanel-admin >/dev/null 2>&1 || :
	/usr/sbin/groupdel openpanel-core >/dev/null 2>&1 || :
	/usr/sbin/groupdel openpaneluser >/dev/null 2>&1 || :
fi
if [ "$1" -ge "1" ]; then
	service openpaneld condrestart >/dev/null 2>&1 || :
fi

%files
%defattr(-,root,root)
%config /etc/init.d/openpaneld
%config(noreplace) %{_localstatedir}/openpanel/db/panel/panel.db
%config(noreplace) %{_sysconfdir}/pki/tls/openpanel/openpanel.pem
%dir %{_sysconfdir}/openpanel/
%dir %{_localstatedir}/openpanel/
%dir %{_localstatedir}/openpanel/bin/
%dir %{_localstatedir}/openpanel/conf/
%dir %attr(0750,openpanel-core,openpanel-core) %{_localstatedir}/openpanel/cache/
%dir %attr(0700,openpanel-core,openpanel-core) %{_localstatedir}/openpanel/db/
%dir %attr(0700,openpanel-core,openpanel-core) %{_localstatedir}/openpanel/db/panel/
%dir %attr(0750,openpanel-core,openpanel-core) %{_localstatedir}/openpanel/debug/
%dir %attr(0750,openpanel-core,openpanel-core) %{_localstatedir}/openpanel/log/
%dir %attr(0750,openpanel-core,openpanel-core) %{_localstatedir}/openpanel/sockets/
%dir %attr(0750,openpanel-core,openpanel-core) %{_localstatedir}/openpanel/conf/rollback/
%dir %attr(0770,openpanel-core,openpanel-core) %{_localstatedir}/openpanel/conf/staging/
%{_localstatedir}/openpanel/bin/*

%files devel
%defattr(-,root,root)
%{_bindir}/mkmodulexml
%{_includedir}/
%{_libdir}/libgrace-coreapi.a
%{_libdir}/openpanel-core/

%files api-sh
%defattr(-,root,root)
%{_bindir}/coreval
%{_localstatedir}/openpanel/api/sh/

%files api-python
%defattr(-,root,root)
%{_localstatedir}/openpanel/api/python/

%files minimal
%files suggested

%changelog
* Wed Jan 18 2011 Igmar Palsenberg <igmar@palsenberg.com>
- Initial packaging
