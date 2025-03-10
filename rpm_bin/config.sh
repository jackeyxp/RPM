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

function checkport {
  #check must be a number...
  echo $1 | grep "^[0-9]*$" >/dev/null
  if [ $? -ne 0 ]; then
    return 1
  fi
  # 1 < webPort < 65535
  if [ $1 -ge 1 -a $1 -le 65535 ] ; then
    return 0
  else
    return 1
  fi
}

if [ "$1" == "auto" ]; then
  #auto find local valid IP address...
  myIP=`ifconfig -a|grep inet|grep -v 127.0.0.1|grep -v inet6|awk '{print $2}'|tr -d "addr:"`
  myWebPort=80
  echo "$myIP:$myWebPort is auto detect..."
else
  #check input IP...
  while [ 1 ]
  do
    read -p "Enter Current Server Web IP: " webIP
    checkip $webIP
    # > 0 then not a IP...
    if [ $? -gt 0 ]; then
      echo "$webIP is not a valid IP, Please try again..."
      continue
    fi
    # == 0 then IP is OK...
    echo "$webIP is valid IP...OK."
    break
  done
  #check input Port...
  while [ 1 ]
  do
    read -p "Enter Current Server Web Port: " webPort
    checkport $webPort
    # > 0 then not a Port...
    if [ $? -gt 0 ]; then
      echo "$webPort is not a valid Port, Please try again..."
      continue
    fi
    # == 0 then Port is OK...
    echo "$webPort is valid Port...OK."
    break
  done

  #check input param is valid IP Address...
  myIP=$webIP
  myWebPort=$webPort
fi

checkip $myIP
if [ $? -gt 0 ]; then
  echo "param1: '$myIP' is not a valid IP, Please try again..."
  exit
fi

#check input param is valid number...
echo $myWebPort | grep "^[0-9]*$" >/dev/null
if [[ $? -ne 0 || ! -n "$myWebPort" ]]; then
  echo "param2: '$myWebPort' is not a number, Please try again..."
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
else
  echo "=== srs not install ==="
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
else
  echo "=== tracker not install ==="
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
else
  echo "=== storage not install ==="
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
else
  echo "=== mod_fastdfs not install ==="
fi

# curl => save to database...
myWxapi="/weike/htdocs/wxapi.php"
myCmd="http://localhost/wxapi.php/Index/config/tracker/$myIP:22122/transmit/$myIP:21001/webport/$myWebPort"
if [ -f "$myWxapi" ]; then
  echo "=== [curl] $myCmd ==="
  curl $myCmd
  sleep 2s
else
  echo "=== web not install ==="
fi
