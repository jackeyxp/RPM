#!/bin/bash

rpm -ivh nginx-all-1.10.2-1.x86_64.rpm
rpm -ivh php-5.6.30-1.x86_64.rpm
rpm -ivh mysql-5.5.3-1.x86_64.rpm
rpm -ivh tracker-5.0.9-1.x86_64.rpm
rpm -ivh storage-5.0.9-1.x86_64.rpm
rpm -ivh srs-2.0.243-1.x86_64.rpm
rpm -ivh transmit-1.0.1-1.x86_64.rpm
rpm -ivh htdocs-0.0.1-1.x86_64.rpm

sh config.sh
