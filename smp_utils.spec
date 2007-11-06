%define name    smp_utils
%define version 0.93
%define release 1

Summary:        Utilities for SAS management protocol (SMP)
Name:           %{name}
Version:        %{version}
Release:        %{release}
License:        FreeBSD
Group:          Utilities/System
URL:            http://www.torque.net/sg/smp_utils.html
Source0:        http://www.torque.net/sg/p/%{name}-%{version}.tgz
BuildRoot:      %{_tmppath}/%{name}-%{version}-root
Packager:       Douglas Gilbert <dgilbert at interlog dot com>

%description
This is a package of utilities. Each utility sends a Serial Attached
SCSI (SAS) Management Protocol (SMP) request to a SMP target.
If the request fails then the error is decoded. If the request succeeds
then the response is either decoded, printed out in hexadecimal or
output in binary. This package supports multiple interfaces since
SMP passthroughs are not mature. This package supports the linux
2.4 and 2.6 series and should be easy to port to other operating
systems.

Warning: Some of these tools access the internals of your system
and the incorrect usage of them may render your system inoperable.

%prep

%setup -q

%build

make \
     CFLAGS="%{optflags} -DSMP_UTILS_LINUX"

%install
[ "%{buildroot}" != "/" ] && rm -rf %{buildroot}

make install \
        PREFIX=%{_prefix} \
        INSTDIR=%{buildroot}/%{_bindir} \
        MANDIR=%{buildroot}/%{_mandir} \
        INCLUDEDIR=%{buildroot}/%{_includedir}

%clean
[ "%{buildroot}" != "/" ] && rm -rf %{buildroot}

%files
%defattr(-,root,root)
%doc ChangeLog COPYING COVERAGE CREDITS INSTALL README
%attr(0755,root,root) %{_bindir}/*
%{_mandir}/man8/*

%changelog
* Mon Oct 29 2007 - dgilbert at interlog dot com
- add affiliation contexts (new in sas2r08), sync to sas2r12
  * smp_utils-0.93
* Fri Dec 08 2006 - dgilbert at interlog dot com
- sync against sas2r07, add smp_conf_general
  * smp_utils-0.92
* Tue Aug 22 2006 - dgilbert at interlog dot com
- add smp_phy_test and smp_discover_list, uniform exit status values
  * smp_utils-0.91
* Sun Jun 11 2006 - dgilbert at interlog dot com
- add smp_read_gpio, smp_conf_route_info and smp_write_gpio
  * smp_utils-0.90
