<?php
require 'vuln_parse.php';

$group="0";
$groupname="所有漏洞";
$taskname="";
$hosts="172.16.16.1-172.16.16.233";

############################
#0-none, 1-once, 2:daily, 3:weekly, 4:monthly
$schedule_type=0; 
$schedule_time="";
$schedule_list="";
$taskname="172.16.16.0_none";

#create task none
$param="group=\"{$group}\"&groupname=\"{$groupname}\"&taskname=\"{$taskname}\"&hosts=\"{$hosts}\"&schdule_type=\"{$schedule_type}\"&schedule_time=\"{$schedule_time}\"&schedule_list=\"{$schedule_list}\"";
call_c_func("create_task", "$param");

#################################
$schedule_type=1; 
$schedule_time="2022-01-13 18:00:00";
$schedule_list="";
$taskname="172.16.16.0_once";

#create task once
$param="group=\"{$group}\"&groupname=\"{$groupname}\"&taskname=\"{$taskname}\"&hosts=\"{$hosts}\"&schdule_type=\"{$schedule_type}\"&schedule_time=\"{$schedule_time}\"&schedule_list=\"{$schedule_list}\"";
call_c_func("create_task", "$param");

#############################
$schedule_type=2; 
$schedule_time="2022-01-13 18:00:00";
$schedule_list="";
$taskname="172.16.16.0_daily";

#create task daily
$param="group=\"{$group}\"&groupname=\"{$groupname}\"&taskname=\"{$taskname}\"&hosts=\"{$hosts}\"&schdule_type=\"{$schedule_type}\"&schedule_time=\"{$schedule_time}\"&schedule_list=\"{$schedule_list}\"";
call_c_func("create_task", "$param");

#############################
$schedule_type=3; 
$schedule_time="2022-01-13 18:00:00";
$schedule_list="MO,FR";
$taskname="172.16.16.0_weekly";

#create task weekly
$param="group=\"{$group}\"&groupname=\"{$groupname}\"&taskname=\"{$taskname}\"&hosts=\"{$hosts}\"&schdule_type=\"{$schedule_type}\"&schedule_time=\"{$schedule_time}\"&schedule_list=\"{$schedule_list}\"";
call_c_func("create_task", "$param");

##########################
$schedule_type=4; 
$schedule_time="2022-01-13 18:00:00";
$schedule_list="2,16,-1";
$taskname="172.16.16.0_monthly";

#create task monthly
$param="group=\"{$group}\"&groupname=\"{$groupname}\"&taskname=\"{$taskname}\"&hosts=\"{$hosts}\"&schdule_type=\"{$schedule_type}\"&schedule_time=\"{$schedule_time}\"&schedule_list=\"{$schedule_list}\"";
call_c_func("create_task", "$param");

echo ("====End======\n");

?>
