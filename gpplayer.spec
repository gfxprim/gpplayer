#
# GPPLAYER specfile
#
# (C) Cyril Hrubis metan{at}ucw.cz 2013-2024
#
#

Summary: A mp3 player written using libmpg123 and gfxprim widgets
Name: gpplayer
Version: git
Release: 1
License: GPL-2.0-or-later
Group: Applications/Multimedia
Url: https://github.com/gfxprim/gpplayer
Source: gpplayer-%{version}.tar.bz2
BuildRequires: alsa-devel
BuildRequires: mpg123-devel
BuildRequires: mpv-devel
BuildRequires: libgfxprim-devel

BuildRoot: %{_tmppath}/%{name}-%{version}-buildroot

%description
A mp3 player written using libmpg123 and gfxprim widgets

%prep
%setup -n gpplayer-%{version}

%build
./configure
make %{?jobs:-j%jobs}

%install
DESTDIR="$RPM_BUILD_ROOT" make install

%files -n gpplayer
%defattr(-,root,root)
%{_bindir}/gpplayer
%{_sysconfdir}/gp_apps/
%{_sysconfdir}/gp_apps/gpplayer/
%{_sysconfdir}/gp_apps/gpplayer/*
%{_datadir}/applications/gpplayer.desktop
%{_datadir}/gpplayer/
%{_datadir}/gpplayer/gpplayer.png

%changelog
* Sun Jan 31 2021 Cyril Hrubis <metan@ucw.cz>

  Initial version.
