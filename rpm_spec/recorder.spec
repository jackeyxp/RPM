Name: recorder
Summary: high performance php web code
Version: 0.0.1
Release: 1
License: GPL
Group: Applications/Server
Source: recorder-0.0.1.tar.gz
Distribution: Linux
Packager: jackey

%description
recorder web code

%prep

%setup

%build 

%install
rm -irf %{buildroot}/weike/htdocs
mkdir -p %{buildroot}/weike/htdocs
cp -afp * %{buildroot}/weike/htdocs
chmod -R 755 %{buildroot}/weike/htdocs

%preun

%files
/weike/htdocs

%post
#modify /weike/htdocs to 755(anyone can excute)...
chmod -R 755 /weike/htdocs

#build www:www user and group...
echo "---- create www user and www group ----"
/usr/sbin/groupadd www
/usr/sbin/useradd -g www www

#modify /weike/htdocs owner...
#notice: owner can write...
echo "---- modify /weike/htdocs owner ----"
chown -R www:www /weike/htdocs
