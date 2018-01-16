CentOS6.8版本 => 打包核心更新日志
==========================================

2018.01.16
==========================================
1. mysql => data => monitor => wk_camera 新增 use_tcp
2. mysql => data => haoyi   => wk_camera 新增 use_tcp

2018.01.10
==========================================
1. mysql => data/haoyi => wk_camera里的shared字段拼写错误；

2018.01.07
==========================================
1. srs => 去掉rtmp用户数为0时的汇报机制，让中转服务器(transmit)全部通过超时机制去删除用户；

2017.12.26
==========================================
1. build-cloud.sh => 自动获取云录播、云监控配置的版本；并对打包结果做了统一规范重命名，避免混乱；

2017.12.22
==========================================
1. nginx-all/nginx-tracker => 新增API访问接口配置，具体查看 nginx.conf 配置

2017.12.18
==========================================
1. transmit => 解决假死，中断未通知的问题；所有连接加入超时检测机制，1分钟没有交互，删除连接；
2. transmit => 解决假死，中断未通知的问题；采集端每隔30秒请求在线通道列表，每个通道上的在线人数，让采集端保持在线，也能让采集端停止那些已经用户数为0的通道；
3. mysql/php => 将 libmysqlclient.so.16.0.0/libmysqlclient_r.so.16.0.0还原到5.5.3-m3版本，避免潜在的调用风险；
4. mysql => 修改[client]默认参数，避免停止数据库报错；default-character-set = utf8
5. mysql => 还原成 5.5.3-m3 之后，停止mysql时会出现字符集切换提示；

2017.12.15
==========================================
1. php/mysql => 找到了 mysql: unknown variable 'character-set-server=utf8' 的问题所在，是由于 libmysqlclient.so.16.0.0 版本的问题
2. php/mysql => libmysqlclient.so.16.0.0/libmysqlclient_r.so.16.0.0 降低到 5.1.73 就没有问题了，发现 myhaoyi.com 一直都用的5.1.73版本；

2017.12.14
==========================================
1. mysql => data => monitor => wk_camera 新增 device_show
2. mysql => data => haoyi   => wk_camera 新增 device_show

2017.12.10
==========================================
1. build-cloud.sh => 将所有的打包模块都放在了一个文件当中；
2. build-cloud.sh => 新增了云录播、云监控的整体打包方案；
3. transmit => 将json-c依赖包整合到了transmit当中，不用单独安装了；

2017.12.07
==========================================
1. mysql => data => monitor => 同步更新了云监控数据库
2. mysql => data => haoyi => 同步更新了云录播数据库

2017.11.21
==========================================
1. transmit => 新增flvjs的返回链接；目前支持 flvjs/rtmp/hls 三种流同时输出；
2. transmit => 所有类型的播放器Flash、HTML5都需要检测超时，Flash播放结束，有时不会汇报，必须使用超时检测，才能让采集端停止上传；
3. srs => 在 srs.conf 中默认开启 http-flv 流输出，以便支持 flvjs 的播放；

2017.11.07
==========================================
1. transmit => 新增了 addCamera | delCamera | modCamera 网页命令；
2. mysql => data => monitor => 云监控数据库单独分离；
3. mysql => data => haoyi => 云录播数据库单独分离；
4. monitor-0.0.1  => 云监控网站单独分离；
5. recorder-0.0.1 => 云录播网站单独分离；

2017.10.25
==========================================
1. srs => 新增配置web_report和web_local，满足汇报地址外部设置的情况，当直播服务器是映射地址时需要；
   web_report  x.x.x.x;
   web_local   1; // 默认1（需要从本地读取汇报地址），0（不需要本地读取，从web_report获取）

2017.10.23
==========================================
1. mysql => data => haoyi => 更新同步数据库字段

2017.10.18
==========================================
1. transmit => 新增了debug模式的日志，只打印不记录；
2. transmit => 修正了处理超时事件检测机制，每隔10秒钟就要处理一次，而不是只处理超时事件（会造成不发生超时事件就永远无法处理）；

2017.10.13
==========================================
1. transmit => 新增CCamera、CPlayer、CSrsServer，重建了它们之间的关联关系；
2. transmit => hls播放器每隔12秒汇报一次，30秒不汇报就超时删除；flash播放器只在第一次汇报（修改播放器类型），srs服务器会在flash播放器计数为0时汇报；
3. transmit => srs服务器5分钟还没有汇报就删除；系统每隔10秒遍历CSrsServer，检测播放器是否超时，删除CCamera的条件是Flash+HLS播放器都为0时；
4. transmit => 改进了错误日志记录方式，更直观明了；
5. transmit => 备注：后续还需要将CClient进一步优化，让各种终端的处理过程逻辑不要像现在这么混乱；

2017.10.11
==========================================
1. php => fastdfs_client.so => 去掉了 transmit.h 文件，避免与其它模块同步修改的问题；

2017.09.28
==========================================
1. srs => 8080端口传输.m3u8和.ts文件时，支持跨域访问
   srs传输.m3u8时 => protocol\srs_http_stack.cpp:349 => w->header()->set("Access-Control-Allow-Origin", "*");
   srs传输.ts时   => app\srs_app_http_stream.cpp:483 => w->header()->set("Access-Control-Allow-Origin", "*");
2. srs => 新增汇报数据 => hls_addr => IP:PORT

2017.08.21
==========================================
1. transmit => 判断超时时间设置为5分钟，以前是10分钟；
2. srs => 当汇报地址失败，每次按10秒汇报（加快汇报周期），汇报成功，每次按2分钟汇报（降低汇报频率）；

2017.07.28
==========================================
1. transmit => 读取数据失败，打印更详细的错误信息；
2. srs => 新增3个配置：汇报地址、汇报端口、汇报https开关；

2017.07.26
==========================================
1. php => fastdfs_client.so => 将发送数据缓存修改成64KB，便于录像课表一次性发送，避免缓存溢出；
2. transmit => 将接收、发送缓存修改成64KB，避免缓存溢出；
3. php_client => fastdfs_client => 将代码文档改成了unix格式；

2017.07.13
==========================================
1. php.ini => 关闭 always_populate_raw_post_data，对php基本的操作都有影响，当初是为了修改srs的http_hooks而修改的；
2. srs 的 http_hooks 的细节还需要修改，并不是我理解的那样调用，需要做调整；

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
2. mysql => 后续补充，修改 'auto' 为 'utf8' 的方式，还是会有问题，最终是将 libmysqlclient.so.16.0.0/libmysqlclient_r.so.16.0.0 降低到 5.1.73
2. mysql => 使用 myisamchk -c -r 修复数据表；
3. phpMyAdmin => 屏蔽了 function.js 里面有关版本检测的代码；
4. phpMyAdmin => 修改 config.default.php 将 localhost 改成 127.0.0.1
5. transmit => 修改了日志存放位置的代码，跟执行程序放在一起 transmit.log
6. mysql => root:Kuihua*#816

SSH Secure Shell 解决中文乱码的终极方法：
=============================================================
1. http://www.cnblogs.com/52linux/archive/2012/03/24/2415082.html
2. #vi /etc/sysconfig/i18n
LANG="zh_CN.GB18030"  
LANGUAGE="zh_CN.GB18030:zh_CN.GB2312:zh_CN"  
SUPPORTED="zh_CN.GB18030:zh_CN:zh:en_US.UTF-8:en_US:en"  
SYSFONT="lat0-sun16"
