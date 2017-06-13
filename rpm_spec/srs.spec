Name: srs
Summary: high performance srs live server
Version: 2.0.243
Release: 1
License: GPL
Group: Applications/Server
Source: srs-2.0.243.tar.gz
Distribution: Linux
Packager: jackey

%description
srs live server

%prep

%setup

%build 

%install
rm -irf %{buildroot}/weike/srs
mkdir -p %{buildroot}/weike/srs
cp -afp * %{buildroot}/weike/srs
chmod -R 755 %{buildroot}/weike/srs

%preun

%postun
sed -i '/weike\/srs/d' /etc/rc.d/rc.local

%files
/weike/srs

%post
#modify /weike/srs to 755(anyone can excute)...
chmod -R 755 /weike/srs

#set srs auto start...
echo "---- make srs auto-start ----"
bootfile="/etc/rc.d/rc.local"

#delete has 'ulimit' and 'weike/srs' line...
sed -i '/ulimit/d' $bootfile
sed -i '/weike\/srs/d' $bootfile
#add 'ulimit' and 'weike/srs' new line...
sed -i '/bin\/sh/a ulimit -SHn 65535' $bootfile
echo "/weike/srs/bin/srs -c /weike/srs/conf/srs.conf" >> $bootfile

echo "---- start srs live server ----"
/weike/srs/bin/srs -c /weike/srs/conf/srs.conf
