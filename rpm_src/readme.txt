
安装环境：CentOS6.8
===================================================
1. PHP+MySQL+Nginx+FastDFS+Transmit+Live 都需要在CentOS6.8下面全部重新编译
2. 重新编译之后，再打包成 rpm 数据包，都需要在CentOS6.8下面完成

SSH Secure Shell 解决中文乱码的终极方法：
===================================================
1. http://www.cnblogs.com/52linux/archive/2012/03/24/2415082.html
2. #vi /etc/sysconfig/i18n
LANG="zh_CN.GB18030"  
LANGUAGE="zh_CN.GB18030:zh_CN.GB2312:zh_CN"  
SUPPORTED="zh_CN.GB18030:zh_CN:zh:en_US.UTF-8:en_US:en"  
SYSFONT="lat0-sun16"

安装 mysql-5.5.3-1.x86_64.rpm
==========================================
1. yum -y install perl
2. yum -y install perl-DBI
3. rpm -ivh mysql-5.5.3-1.x86_64.rpm

安装编译环境
==========================================
yum -y install gcc gcc-c++ autoconf automake
yum -y install zlib zlib-devel openssl openssl-devel pcre pcre-devel
yum -y install perl vim subversion cmake make gcc gcc-c++ openssl-devel
yum -y install gzip libxml2-devel patch libcurl-devel gd-devel
yum -y install openldap openldap-devel
ln -s /usr/lib64/libjpeg.so /usr/lib/libjpeg.so
ln -s /usr/lib64/libpng.so /usr/lib/libpng.so
cp -frp /usr/lib64/libldap* /usr/lib/
ln -s /usr/lib64/mysql/libmysqlclient.so.16.0.0 /usr/lib/libmysqlclient.so

安装 mysql，需要独立编译安装
==========================================
yum -y install ncurses-devel

修改默认的字符集
==========================================
mysql_com.h:30 => utf8

/usr/sbin/groupadd mysql
/usr/sbin/useradd -g mysql mysql
tar zxvf mysql-5.5.3-m3.tar.gz
cd mysql-5.5.3-m3/
make clean
./configure --prefix=/weike/mysql/ --enable-assembler --with-extra-charsets=complex --enable-thread-safe-client --with-big-tables --with-readline --with-ssl --with-embedded-server --enable-local-infile --with-plugins=partition,innobase,myisammrg
make && make install

安装 php，需要独立编译安装
==========================================
1. 安装libiconv
tar zxvf libiconv-1.13.1.tar.gz
cd libiconv-1.13.1/
./configure --prefix=/usr/local
make
make install
cd ../

2. 安装libmcrypt
tar zxvf libmcrypt-2.5.8.tar.gz 
cd libmcrypt-2.5.8/
./configure
make
make install
/sbin/ldconfig
cd libltdl/
./configure --enable-ltdl-install
make
make install
cd ../../

3. 安装mhash
tar zxvf mhash-0.9.9.9.tar.gz
cd mhash-0.9.9.9/
./configure
make
make install
cd ../

4. 安装软连接
ln -s /usr/local/lib/libmcrypt.la /usr/lib/libmcrypt.la
ln -s /usr/local/lib/libmcrypt.so /usr/lib/libmcrypt.so
ln -s /usr/local/lib/libmcrypt.so.4 /usr/lib/libmcrypt.so.4
ln -s /usr/local/lib/libmcrypt.so.4.4.8 /usr/lib/libmcrypt.so.4.4.8
ln -s /usr/local/lib/libmhash.a /usr/lib/libmhash.a
ln -s /usr/local/lib/libmhash.la /usr/lib/libmhash.la
ln -s /usr/local/lib/libmhash.so /usr/lib/libmhash.so
ln -s /usr/local/lib/libmhash.so.2 /usr/lib/libmhash.so.2
ln -s /usr/local/lib/libmhash.so.2.0.1 /usr/lib/libmhash.so.2.0.1
ln -s /usr/local/bin/libmcrypt-config /usr/bin/libmcrypt-config

5. 安装mcrypt
tar zxvf mcrypt-2.6.8.tar.gz
cd mcrypt-2.6.8/
/sbin/ldconfig
./configure
make
make install
cd ../

6. 编译安装PHP 
rm -irf php-5.2.14       
tar zxvf php-5.2.14.tar.gz
gzip -cd php-5.2.14-fpm-0.5.14.diff.gz | patch -d php-5.2.14 -p1
cd php-5.2.14/
make clean
./configure --prefix=/weike/php --with-config-file-path=/weike/php/etc --with-mysql=/weike/mysql --with-mysqli=/weike/mysql/bin/mysql_config --with-iconv-dir=/usr/local --with-freetype-dir --with-jpeg-dir --with-png-dir --with-zlib --with-libxml-dir=/usr --enable-xml --disable-rpath --enable-discard-path --enable-safe-mode --enable-bcmath --enable-shmop --enable-sysvsem --enable-inline-optimization --with-curl --with-curlwrappers --enable-mbregex --enable-fastcgi --enable-fpm --enable-force-cgi-redirect --enable-mbstring --with-mcrypt --with-gd --enable-gd-native-ttf --with-openssl --with-mhash --enable-pcntl --enable-sockets --with-ldap --with-ldap-sasl --with-xmlrpc --enable-zip --enable-soap
make ZEND_EXTRA_LIBS='-liconv'
make install
cp php.ini-dist /weike/php/etc/php.ini
cd ../

安装 PHP 扩展模块
==========================================
0. 安装 memcache 扩展
tar zxvf memcache-2.2.5.tgz
cd memcache-2.2.5/
/weike/php/bin/phpize
./configure --with-php-config=/weike/php/bin/php-config
make
make install
cd ../

1. 安装 eaccelerator 扩展
tar jxvf eaccelerator-0.9.6.1.tar.bz2
cd eaccelerator-0.9.6.1/
/weike/php/bin/phpize
./configure --enable-eaccelerator=shared --with-php-config=/weike/php/bin/php-config
make
make install
cd ../

2. 安装 PDO_MYSQL 扩展
tar zxvf PDO_MYSQL-1.0.2.tgz
cd PDO_MYSQL-1.0.2/
/weike/php/bin/phpize
./configure --with-php-config=/weike/php/bin/php-config --with-pdo-mysql=/weike/mysql
make
make install
cd ../

3. 安装 ImageMagick 扩展
yum install perl-ExtUtils-CBuilder perl-ExtUtils-MakeMaker
tar zxvf ImageMagick.tar.gz
cd ImageMagick-6.5.1-2/
./configure
make
make install
cd ../

4. 安装 imagick 扩展
tar zxvf imagick-2.3.0.tgz
cd imagick-2.3.0/
/weike/php/bin/phpize
./configure --with-php-config=/weike/php/bin/php-config
make
make install
cd ../

5. 安装编译后，开始编译fastdfs-client模块： 
=======================================================
注意：不能单独编译，只能跟fastdfs-master一起编译
=======================================================
编译后的php放置到 => /weike/php
编译 /fastdfs-master/php_client 插件模块：
cd /fastdfs-master/php_client
/weike/php/bin/phpize
./configure --with-php-config=/weike/php/bin/php-config
make && make install
插件模块位置 => php_client/modules/fastdfs_client.so
插件原始配置 => php_client/fastdfs_client.ini => 可以被整合到php.ini当中
整理后的安装 => linux/webserver/rpm_web_64/php-5.2.14
整理后的配置 => linux/webserver/rpm_web_64/php-5.2.14/etc/php.ini

6. 更新特定的 php.ini 需要安装软连接指向 /weike/php/lib 和 /weike/php/ext
=====================================================================================
ln -sf /weike/php/lib/libmhash.so.2.0.1 /usr/lib/libmhash.so
ln -sf /weike/php/lib/libmhash.so.2.0.1 /usr/lib/libmhash.so.2
ln -sf /weike/php/lib/libmcrypt.so.4.4.8 /usr/lib/libmcrypt.so
ln -sf /weike/php/lib/libmcrypt.so.4.4.8 /usr/lib/libmcrypt.so.4
ln -sf /weike/php/lib/libltdl.so.3.1.0 /usr/lib/libltdl.so
ln -sf /weike/php/lib/libltdl.so.3.1.0 /usr/lib/libltdl.so.3
ln -sf /weike/php/lib/libiconv.so.2.5.0 /usr/lib/libiconv.so
ln -sf /weike/php/lib/libiconv.so.2.5.0 /usr/lib/libiconv.so.2
ln -sf /weike/php/lib/libMagickWand.so.2.0.0 /usr/lib/libMagickWand.so
ln -sf /weike/php/lib/libMagickWand.so.2.0.0 /usr/lib/libMagickWand.so.2
ln -sf /weike/php/lib/libMagickCore.so.2.0.0 /usr/lib/libMagickCore.so
ln -sf /weike/php/lib/libMagickCore.so.2.0.0 /usr/lib/libMagickCore.so.2
ln -sf /weike/php/lib/libmysqlclient.so.16.0.0 /usr/lib/libmysqlclient.so
ln -sf /weike/php/lib/libmysqlclient.so.16.0.0 /usr/lib/libmysqlclient.so.16
ln -sf /weike/php/lib/libmysqlclient_r.so.16.0.0 /usr/lib/libmysqlclient_r.so
ln -sf /weike/php/lib/libmysqlclient_r.so.16.0.0 /usr/lib/libmysqlclient_r.so.16
ln -sf /weike/php/lib/libfastcommon.so /usr/lib/libfastcommon.so
ln -sf /weike/php/lib/libfdfsclient.so /usr/lib/libfdfsclient.so

编译安装 tracker 
=====================================================================================
yum -y install unzip gzip
unzip -q libfastcommon-1.33.zip
cd libfastcommon-master
./make.sh && ./make.sh install
ln -sf /usr/lib/libfastcommon.so /usr/local/lib/libfastcommon.so

unzip -q fastdfs-5.09.zip
cd fastdfs-master
vi make.sh
TARGET_PREFIX=$DESTDIR/usr => TARGET_PREFIX=$DESTDIR/usr/local
vi client/Makefile.in
TARGET_INC = $(TARGET_PREFIX)/include => TARGET_INC = /usr/include
./make.sh && ./make.sh install
ln -sf /usr/local/lib64/libfdfsclient.so /usr/lib64/libfdfsclient.so
ln -sf /usr/local/lib/libfdfsclient.so /usr/lib/libfdfsclient.so

vi /etc/fdfs/tracker.conf
disabled=false            #启用配置文件
port=22122                #设置tracker的端口号
base_path=/fdfs/tracker   #设置tracker的数据文件和日志目录（需预先创建）
http.server_port=80       #设置http端口号

mkdir /fdfs/tracker
vim /etc/sysconfig/iptables # 修改防火墙，打开端口22122
/usr/local/bin/fdfs_trackerd /etc/fdfs/tracker.conf restart #启动tracker

安装编译环境
==========================================
yum -y install gcc gcc-c++ autoconf automake
yum -y install zlib zlib-devel openssl openssl-devel pcre pcre-devel
yum -y install perl vim subversion cmake make gcc gcc-c++ openssl-devel
yum -y install unzip gzip libxml2-devel patch libcurl-devel gd-devel

编译php5.6.30版本
===========================================
yum -y install zlib-devel libxm12-devel libjpeg-devel libjpeg-turbo-devel libiconv-devel freetype-devel libpng-devel libpng-devel gd-devel libcurl-devel libxslt-devel

tar zxvf php-5.6.30.tar.gz
cd php-5.6.30
./configure --prefix=/weike/php --with-config-file-path=/weike/php/etc --with-mysql=/weike/mysql --with-mysqli=/weike/mysql/bin/mysql_config --with-iconv-dir=/usr/local --with-freetype-dir --with-jpeg-dir --with-png-dir --with-zlib --with-libxml-dir=/usr --enable-xml --disable-rpath --enable-bcmath --enable-shmop --enable-sysvsem --enable-inline-optimization --with-curl --enable-mbregex --enable-fpm --enable-mbstring --with-mcrypt --with-gd --enable-gd-native-ttf --with-openssl --with-mhash --enable-pcntl --enable-sockets --with-ldap --with-ldap-sasl --with-xmlrpc --enable-zip --enable-soap --disable-debug --disable-ipv6
make ZEND_EXTRA_LIBS='-liconv'
make install

1. 安装 PDO_MYSQL 扩展
tar zxvf PDO_MYSQL-1.0.2.tgz
cd PDO_MYSQL-1.0.2/
/weike/php/bin/phpize
./configure --with-php-config=/weike/php/bin/php-config --with-pdo-mysql=/weike/mysql
ln -s /weike/mysql/include/mysql/* /usr/include
make
make install
cd ../

2. 安装 ImageMagick 扩展
yum install perl-ExtUtils-CBuilder perl-ExtUtils-MakeMaker
tar zxvf ImageMagick.tar.gz
cd ImageMagick-6.5.1-2/
./configure
make
make install
cd ../

3. 安装 imagick 扩展
tar zxvf imagick-3.4.3.tgz
cd imagick-3.4.3/
/weike/php/bin/phpize
./configure --with-php-config=/weike/php/bin/php-config
make
make install
cd ../

