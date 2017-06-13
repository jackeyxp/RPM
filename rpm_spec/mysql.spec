Name: mysql
Summary: high performance mysql server
Version: 5.5.3
Release: 1
License: GPL
Group: Applications/Server
Source: mysql-5.5.3.tar.gz
Distribution: Linux
Packager: jackey

%description
mysql database server

%prep

%setup

%build 

%install
rm -irf %{buildroot}/weike/mysql
mkdir -p %{buildroot}/weike/mysql
cp -afp * %{buildroot}/weike/mysql
chmod -R 755 %{buildroot}/weike/mysql

%preun
#/weike/mysql/sbin/mysql stop

%postun
sed -i '/weike\/mysql/d' /etc/rc.d/rc.local

%files
/weike/mysql

%post
#modify /weike/mysql to 755 ...
chmod -R 755 /weike/mysql

#build mysql:mysql user and group...
echo "---- create mysql user and mysql group ----"
/usr/sbin/groupadd mysql
/usr/sbin/useradd -g mysql mysql

#modify /weike/mysql owner...
#notice: owner can write ...
echo "---- modify /weike/mysql owner ----"
chown -R mysql:mysql /weike/mysql

#set mysql auto start...
echo "---- make mysql auto-start ----"
bootfile="/etc/rc.d/rc.local"

#delete has 'weike/mysql' line...
sed -i '/weike\/mysql/d' $bootfile
#add 'weike/mysql' new line...
echo "/weike/mysql/sbin/mysql start" >> $bootfile

echo "---- start mysql server ----"
/weike/mysql/sbin/mysql start
