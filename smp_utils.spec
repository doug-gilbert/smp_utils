%define name    smp_utils
%define version 1.00
%define release 1

Summary:        Utilities for SAS Serial Management Protocol (SMP)
Name:           %{name}
Version:        %{version}
Release:        %{release}
License:        FreeBSD
Group:          Utilities/System
URL:            http://sg.danny.cz/sg/smp_utils.html
Source0:        http://sg.danny.cz/sg/p/%{name}-%{version}.tgz
Provides:       smp_utils
# BuildRequires:  libtool
BuildRoot:      %{_tmppath}/%{name}-%{version}-root
Packager:       Douglas Gilbert <dgilbert at interlog dot com>

%description
This is a package of utilities. Each utility sends a Serial Attached
SCSI (SAS) Serial Management Protocol (SMP) request to a SMP target.
If the request fails then the error is decoded. If the request succeeds
then the response is either decoded, printed out in hexadecimal or
output in binary. This package supports the linux 2.6 and 3 series and
has ports to FreeBSD and Solaris.

Warning: These utilities access SAS expanders (storage switches) and
the incorrect usage of them may render your system and others inoperable.

%package libs
Summary: Shared library for %{name}
Group: System/Libraries

%description libs
This package contains the shared library for %{name}.

%package devel
Summary: Static library and header files for the smputils library
Group: Development/C
Requires: %{name}-libs = %{version}-%{release}

%description devel
This package contains the static %{name} library and its header files for
developing applications.

%prep
%setup -q

%build
%configure
sed -i 's|^hardcode_libdir_flag_spec=.*|hardcode_libdir_flag_spec=""|g' libtool
sed -i 's|^runpath_var=LD_RUN_PATH|runpath_var=DIE_RPATH_DIE|g' libtool

%install
if [ "$RPM_BUILD_ROOT" != "/" ]; then
        rm -rf $RPM_BUILD_ROOT
fi

make install \
        DESTDIR=$RPM_BUILD_ROOT

%clean
if [ "$RPM_BUILD_ROOT" != "/" ]; then
        rm -rf $RPM_BUILD_ROOT
fi

%files
%defattr(-,root,root)
%doc AUTHORS ChangeLog COPYING COVERAGE CREDITS INSTALL NEWS README
%attr(755,root,root) %{_bindir}/*
%{_mandir}/man8/*

%files libs
%defattr(-,root,root)
%{_libdir}/*.so.*

%files devel
%defattr(-,root,root)
%{_includedir}/scsi/*.h
%{_libdir}/*.so
%{_libdir}/*.a
%{_libdir}/*.la


%changelog
* Tue Jun 15 2021 - dgilbert at interlog dot com
- see ChangeLog
  * smp_utils-1.00
* Thu Mar 05 2020 - dgilbert at interlog dot com
- add support for G5 (22.5 Gbps, SAS-4, SPL-5)
  * smp_utils-0.99
* Mon May 26 2014 - dgilbert at interlog dot com
- put execs back in /usr/bin, add aac interface
  * smp_utils-0.98
* Fri Jan 20 2012 - dgilbert at interlog dot com
- change to ./configure style build, put execs in /usr/sbin
  * smp_utils-0.97
* Sun Jun 19 2011 - dgilbert at interlog dot com
- add zoning for SAS-2, SPL, SPL-2
  * smp_utils-0.96
* Tue Oct 27 2009 - dgilbert at interlog dot com
- discover changes in spl-r04
  * smp_utils-0.95
* Mon Dec 29 2008 - dgilbert at interlog dot com
- adjust sgv4 for lk 2.6.27, sync with sas2r15
  * smp_utils-0.94
* Sun Jan 06 2008 - dgilbert at interlog dot com
- sync with sas2r13, add 'sgv4' interface
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
