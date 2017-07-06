CentOS6.8版本
==========================================

2017.07.05
==========================================
1. 对所有的部件进行了重新编译，更新到阿里云服务器上。

2017.07.04
==========================================
1. --with-openssl=../openssl-1.0.1f --with-openssl-opt="enable-tlsext" => 一个443支持多个https域名，公司网站需要用到 nginx-all 版本；
2. 新增TLS的SNI扩展，可以支持多个https域名，这个功能只是公司网站用，不用更新到all或tracker或storage当中，需要时再编译；
3. 编译方法详见《浩一监控技术总结.doc》 => G. 编译nginx => All
4. 编译参考 => http://www.jianshu.com/p/d40e249774ff

2017.07.03
==========================================
1. srs => 修改了代码，支持http_hooks回调时，将问号前后的数据分离；与nginx-rtmp保持一致；
2. transmit => 修改了录像状态的bug。

2017.07.01
==========================================
1. nginx-storage => 新增ssl支持
2. nginx-tracker => 新增ssl支持
3. nginx-all => 包含storage+tracker的合集，新增ssl支持

2017.06.12
==========================================
1. 新增 srs-2.0.243，用来替换 live-1.12.0，直播的效率更高，速度更快；
2. 新增 php-5.6.30，解决curl崩溃的问题，速度更快，问题相对较少；

2017.05.26
==========================================
1. 改进直播服务器，每隔5分钟汇报，10分钟没有汇报，自动断开；
2. 改进后台网站，可以查询监控中转服务器，存储服务器，直播服务器，所有服务器的状态；
3. 改进中转服务器，可以检测直播服务器超时，处理直播服务器退出。

2017.05.12
==========================================
1. 新增 build-htdocs.sh，单独为网站代码打包。
2. 新增 build-transmit.sh，单独为转发服务器打包。

2017.05.11
==========================================
1. /etc/rc.local 在6.8版本中改成了 /etc/rc.d/rc.local
2. 需要将开机启动部分修改为 /etc/rc.d/rc.local
3. storage => 新增软链接 => ln -s /fdfs/storage/data /fdfs/storage/data/M00
4. storage => 新增自动启动
5. tracker => 新增自动启动

2017.05.10
==========================================
1. mysql => 修改了源代码 mysql_com.h，默认字符集由'auto'改成'utf8'，这样就能在使用命令行时不会出现问题；
2. mysql => 使用 myisamchk -c -r 修复数据表；
3. phpMyAdmin => 屏蔽了 function.js 里面有关版本检测的代码；
4. phpMyAdmin => 修改 config.default.php 将 localhost 改成 127.0.0.1
5. transmit => 修改了日志存放位置的代码，跟执行程序放在一起 transmit.log

SSH Secure Shell 解决中文乱码的终极方法：
=============================================================
1. http://www.cnblogs.com/52linux/archive/2012/03/24/2415082.html
2. #vi /etc/sysconfig/i18n
LANG="zh_CN.GB18030"  
LANGUAGE="zh_CN.GB18030:zh_CN.GB2312:zh_CN"  
SUPPORTED="zh_CN.GB18030:zh_CN:zh:en_US.UTF-8:en_US:en"  
SYSFONT="lat0-sun16"
