Name: nginx-storage
Summary: high performance httpd server
Version: 1.10.2
Release: 1
License: GPL
Group: Applications/Server
Source: nginx-storage-1.10.2.tar.gz
Distribution: Linux
Packager: jackey

%description
httpd server

%prep

%setup

%build 

%install
rm -irf %{buildroot}/weike/nginx
mkdir -p %{buildroot}/weike/nginx
cp -afp * %{buildroot}/weike/nginx
chmod -R 755 %{buildroot}/weike/nginx

%preun

%postun
sed -i '/weike\/nginx/d' /etc/rc.d/rc.local
rm -rf /usr/lib64/libfastcommon.so
rm -rf /usr/lib64/libfdfsclient.so

%files
/weike/nginx

%post
#modify /weike/nginx to 755(anyone can excute)...
chmod -R 755 /weike/nginx

#build www:www user and group...
echo "---- create www user and www group ----"
/usr/sbin/groupadd www
/usr/sbin/useradd -g www www

#modify /weike/nginx/logs owner...
#notice: owner can write...
echo "---- modify /weike/nginx/logs owner ----"
chown -R www:www /weike/nginx/logs

echo "---- build soft-link to /usr/lib64  ----"
ln -sf /weike/nginx/lib/libfastcommon.so /usr/lib64/libfastcommon.so
ln -sf /weike/nginx/lib/libfdfsclient.so /usr/lib64/libfdfsclient.so

#set nginx auto start...
echo "---- make nginx auto-start ----"
bootfile="/etc/rc.d/rc.local"

#delete has 'ulimit' and 'weike/nginx' line...
sed -i '/ulimit/d' $bootfile
sed -i '/weike\/nginx/d' $bootfile
#add 'ulimit' and 'weike/nginx' new line...
sed -i '/bin\/sh/a ulimit -SHn 65535' $bootfile
echo "/weike/nginx/sbin/nginx -p /weike/nginx/" >> $bootfile

#start nginx-server, default start 8 process, conf/nginx.conf can change...
#notice: /weike/nginx/sbin/nginx -h => for help...
#notice: -p param must be set...
echo "---- start nginx http server ----"
/weike/nginx/sbin/nginx -p /weike/nginx/
