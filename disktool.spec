Name:		disktool
Version:	%{version}
Release:	%{release}%{?dist}
Summary:	A set of tools to display and manipulate the block devices visible to the system using sysfs.

Group:		Development/Tools
License:	GPLv2
URL:		http://hg.chrisprocter.co.uk/repos/
Source:		disktool-%{version}.tar	
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}


%description
A set of tools to display and manipulate the block devices visible to the system using sysfs.

%prep
%setup -q


%build
make 


%install
rm -rf $RPM_BUILD_ROOT
mkdir $RPM_BUILD_ROOT
make install INSTALL_ROOT=$RPM_BUILD_ROOT


%clean
rm -rf $RPM_BUILD_ROOT


%files
%defattr(-,root,root,-)
/usr/sbin/disktool
/usr/share/man/man8/disktool.8.gz

%changelog

