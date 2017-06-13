Name: php
Summary: high performance php server
Version: 5.6.30
Release: 1
License: GPL
Group: Applications/Server
Source: php-5.6.30.tar.gz
Distribution: Linux
Packager: jackey

%description
php script parser

%prep

%setup

%build 

%install
rm -irf %{buildroot}/weike/php
mkdir -p %{buildroot}/weike/php
cp -afp * %{buildroot}/weike/php
chmod -R 755 %{buildroot}/weike/php

%preun
#/weike/php/sbin/php-fpm stop

%postun
sed -i '/weike\/php/d' /etc/rc.d/rc.local
rm -rf /usr/lib64/libmhash.so
rm -rf /usr/lib64/libmhash.so.2
rm -rf /usr/lib64/libmcrypt.so
rm -rf /usr/lib64/libmcrypt.so.4
rm -rf /usr/lib64/libiconv.so
rm -rf /usr/lib64/libiconv.so.2
rm -rf /usr/lib64/libMagickWand.so
rm -rf /usr/lib64/libMagickWand.so.2
rm -rf /usr/lib64/libMagickCore.so
rm -rf /usr/lib64/libMagickCore.so.2
rm -rf /usr/lib64/libmysqlclient.so
rm -rf /usr/lib64/libmysqlclient.so.16
rm -rf /usr/lib64/libmysqlclient_r.so
rm -rf /usr/lib64/libmysqlclient_r.so.16
rm -rf /usr/lib64/libfastcommon.so
rm -rf /usr/lib64/libfdfsclient.so

%files
/weike/php

%post
#modify /weike/php to 755(anyone can excute)...
chmod -R 755 /weike/php

#modify /weike/php/tmp to 777(anyone can excute/write/read)...
chmod -R 777 /weike/php/tmp
chmod -R 777 /weike/php/logs

#build www:www user and group...
echo "---- create www user and www group ----"
/usr/sbin/groupadd www
/usr/sbin/useradd -g www www

#chown -R www:www /weike/php

#set resource limit...
ulimit -SHn 65535

echo "---- build soft-link for php-lib copy to /usr/lib64 ----"
ln -sf /weike/php/lib/libmhash.so.2.0.1 /usr/lib64/libmhash.so
ln -sf /weike/php/lib/libmhash.so.2.0.1 /usr/lib64/libmhash.so.2
ln -sf /weike/php/lib/libmcrypt.so.4.4.8 /usr/lib64/libmcrypt.so
ln -sf /weike/php/lib/libmcrypt.so.4.4.8 /usr/lib64/libmcrypt.so.4
ln -sf /weike/php/lib/libiconv.so.2.5.0 /usr/lib64/libiconv.so
ln -sf /weike/php/lib/libiconv.so.2.5.0 /usr/lib64/libiconv.so.2
ln -sf /weike/php/lib/libMagickWand.so.2.0.0 /usr/lib64/libMagickWand.so
ln -sf /weike/php/lib/libMagickWand.so.2.0.0 /usr/lib64/libMagickWand.so.2
ln -sf /weike/php/lib/libMagickCore.so.2.0.0 /usr/lib64/libMagickCore.so
ln -sf /weike/php/lib/libMagickCore.so.2.0.0 /usr/lib64/libMagickCore.so.2
ln -sf /weike/php/lib/libmysqlclient.so.16.0.0 /usr/lib64/libmysqlclient.so
ln -sf /weike/php/lib/libmysqlclient.so.16.0.0 /usr/lib64/libmysqlclient.so.16
ln -sf /weike/php/lib/libmysqlclient_r.so.16.0.0 /usr/lib64/libmysqlclient_r.so
ln -sf /weike/php/lib/libmysqlclient_r.so.16.0.0 /usr/lib64/libmysqlclient_r.so.16
ln -sf /weike/php/lib/libfastcommon.so /usr/lib64/libfastcommon.so
ln -sf /weike/php/lib/libfdfsclient.so /usr/lib64/libfdfsclient.so

#load the dynamic config...
ldconfig

#set php-fpm auto start...
echo "---- make php-cgi auto-start ----"
bootfile="/etc/rc.d/rc.local"

#delete has 'ulimit' and 'weike/php' line...
sed -i '/ulimit/d' $bootfile
sed -i '/weike\/php/d' $bootfile
#add 'ulimit' and 'weike/php' new line...
sed -i '/bin\/sh/a ulimit -SHn 65535' $bootfile
echo "/weike/php/sbin/php-fpm" >> $bootfile

#start php-cgi process, listen on 127.0.0.1:9000, process number => 128(if mem < 3GB, set to 64), user is www;
#notice: /weike/php/sbin/php-fpm,other param: start|stop|quit|restart|reload|logrotate, modify php.ini, do not restart php-cgi, use reload param
#notice: php-fpm is PHP FastCGI manager patch, it can smoothly change php.ini not restart php-cgi...

echo "---- start php-cgi ----"
/weike/php/sbin/php-fpm
