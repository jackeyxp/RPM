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

#prepare srs source code...
tar cfvz srs-2.0.243.tar.gz srs-2.0.243 --exclude=CVS

#copy srs source/spec to rpm directory...
cp -afp $web_path/srs-2.0.243.tar.gz $root_path/SOURCES/
rm -rf $web_path/srs-2.0.243.tar.gz

#build all the rpm...
rpmbuild -ba ../rpm_spec/srs.spec

#cp rpm to rpm_bin
cp -afp $root_path/RPMS/x86_64/srs-2.0.243-1.x86_64.rpm ../rpm_bin/
