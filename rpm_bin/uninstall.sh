#!/bin/bash

arrList=(php transmit srs mysql-5.5.3 nginx tracker storage htdocs)

killall -1 /weike/php/sbin/php-fpm
sleep 1s
killall -1 /weike/transmit/transmit
sleep 1s
/weike/srs/etc/srs stop
sleep 1s
/weike/mysql/sbin/mysql stop
sleep 1s
/weike/nginx/sbin/nginx -p /weike/nginx/ -s stop
sleep 1s
/weike/tracker/bin/fdfs_trackerd /etc/fdfs/tracker.conf stop
sleep 1s
/weike/storage/bin/fdfs_storaged /etc/fdfs/storage.conf stop
sleep 1s

for(( i=0; i< ${#arrList[@]}; i++))
do
  myResult=`rpm -qa | grep ${arrList[i]}`
  if [ ! -z $myResult ]; then
    echo "=== uninstall $myResult ==="
    rpm -e $myResult
    sleep 1s
  fi
done
