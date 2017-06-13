Name: storage
Summary: storage for fastdfs
Version: 5.0.9
Release: 1
License: GPL
Group: Applications/Server
Source: storage-5.0.9.tar.gz
Distribution: Linux
Packager: jackey

%description
storage server

%prep

%setup

%build 

%install
rm -irf %{buildroot}/weike/storage
mkdir -p %{buildroot}/weike/storage
mkdir -p %{buildroot}/etc/fdfs
cp -afp bin %{buildroot}/weike/storage
cp -afp lib %{buildroot}/weike/storage
cp -afp etc/* %{buildroot}/etc/fdfs
chmod -R 755 %{buildroot}/etc/fdfs
chmod -R 755 %{buildroot}/weike/storage

%preun

%postun
sed -i '/weike\/storage/d' /etc/rc.d/rc.local
rm -rf /usr/lib64/libfastcommon.so

%files
/weike/storage
/etc/fdfs

%post
#modify /weike/storage to 755(anyone can excute)...
chmod -R 755 /weike/storage
chmod -R 755 /etc/fdfs
mkdir -p /fdfs/storage

#set resource limit...
ulimit -SHn 65535

echo "---- build soft-link to /usr/lib64 ----"
ln -sf /weike/storage/lib/libfastcommon.so /usr/lib64/libfastcommon.so

#set storage auto start...
echo "---- make storage auto-start ----"
bootfile="/etc/rc.d/rc.local"

#delete has 'ulimit' and 'weike/tracker' line...
sed -i '/ulimit/d' $bootfile
sed -i '/weike\/storage/d' $bootfile
sed -i '/bin\/sh/a ulimit -SHn 65535' $bootfile
echo "/weike/storage/bin/fdfs_storaged /etc/fdfs/storage.conf restart" >> $bootfile

#start storage-server...
echo "---- start storage server ----"
/weike/storage/bin/fdfs_storaged /etc/fdfs/storage.conf restart
ln -s /fdfs/storage/data /fdfs/storage/data/M00
