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

#prepare php/mysql/nginx source code...
tar cfvz php-5.6.30.tar.gz php-5.6.30 --exclude=CVS
tar cfvz mysql-5.5.3.tar.gz mysql-5.5.3 --exclude=CVS
tar cfvz tracker-5.0.9.tar.gz tracker-5.0.9 --exclude=CVS
tar cfvz nginx-tracker-1.10.2.tar.gz nginx-tracker-1.10.2 --exclude=CVS
tar cfvz storage-5.0.9.tar.gz storage-5.0.9 --exclude=CVS
tar cfvz nginx-storage-1.10.2.tar.gz nginx-storage-1.10.2 --exclude=CVS
tar cfvz nginx-all-1.10.2.tar.gz nginx-all-1.10.2 --exclude=CVS
tar cfvz srs-2.0.243.tar.gz srs-2.0.243 --exclude=CVS
tar cfvz transmit-1.0.1.tar.gz transmit-1.0.1 --exclude=CVS

#copy web-server source/spec to rpm directory...
cp -afp $web_path/php-5.6.30.tar.gz $root_path/SOURCES/
rm -rf $web_path/php-5.6.30.tar.gz
cp -afp $web_path/mysql-5.5.3.tar.gz $root_path/SOURCES/
rm -rf $web_path/mysql-5.5.3.tar.gz
cp -afp $web_path/tracker-5.0.9.tar.gz $root_path/SOURCES/
rm -rf $web_path/tracker-5.0.9.tar.gz
cp -afp $web_path/nginx-tracker-1.10.2.tar.gz $root_path/SOURCES/
rm -rf $web_path/nginx-tracker-1.10.2.tar.gz
cp -afp $web_path/storage-5.0.9.tar.gz $root_path/SOURCES/
rm -rf $web_path/storage-5.0.9.tar.gz
cp -afp $web_path/nginx-storage-1.10.2.tar.gz $root_path/SOURCES/
rm -rf $web_path/nginx-storage-1.10.2.tar.gz
cp -afp $web_path/nginx-all-1.10.2.tar.gz $root_path/SOURCES/
rm -rf $web_path/nginx-all-1.10.2.tar.gz
cp -afp $web_path/srs-2.0.243.tar.gz $root_path/SOURCES/
rm -rf $web_path/srs-2.0.243.tar.gz
cp -afp $web_path/transmit-1.0.1.tar.gz $root_path/SOURCES/
rm -rf $web_path/transmit-1.0.1.tar.gz

#build all the rpm...
rpmbuild -ba ../rpm_spec/php-5.6.spec
rpmbuild -ba ../rpm_spec/mysql.spec
rpmbuild -ba ../rpm_spec/tracker.spec
rpmbuild -ba ../rpm_spec/nginx-tracker.spec
rpmbuild -ba ../rpm_spec/storage.spec
rpmbuild -ba ../rpm_spec/nginx-storage.spec
rpmbuild -ba ../rpm_spec/nginx-all.spec
rpmbuild -ba ../rpm_spec/srs.spec
rpmbuild -ba ../rpm_spec/transmit.spec

#cp rpm to rpm_bin
cp -afp $root_path/RPMS/x86_64/php-5.6.30-1.x86_64.rpm ../rpm_bin/
cp -afp $root_path/RPMS/x86_64/mysql-5.5.3-1.x86_64.rpm ../rpm_bin/
cp -afp $root_path/RPMS/x86_64/tracker-5.0.9-1.x86_64.rpm ../rpm_bin/
cp -afp $root_path/RPMS/x86_64/nginx-tracker-1.10.2-1.x86_64.rpm ../rpm_bin/
cp -afp $root_path/RPMS/x86_64/storage-5.0.9-1.x86_64.rpm ../rpm_bin/
cp -afp $root_path/RPMS/x86_64/nginx-storage-1.10.2-1.x86_64.rpm ../rpm_bin/
cp -afp $root_path/RPMS/x86_64/nginx-all-1.10.2-1.x86_64.rpm ../rpm_bin/
cp -afp $root_path/RPMS/x86_64/srs-2.0.243-1.x86_64.rpm ../rpm_bin/
cp -afp $root_path/RPMS/x86_64/transmit-1.0.1-1.x86_64.rpm ../rpm_bin/
cp -afp $web_path/transmit-1.0.1/json-c-0.10-1.x86_64.rpm ../rpm_bin/