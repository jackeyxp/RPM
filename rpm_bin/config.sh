#!/bin/bash

function checkip {
  #check dot number...
  dot=`echo $1 | awk -F '.' '{print NF-1}'`
  if [ $dot -ne 3 ]; then
    return 1
  fi
  count=0
  #check input string...
  for var in `echo $1 | awk -F. '{print $1, $2, $3, $4}'`; do
    #check must be a number...
    echo $var | grep "^[0-9]*$" >/dev/null
    if [ $? -ne 0 ]; then
      return 1
    fi
    #must be 0 <= var <= 255
    if [ $var -ge 0 -a $var -le 255 ] ; then
      ((count=count+1))
      continue
    else
      return 1
    fi
  done
  #count must equal 4...
  if [ $count -eq 4 ]; then
    return 0
  else
    return 1
  fi
}

#check input param is valid IP Address...
myIP=$1
checkip $myIP
if [ $? -gt 0 ]; then
  echo "'$myIP' is not a valid IP, Please try agin..."
  exit
fi

# srs => /weike/srs/conf/srs.conf => web_addr 192.168.1.xx;
mySrs="/weike/srs/conf/srs.conf"
if [ -f "$mySrs" ]; then
  echo "=== [SRS] $mySrs ==="
  sed -i "s/^web_addr.*$/web_addr $myIP;/" $mySrs
  echo "=== [SRS] restart ==="
  /weike/srs/etc/srs restart
  sleep 2s
fi

# fdfs-client => /etc/fdfs/client.conf => tracker_server=192.168.1.xx:22122
myClient="/etc/fdfs/client.conf"
if [ -f "$myClient" ]; then
  echo "=== [php fdfs client] $myClient ==="
  sed -i "s/^tracker_server=.*$/tracker_server=$myIP:22122/" $myClient
  echo "=== [php fdfs client] restart php-fpm ==="
  killall -1 /weike/php/sbin/php-fpm
  /weike/php/sbin/php-fpm
  sleep 2s
fi

# fdfs-storage => /etc/fdfs/storage.conf => tracker_server=192.168.1.xx:22122
myStorage="/etc/fdfs/storage.conf"
myData="/fdfs/storage/data"
myLink="/fdfs/storage/data/M00"
if [ -f "$myStorage" ]; then
  echo "=== [fdfs storage] $myStorage ==="
  sed -i "s/^tracker_server=.*$/tracker_server=$myIP:22122/" $myStorage
  echo "=== [fdfs storage] restart ==="
  /weike/storage/bin/fdfs_storaged /etc/fdfs/storage.conf restart
  sleep 3s
  # two condition must use [[ ]]
  if [[ -d "$myData" && ! -L "$myLink" ]]; then
    echo "=== [fdfs storage] build soft link ==="
    ln -s $myData $myLink
  fi
fi

# nginx => /etc/fdfs/mod_fastdfs.conf => tracker_server=192.168.1.xx:22122
myNginx="/etc/fdfs/mod_fastdfs.conf"
if [ -f "$myNginx" ]; then
  echo "=== [nginx] $myNginx ==="
  sed -i "s/^tracker_server=.*$/tracker_server=$myIP:22122/" $myNginx
  echo "=== [nginx] restart ==="
  /weike/nginx/sbin/nginx -p /weike/nginx/ -s stop
  sleep 1s
  /weike/nginx/sbin/nginx -p /weike/nginx/
  sleep 1s
fi

# curl => save to database...
myWxapi="/weike/htdocs/wxapi.php"
myCmd="http://localhost/wxapi.php/Index/config/tracker/$myIP:22122/transmit/$myIP:21001"
if [ -f "$myWxapi" ]; then
  echo "=== [curl] $myCmd ==="
  curl $myCmd
  sleep 2s
fi
