#!/bin/bash

BASE_DIR=/usr/local/openvas
DATA_DIR=/opt

PATH_DIRS=$BASE_DIR/depends/bin:$BASE_DIR/depends/sbin:$BASE_DIR/postgresql/bin\
:$BASE_DIR/python/bin:$BASE_DIR/perl/bin:$BASE_DIR/gvm/bin:$BASE_DIR/gvm/sbin\
:/usr/bin:/bin:/usr/sbin:/sbin
LIB_DIRS=$BASE_DIR/depends/lib:$BASE_DIR/postgresql/lib:$BASE_DIR/gvm/lib

export PATH=$PATH_DIRS
export LD_LIBRARY_PATH=$LIB_DIRS

[ "`id -u`" -ne "0" ] && { echo "$0 must be run as root!"; exit 1;}


#redis vars
redis_pid_file=$BASE_DIR/gvm/run/redis/redis-server.pid
redis_conf_file=$DATA_DIR/var/conf/redis-openvas.conf

function start_redis() {
 echo "start redis_service..."

 $BASE_DIR/depends/bin/redis-server $redis_conf_file
 ret=$?

 if [ $ret -ne 0 ]; then
  echo "start redis-service failed!"
 fi

 return $ret
}

function stop_redis() {
 echo "stop redis_service..."

 [ -e  $redis_pid_file ] && (
   redis_pid=$(cat $redis_pid_file)
   [ -n "$redis_pid" ] && kill -TERM $redis_pid
   
   sleep 2
  ) || echo "redis is not running."

 killall redis-server 2>/dev/null
 return 0
}

#pg vars
pg_data_dir=$DATA_DIR/pg_data
pg_bin=$BASE_DIR/postgresql/bin/pg_ctl
pg_pid_file=$pg_data_dir/postmaster.pid

function start_pg() {
 echo "start postgres_service..."

 if [ -e "$pg_data_dir" ]; then
  sudo -n -u postgres LD_LIBRARY_PATH=$LIB_DIRS -s $pg_bin -D $pg_data_dir start 2>/dev/null < /dev/null
  
  sleep 2
 fi

 return $?
}

function stop_pg() {
 echo "stop postgres_service..."

 if [ -e "$pg_data_dir" ]; then
  sudo -n -u postgres LD_LIBRARY_PATH=$LIB_DIRS -s $pg_bin -D $pg_data_dir stop -s -m fast 2>/dev/null < /dev/null
  
  sleep 2
 fi

 killall postgres 2>/dev/null
 return $?
}

#gvm vars
vas_pid_file=$BASE_DIR/gvm/run/ospd/ospd.pid
vas_sock_file=$BASE_DIR/gvm/run/ospd/ospd.sock
vas_lock_dir=$BASE_DIR/gvm/run/ospd
vas_conf_file=$BASE_DIR/gvm/run/ospd/ospd-openvas.conf
vas_log_conf=$BASE_DIR/gvm/run/ospd/ospd-logging.conf
vas_log_file=$BASE_DIR/gvm/var/log/gvm/ospd-openvas.log

gvm_pid_file=$BASE_DIR/gvm/run/gvm/gvmd.pid

vas_param="--config $vas_conf_file --log-config $vas_log_conf --log-file $vas_log_file\
 --unix-socket $vas_sock_file --socket-mode 00777 --lock-file-dir $vas_lock_dir --pid-file $vas_pid_file"

gvm_param="--db-host=/usr/local/openvas/gvm/run/postgresql --listen=0.0.0.0 --port=9390"

function start_gvm() {
 echo "start gvm_service..."
 
 [ -e  $redis_pid_file ] && [ $(ps -ef|grep "`cat $redis_pid_file`"|grep 'redis-server'|grep -v grep|wc -l) -ne 0 ] || {
  echo "redis must be run first!"; return 1;}
  
 [ -e  $pg_pid_file ] && [ $(ps -ef|grep "`cat $pg_pid_file`"|grep 'postgres'|grep -v grep|wc -l) -ne 0 ] || {
  echo "postgresql must be run first!"; return 1;}

 $BASE_DIR/python/bin/ospd-openvas $vas_param
 ret1=$?
 if [ $ret1 -ne 0 ]; then
  echo "start ospd-openvas failed!"
  return $ret1
 fi

 sleep 1

 $BASE_DIR/gvm/sbin/gvmd $gvm_param
 ret2=$?
 if [ $ret2 -ne 0 ]; then
  echo "start gvmd failed!"
  return $ret2
 fi

 return 0
}

function stop_gvm() {
 echo "stop gvm_service..."

 [ -e  $gvm_pid_file ] && (
   gvm_pid=$(cat $gvm_pid_file)
   [ -n "$gvm_pid" ] && kill -TERM $gvm_pid || echo "stop gvmd failed!"
   
   sleep 2
  ) || echo "gvmd is not running."

 killall -9 gvmd

 [ -f $vas_pid_file ] && (
   vas_pid=$(cat $vas_pid_file)
   [ -n "$vas_pid" ] && kill -TERM $vas_pid || echo "stop ospd-openvas failed!"
   
   sleep 2
  ) ||  echo "ospd-openvas is not running."

 killall -9 ospd-openvas
 killall -9 openvas

 return 0
}

