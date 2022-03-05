#!/bin/bash

base_dir="/usr/local/openvas"
data_root=/opt
tmp_dir=$data_root/.tmp

function deal_data() {

tar xf setup_vas.tar -C  $tmp_dir 2>/dev/null || { echo "decompress package failed!"; return 1;}

tar zxf $tmp_dir/dep.tar.gz -C $tmp_dir 2>/dev/null || { echo "decompress dep.tar.gz failed!"; return 2;}
tar zxf $tmp_dir/gvm.tar.gz -C $tmp_dir 2>/dev/null || { echo "decompress gvm.tar.gz failed!"; return 2;}

tar zxf $tmp_dir/pg_data.tar.gz -C $tmp_dir 2>/dev/null || { echo "decompress pg_data.tar.gz failed!"; return 2;}
tar zxf $tmp_dir/var.tar.gz -C $tmp_dir 2>/dev/null || { echo "decompress var.tar.gz failed!"; return 2;}

[ -f $tmp_dir/nasl.tar.gz ]  && { tar zxf $tmp_dir/nasl.tar.gz -C $tmp_dir/var/lib/openvas 2>/dev/null || { echo "decompress nasl.tar.gz failed!"; return 2;} }

chown -R root:root $tmp_dir/depends $tmp_dir/gvm $tmp_dir/perl $tmp_dir/postgresql $tmp_dir/python $tmp_dir/var 2>/dev/null || { echo "chown dirs failed!"; return 3;}
chown -R postgres:root $tmp_dir/pg_data 2>/dev/null || { echo "chown pg_data dirs failed!"; return 3;}
chmod -R 777 $tmp_dir/var/log/gvm $tmp_dir/var/run 2>/dev/null

#move data
rm -rf $data_root/var $data_root/pg_data 2>/dev/null || { echo "delete old data failed!"; return 4;}
rm -rf $base_dir/depends $base_dir/gvm $base_dir/perl $base_dir/postgresql $base_dir/python 2>/dev/null || { echo "delete old data failed!"; return 4;}
mv -f $tmp_dir/depends $tmp_dir/gvm $tmp_dir/perl $tmp_dir/postgresql $tmp_dir/python "$base_dir/" 2>/dev/null || { echo "move data failed!"; return 4;}
mv -f $tmp_dir/var $tmp_dir/pg_data "$data_root/" 2>/dev/null || { echo "move data failed!"; return 4;}


#add sudo env
sed -i 's/^Defaults[ ][ ]*requiretty/#Defaults requiretty/' /etc/sudoers

return 0
}

function deal_service() {
 (cd "$data_root/var/services" && cp -f vas_functions.sh openvas gvm_service postgres_service redis_service /etc/init.d/) && {
 chmod 444 /etc/init.d/vas_functions.sh
 chmod 755 /etc/init.d/openvas
 chmod 755 /etc/init.d/gvm_service
 chmod 755 /etc/init.d/redis_service 
 chmod 755 /etc/init.d/postgres_service
 } || { echo "install service failed!"; return 5;}


 rm -rf "$data_root/var/services"

return 0
}

function check_env() {
[ "`id -u`" -ne "0" ] && { echo "$0 must be run as root!"; return 1;}
[ ! -e "$base_dir" ] && { echo "not found dir<$base_dir>!"; return 1;}

[ ! -e "$data_root" ] && { echo "not found dir<$data_root>!"; return 1;}

[ -e $tmp_dir ] && rm -rf $tmp_dir
mkdir $tmp_dir || { echo "create dir failed!"; return 1;}

return 0
}

function adduserpg() {
 id -u postgres 1>/dev/null 2>&1 || useradd -M -N -g root postgres 2>/dev/null
}

service openvas stop >/dev/null 2>&1 </dev/null

check_env
ret=$?
[ $ret -ne 0 ] && exit $ret

adduserpg
deal_data
ret=$?
rm -rf "$tmp_dir"
[ $ret -ne 0 ] && exit $ret

deal_service
ret=$?
if [ $ret -eq 0 ]; then
 echo "Success to install all files."
 rm -f setup_vas.tar "$0"
fi

exit $ret 

#############<-end->######################
