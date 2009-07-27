%define _plugindir %{_libdir}/gimp/2.0/plug-ins
%define _mathmapdir %{_datadir}/gimp/2.0/mathmap
%define _langdir %{_datadir}/gtksourceview-2.0/language-specs

%if 0%{?mandriva_version}
%define mdkversion %{mandriva_version}00
%endif

%if "x%{?_vendor}" == "xmandriva"

%define _mmrelease %mkrel 1
%define _gtksourceview libgtksourceview-2.0

%ifarch x86_64
%define _giflib lib64ungif4
%else
%define _giflib libungif
%endif

%else

%define _mmrelease 1
%define _giflib giflib

%if 0%{?suse_version}
%define _gtksourceview gtksourceview
%else
%define _gtksourceview gtksourceview2
%endif

%endif

Name:           mathmap
Version:        1.3.4
Release:        %{_mmrelease}
License:        GPL
BuildRoot:      %{_tmppath}/%{name}-%{version}-build
Group:		Applications/Multimedia
Summary:	MathMap GIMP Plug-In and Command-Line Tool
URL:		http://www.complang.tuwien.ac.at/schani/mathmap/
Source:		%{name}_%{version}-1.tar.gz
Requires: gcc
Requires: gimp
BuildRequires: gcc
BuildRequires: gcc-c++
BuildRequires: libpng-devel
BuildRequires: libjpeg-devel
BuildRequires: %{_giflib}-devel
BuildRequires: gsl-devel
BuildRequires: gimp-devel
BuildRequires: gimp
BuildRequires: make
BuildRequires: fftw3-devel
BuildRequires: %{_gtksourceview}-devel
BuildRequires: gettext
BuildRequires: unzip
BuildRequires: doxygen
BuildRequires: perl
%if "x%{?_vendor}" == "xmandriva"
BuildRequires: gimp-help-2-en
%if %mdkversion >= 200900
BuildRequires: pulseaudio-esound-compat
%endif
%endif
%if 0%{?fedora_version}
%if %fedora_version == 8
BuildRequires: lynx
%endif
%endif

%description
MathMap is a GIMP plug-in which allows distortion of images specified
by mathematical formulae.  For each pixel in the generated image, an
expression is evaluated which should return a pixel value.  The
expression can either refer to a pixel in the source image or can
generate pixels completely independent of the source.

%prep
%setup

%build
%{__make}

%install
make DESTDIR=$RPM_BUILD_ROOT install

%clean
rm -rf "$RPM_BUILD_ROOT"

%files
%defattr(-,root,root)
%doc ANNOUNCEMENT COPYING README README.filters README.git
%{_bindir}/mathmap
%{_plugindir}/mathmap
%{_langdir}/mathmap.lang
%{_mathmapdir}
%{_datadir}/locale/*/LC_MESSAGES/mathmap.mo

%changelog
* Sun Jul 26 2009 Mark Probst <schani@complang.tuwien.ac.at> 1.3.4
- Update for gtksourceview2

* Sun Aug 31 2008 Mark Probst <schani@complang.tuwien.ac.at> 1.3.4
- Update for version 1.3.4

* Tue Aug 26 2008 Mark Probst <schani@complang.tuwien.ac.at> 1.3.3
- Update for version 1.3.3

* Sat Feb 16 2008 Mark Probst <schani@complang.tuwien.ac.at> 1.3.2
- Update for version 1.3.2

* Sun Jan 13 2008 Mark Probst <schani@complang.tuwien.ac.at> 1.3.1
- Update for version 1.3.1

* Tue Jan 01 2008 Mark Probst <schani@complang.tuwien.ac.at> 1.3.0
- Update for version 1.3.0

* Mon Dec 03 2007 Mark Probst <schani@complang.tuwien.ac.at> 1.2.4
- openSUSE Build Service

* Fri Nov 23 2007 Mark Probst <schani@complang.tuwien.ac.at> 1.2.4
- Update for version 1.2.4

* Fri Nov 09 2007 Mark Probst <schani@complang.tuwien.ac.at> 1.2.3
- Update for version 1.2.3

* Sun Nov 04 2007 Mark Probst <schani@complang.tuwien.ac.at> 1.2.2
- Update for version 1.2.2

* Thu May 04 2007 Mark Probst <schani@complang.tuwien.ac.at> 1.2.0
- Update for version 1.2.0

* Thu Apr 12 2007 Mark Probst <schani@complang.tuwien.ac.at> 1.1.3
- First creation of spec file
