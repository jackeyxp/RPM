CentOS6.8版本
==========================================

安装编译环境
==========================================
yum -y install gcc gcc-c++ autoconf automake
yum -y install zlib zlib-devel openssl openssl-devel pcre pcre-devel
yum -y install perl vim subversion cmake make gcc gcc-c++ openssl-devel
yum -y install unzip gzip libxml2-devel patch libcurl-devel gd-devel

编译安装 mysql，默认密码：Kuihua*#816
==========================================
yum -y install ncurses-devel

/usr/sbin/groupadd mysql
/usr/sbin/useradd -g mysql mysql
tar zxvf mysql-5.5.3-m3.tar.gz
cd mysql-5.5.3-m3/
make clean
./configure --prefix=/weike/mysql/ --enable-assembler --with-extra-charsets=complex --enable-thread-safe-client --with-big-tables --with-readline --with-ssl --with-embedded-server --enable-local-infile --with-plugins=partition,innobase,myisammrg
make && make install
