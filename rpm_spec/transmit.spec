Name: transmit
Summary: high performance transmit server
Version: 1.0.1
Release: 1
License: GPL
Group: Applications/Server
Source: transmit-1.0.1.tar.gz
Distribution: Linux
Packager: jackey

%description
transmit server

%prep

%setup

%build 
g++ transmit.c -o transmit -ljson

%install
rm -irf %{buildroot}/weike/transmit
mkdir -p %{buildroot}/weike/transmit
cp -afp transmit %{buildroot}/weike/transmit/
cp -afp libjson.so.0.1.0 %{buildroot}/weike/transmit/

%preun

%postun
sed -i '/weike\/transmit/d' /etc/rc.d/rc.local

%files
/weike/transmit

%post
echo "---- build soft-link for libjson copy to /usr/lib64 ----"
ln -sf /weike/transmit/libjson.so.0.1.0 /usr/lib64/libjson.so
ln -sf /weike/transmit/libjson.so.0.1.0 /usr/lib64/libjson.so.0

#modify /weike/transmit to 755(anyone can excute)...
chmod -R 755 /weike/transmit

#set transmit auto start...
echo "---- make transmit auto-start ----"
bootfile="/etc/rc.d/rc.local"

#delete has 'ulimit' and 'weike/transmit' line...
sed -i '/ulimit/d' $bootfile
sed -i '/weike\/transmit/d' $bootfile
#add 'ulimit' and 'weike/transmit' new line...
sed -i '/bin\/sh/a ulimit -SHn 65535' $bootfile
echo "/weike/transmit/transmit &" >> $bootfile

#start transmit-server...
echo "---- start transmit server ----"
/weike/transmit/transmit &
