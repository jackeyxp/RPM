#!/bin/bash

center_web="https://myhaoyi.com"

root_path="/root"
down_path=$root_path/download
down_limit="--limit-rate=100k"

mkdir -p $down_path

function install_nginx_all()
{
  echo "--------- [OK] begin install nginx-all ---------"
  if [ -f "/weike/nginx/sbin/nginx" ]; then
    echo "--------- [ERR] nginx has been installed ---------"
    return 0
  fi

  down_file="nginx-all-1.10.2-1.x86_64.rpm"
  echo "--------- [OK] get ${down_file} ---------"
  wget $down_limit -c -P $down_path $center_web/download/$down_file
  cd $down_path
  rpm -ivh $down_file
}

function install_nginx_tracker()
{
  echo "--------- [OK] begin install nginx-tracker ---------"
  if [ -f "/weike/nginx/sbin/nginx" ]; then
    echo "--------- [ERR] nginx has been installed ---------"
    return 0
  fi

  down_file="nginx-tracker-1.10.2-1.x86_64.rpm"
  echo "--------- [OK] get ${down_file} ---------"
  wget $down_limit -c -P $down_path $center_web/download/$down_file
  cd $down_path
  rpm -ivh $down_file
}

function install_nginx_storage()
{
  echo "--------- [OK] begin install nginx-storage ---------"
  if [ -f "/weike/nginx/sbin/nginx" ]; then
    echo "--------- [ERR] nginx has been installed ---------"
    return 0
  fi

  down_file="nginx-storage-1.10.2-1.x86_64.rpm"
  echo "--------- [OK] get ${down_file} ---------"
  wget $down_limit -c -P $down_path $center_web/download/$down_file
  cd $down_path
  rpm -ivh $down_file
}

function install_php()
{
  echo "--------- [OK] begin install php ---------"
  if [ -f "/weike/php/bin/php" ]; then
    echo "--------- [ERR] php has been installed ---------"
    return 0
  fi

  down_file="php-5.6.30-1.x86_64.rpm"
  echo "--------- [OK] get ${down_file} ---------"
  wget $down_limit -c -P $down_path $center_web/download/$down_file
  cd $down_path
  rpm -ivh $down_file
}

function install_mysql()
{
  echo "--------- [OK] begin install mysql ---------"
  if [ -f "/weike/mysql/bin/mysql" ]; then
    echo "--------- [ERR] mysql has been installed ---------"
    return 0
  fi

  down_file="mysql-5.5.3-1.x86_64.rpm"
  echo "--------- [OK] get ${down_file} ---------"
  wget $down_limit -c -P $down_path $center_web/download/$down_file
  cd $down_path
  rpm -ivh $down_file
}

function install_srs()
{
  echo "--------- [OK] begin install srs ---------"
  if [ -f "/weike/srs/bin/srs" ]; then
    echo "--------- [ERR] srs has been installed ---------"
    return 0
  fi

  down_file="srs-2.0.243-1.x86_64.rpm"
  echo "--------- [OK] get ${down_file} ---------"
  wget $down_limit -c -P $down_path $center_web/download/$down_file
  cd $down_path
  rpm -ivh $down_file
}

function install_tracker()
{
  echo "--------- [OK] begin install tracker ---------"
  if [ -f "/weike/tracker/bin/fdfs_trackerd" ]; then
    echo "--------- [ERR] tracker has been installed ---------"
    return 0
  fi

  down_file="tracker-5.0.9-1.x86_64.rpm"
  echo "--------- [OK] get ${down_file} ---------"
  wget $down_limit -c -P $down_path $center_web/download/$down_file
  cd $down_path
  rpm -ivh $down_file
}

function install_storage()
{
  echo "--------- [OK] begin install storage ---------"
  if [ -f "/weike/storage/bin/fdfs_storaged" ]; then
    echo "--------- [ERR] storage has been installed ---------"
    return 0
  fi

  down_file="storage-5.0.9-1.x86_64.rpm"
  echo "--------- [OK] get ${down_file} ---------"
  wget $down_limit -c -P $down_path $center_web/download/$down_file
  cd $down_path
  rpm -ivh $down_file
}

function install_transmit()
{
  echo "--------- [OK] begin install transmit ---------"
  if [ -f "/weike/transmit/transmit" ]; then
    echo "--------- [ERR] transmit has been installed ---------"
    return 0
  fi

  down_file="transmit-1.0.1-1.x86_64.rpm"
  echo "--------- [OK] get ${down_file} ---------"
  wget $down_limit -c -P $down_path $center_web/download/$down_file
  cd $down_path
  rpm -ivh $down_file
}

function install_monitor_web()
{
  echo "--------- [OK] begin install monitor web ---------"
  if [ -d "/weike/htdocs" ]; then
    echo "--------- [ERR] monitor web has been installed ---------"
    return 0
  fi

  down_file="monitor-0.0.1-1.x86_64.rpm"
  echo "--------- [OK] get ${down_file} ---------"
  wget $down_limit -c -P $down_path $center_web/download/$down_file
  cd $down_path
  rpm -ivh $down_file
}

function install_recorder_web()
{
  echo "--------- [OK] begin install recorder web ---------"
  if [ -d "/weike/htdocs" ]; then
    echo "--------- [ERR] recorder web has been installed ---------"
    return 0
  fi

  down_file="recorder-0.0.1-1.x86_64.rpm"
  echo "--------- [OK] get ${down_file} ---------"
  wget $down_limit -c -P $down_path $center_web/download/$down_file
  cd $down_path
  rpm -ivh $down_file
}

function install_cloud_monitor()
{
  echo "--------- [OK] begin install cloud monitor ---------"
}

function install_cloud_recorder()
{
  echo "--------- [OK] begin install cloud recorder ---------"
}

select myInstall in "install cloud monitor" "install cloud recorder" \
  "install nginx-all" "install nginx-tracker" "install nginx-storage" \
  "install php" "install mysql" "install srs" "install transmit" \
  "install tracker" "install storage" \
  "install monitor web" "install recorder web" \
  "quit"
do 
  case $myInstall in
  "install cloud monitor")
    install_cloud_monitor
    ;;
  "install cloud recorder")
    install_cloud_recorder
    ;;
  "install nginx-all")
    install_nginx_all
    ;;
  "install nginx-tracker")
    install_nginx_tracker
    ;;
  "install nginx-storage")
    install_nginx_storage
    ;;
  "install php")
    install_php
    ;;
  "install mysql")
    install_mysql
    ;;
  "install srs")
    install_srs
    ;;
  "install tracker")
    install_tracker
    ;;
  "install storage")
    install_storage
    ;;
  "install transmit")
    install_transmit
    ;;
  "install monitor web")
    install_monitor_web
    ;;
  "install recorder web")
    install_recorder_web
    ;;
  "quit")
    echo "quit"
    break
    ;;
  esac
done
