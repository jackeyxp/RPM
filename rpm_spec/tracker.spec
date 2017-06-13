Name: tracker
Summary: tracker for fastdfs
Version: 5.0.9
Release: 1
License: GPL
Group: Applications/Server
Source: tracker-5.0.9.tar.gz
Distribution: Linux
Packager: jackey

%description
tracker server

%prep

%setup

%build 

%install
rm -irf %{buildroot}/weike/tracker
mkdir -p %{buildroot}/weike/tracker
mkdir -p %{buildroot}/etc/fdfs
cp -afp bin %{buildroot}/weike/tracker
cp -afp lib %{buildroot}/weike/tracker
cp -afp etc/* %{buildroot}/etc/fdfs
chmod -R 755 %{buildroot}/etc/fdfs
chmod -R 755 %{buildroot}/weike/tracker

%preun

%postun
sed -i '/weike\/tracker/d' /etc/rc.d/rc.local
rm -rf /usr/lib64/libfastcommon.so

%files
/weike/tracker
/etc/fdfs

%post
#modify /weike/tracker to 755(anyone can excute)...
chmod -R 755 /weike/tracker
chmod -R 755 /etc/fdfs
mkdir -p /fdfs/tracker

#set resource limit...
ulimit -SHn 65535

echo "---- build soft-link to /usr/lib64 ----"
ln -sf /weike/tracker/lib/libfastcommon.so /usr/lib64/libfastcommon.so

#set tracker auto start...
echo "---- make tracker auto-start ----"
bootfile="/etc/rc.d/rc.local"

#delete has 'ulimit' and 'weike/tracker' line...
sed -i '/ulimit/d' $bootfile
sed -i '/weike\/tracker/d' $bootfile
sed -i '/bin\/sh/a ulimit -SHn 65535' $bootfile
echo "/weike/tracker/bin/fdfs_trackerd /etc/fdfs/tracker.conf restart" >> $bootfile

#start tracker-server...
echo "---- start tracker server ----"
/weike/tracker/bin/fdfs_trackerd /etc/fdfs/tracker.conf restart
