#
# WORDTEST specfile
#
# (C) Cyril Hrubis metan{at}ucw.cz 2026
#

Summary: A vocabulary test for language courses
Name: wordtest
Version: git
Release: 1
License: GPL-2.0-or-later
Group: Amusements/Teaching/Language
Url: https://github.com/gfxprim/wordtest
Source: wordtest-%{version}.tar.bz2
BuildRequires: libgfxprim-devel

BuildRoot: %{_tmppath}/%{name}-%{version}-buildroot

%description
A vocabulary test for language courses with a quiz and a written exam mode.
Courses are installed separately into /usr/share/wordtest/.

%prep
%setup -n wordtest-%{version}

%build
make %{?jobs:-j%jobs}

%install
DESTDIR="$RPM_BUILD_ROOT" make install

%files -n wordtest
%defattr(-,root,root)
%{_bindir}/wordtest
%{_sysconfdir}/gp_apps/
%{_sysconfdir}/gp_apps/wordtest/
%{_sysconfdir}/gp_apps/wordtest/*
%{_datadir}/applications/wordtest.desktop
%dir %{_datadir}/wordtest/

%changelog
* Mon Oct 26 2026 Cyril Hrubis <metan@ucw.cz>

  Initial version.
