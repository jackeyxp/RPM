#!/bin/sh

#define common param...
web_path="${PWD}"
root_path="/root/rpmbuild"

rm -rf $root_path
mkdir -p $root_path/{BUILD,RPMS,SOURCES,SPECS,SRPMS}

#anyone can excute...
chmod -R 755 $web_path

#enter compile directory...
cd $web_path

#mask debug for rpm builder...
echo '%debug_package %{nil}'>>~/.rpmmacros

function build_php()
{
  version="php-5.6.30"
  echo "build php..."
  #prepare source code...
  tar cfvz ${version}.tar.gz ${version}
  #copy source/spec to rpm directory...
  cp -afp $web_path/${version}.tar.gz $root_path/SOURCES/
  rm -rf $web_path/${version}.tar.gz
  #build the rpm...
  rpmbuild -ba ../rpm_spec/php-5.6.spec
  #cp rpm to rpm_bin
  cp -afp $root_path/RPMS/x86_64/${version}-1.x86_64.rpm ../rpm_bin/
}

function build_mysql()
{
  version="mysql-5.5.3"
  echo "build mysql..."
  #prepare source code...
  tar cfvz ${version}.tar.gz ${version}
  #copy source/spec to rpm directory...
  cp -afp $web_path/${version}.tar.gz $root_path/SOURCES/
  rm -rf $web_path/${version}.tar.gz
  #build the rpm...
  rpmbuild -ba ../rpm_spec/mysql.spec
  #cp rpm to rpm_bin
  cp -afp $root_path/RPMS/x86_64/${version}-1.x86_64.rpm ../rpm_bin/
}

function build_nginx_all()
{
  version="nginx-all-1.10.2"
  echo "build nginx-all..."
  #prepare source code...
  tar cfvz ${version}.tar.gz ${version}
  #copy source/spec to rpm directory...
  cp -afp $web_path/${version}.tar.gz $root_path/SOURCES/
  rm -rf $web_path/${version}.tar.gz
  #build the rpm...
  rpmbuild -ba ../rpm_spec/nginx-all.spec
  #cp rpm to rpm_bin
  cp -afp $root_path/RPMS/x86_64/${version}-1.x86_64.rpm ../rpm_bin/
}

function build_nginx_tracker()
{
  version="nginx-tracker-1.10.2"
  echo "build nginx-tracker..."
  #prepare source code...
  tar cfvz ${version}.tar.gz ${version}
  #copy source/spec to rpm directory...
  cp -afp $web_path/${version}.tar.gz $root_path/SOURCES/
  rm -rf $web_path/${version}.tar.gz
  #build the rpm...
  rpmbuild -ba ../rpm_spec/nginx-tracker.spec
  #cp rpm to rpm_bin
  cp -afp $root_path/RPMS/x86_64/${version}-1.x86_64.rpm ../rpm_bin/
}

function build_nginx_storage()
{
  version="nginx-storage-1.10.2"
  echo "build nginx-storage..."
  #prepare source code...
  tar cfvz ${version}.tar.gz ${version}
  #copy source/spec to rpm directory...
  cp -afp $web_path/${version}.tar.gz $root_path/SOURCES/
  rm -rf $web_path/${version}.tar.gz
  #build the rpm...
  rpmbuild -ba ../rpm_spec/nginx-storage.spec
  #cp rpm to rpm_bin
  cp -afp $root_path/RPMS/x86_64/${version}-1.x86_64.rpm ../rpm_bin/
}

function build_srs()
{
  version="srs-2.0.243"
  echo "build srs..."
  #prepare source code...
  tar cfvz ${version}.tar.gz ${version}
  #copy source/spec to rpm directory...
  cp -afp $web_path/${version}.tar.gz $root_path/SOURCES/
  rm -rf $web_path/${version}.tar.gz
  #build the rpm...
  rpmbuild -ba ../rpm_spec/srs.spec
  #cp rpm to rpm_bin
  cp -afp $root_path/RPMS/x86_64/${version}-1.x86_64.rpm ../rpm_bin/
}

function build_tracker()
{
  version="tracker-5.0.9"
  echo "build tracker..."
  #prepare source code...
  tar cfvz ${version}.tar.gz ${version}
  #copy source/spec to rpm directory...
  cp -afp $web_path/${version}.tar.gz $root_path/SOURCES/
  rm -rf $web_path/${version}.tar.gz
  #build the rpm...
  rpmbuild -ba ../rpm_spec/tracker.spec
  #cp rpm to rpm_bin
  cp -afp $root_path/RPMS/x86_64/${version}-1.x86_64.rpm ../rpm_bin/
}

function build_storage()
{
  version="storage-5.0.9"
  echo "build storage..."
  #prepare source code...
  tar cfvz ${version}.tar.gz ${version}
  #copy source/spec to rpm directory...
  cp -afp $web_path/${version}.tar.gz $root_path/SOURCES/
  rm -rf $web_path/${version}.tar.gz
  #build the rpm...
  rpmbuild -ba ../rpm_spec/storage.spec
  #cp rpm to rpm_bin
  cp -afp $root_path/RPMS/x86_64/${version}-1.x86_64.rpm ../rpm_bin/
}

function build_transmit()
{
  version="transmit-1.0.1"
  echo "build transmit..."
  #prepare source code...
  tar cfvz ${version}.tar.gz ${version}
  #copy source/spec to rpm directory...
  cp -afp $web_path/${version}.tar.gz $root_path/SOURCES/
  rm -rf $web_path/${version}.tar.gz
  #build the rpm...
  rpmbuild -ba ../rpm_spec/transmit.spec
  #cp rpm to rpm_bin
  cp -afp $root_path/RPMS/x86_64/${version}-1.x86_64.rpm ../rpm_bin/
}

function build_monitor_web()
{
  version="monitor-0.0.1"
  echo "build monitor web..."
  #prepare source code...
  tar cfvz ${version}.tar.gz ${version}
  #copy source/spec to rpm directory...
  cp -afp $web_path/${version}.tar.gz $root_path/SOURCES/
  rm -rf $web_path/${version}.tar.gz
  #build the rpm...
  rpmbuild -ba ../rpm_spec/monitor.spec
  #cp rpm to rpm_bin
  cp -afp $root_path/RPMS/x86_64/${version}-1.x86_64.rpm ../rpm_bin/
}

function build_recorder_web()
{
  version="recorder-0.0.1"
  echo "build recorder web..."
  #prepare source code...
  tar cfvz ${version}.tar.gz ${version}
  #copy source/spec to rpm directory...
  cp -afp $web_path/${version}.tar.gz $root_path/SOURCES/
  rm -rf $web_path/${version}.tar.gz
  #build the rpm...
  rpmbuild -ba ../rpm_spec/recorder.spec
  #cp rpm to rpm_bin
  cp -afp $root_path/RPMS/x86_64/${version}-1.x86_64.rpm ../rpm_bin/
}

function build_cloud_monitor()
{
  config="monitor-0.0.1/wxapi/Conf/config.php"
  version=$(awk -F"'" '/VERSION/{print $4}' $config)
  echo "build cloud-monitor-${version}"
  cd ../rpm_bin/
  tar cfvz cloud-monitor-${version}.tar.gz \
    nginx-all-1.10.2-1.x86_64.rpm php-5.6.30-1.x86_64.rpm mysql-5.5.3-1.x86_64.rpm \
    tracker-5.0.9-1.x86_64.rpm storage-5.0.9-1.x86_64.rpm \
    srs-2.0.243-1.x86_64.rpm transmit-1.0.1-1.x86_64.rpm \
    config.sh install_monitor.sh uninstall.sh \
    monitor-0.0.1-1.x86_64.rpm
  cd $web_path
}

function build_cloud_recorder()
{
  config="recorder-0.0.1/wxapi/Conf/config.php"
  version=$(awk -F"'" '/VERSION/{print $4}' $config)
  echo "build cloud-recorder-${version}.tar.gz"
  cd ../rpm_bin/
  tar cfvz cloud-recorder-${version}.tar.gz \
    nginx-all-1.10.2-1.x86_64.rpm php-5.6.30-1.x86_64.rpm mysql-5.5.3-1.x86_64.rpm \
    tracker-5.0.9-1.x86_64.rpm storage-5.0.9-1.x86_64.rpm \
    srs-2.0.243-1.x86_64.rpm transmit-1.0.1-1.x86_64.rpm \
    config.sh install_recorder.sh uninstall.sh \
    recorder-0.0.1-1.x86_64.rpm
  cd $web_path
}

function build_all()
{
  echo "build all rpm package..."
  build_php
  build_mysql
  build_nginx_all
  build_nginx_tracker
  build_nginx_storage
  build_srs
  build_tracker
  build_storage
  build_transmit
  build_monitor_web
  build_recorder_web
}

#select build monitor or recorder...
select myBuild in "build all" "build php" "build mysql" \
  "build nginx-all" "build nginx-tracker" "build nginx-storage" \
  "build srs" "build tracker" "build storage" "build transmit" \
  "build monitor web" "build recorder web" \
  "build cloud monitor" "build cloud recorder" \
  "quit"
do 
  case $myBuild in
  "build all")
    build_all
    ;;
  "build php")
    build_php
    ;;
  "build mysql")
    build_mysql
    ;;
  "build nginx-all")
    build_nginx_all
    ;;
  "build nginx-tracker")
    build_nginx_tracker
    ;;
  "build nginx-storage")
    build_nginx_storage
    ;;
  "build srs")
    build_srs
    ;;
  "build tracker")
    build_tracker
    ;;
  "build storage")
    build_storage
    ;;
  "build transmit")
    build_transmit
    ;;
  "build monitor web")
    build_monitor_web
    ;;
  "build recorder web")
    build_recorder_web
    ;;
  "build cloud monitor")
    build_cloud_monitor
    ;;
  "build cloud recorder")
    build_cloud_recorder
    ;;
  "quit")
    echo "quit"
    break
    ;;
  esac
done
