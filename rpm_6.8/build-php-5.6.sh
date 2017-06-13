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

#copy web-server source/spec to rpm directory...
cp -afp $web_path/php-5.6.30.tar.gz $root_path/SOURCES/
rm -rf $web_path/php-5.6.30.tar.gz

#build all the rpm...
rpmbuild -ba ../rpm_spec/php-5.6.spec

#cp rpm to rpm_bin
cp -afp $root_path/RPMS/x86_64/php-5.6.30-1.x86_64.rpm ../rpm_bin/
