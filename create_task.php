<?php
require 'vuln_parse.php';

$group="0";
$groupname="所有漏洞";
$taskname="";
$hosts="172.16.13.2";

############################
#0-none, 1-once, 2:daily, 3:weekly, 4:monthly
$schedule_type=0; 
$schedule_time="";
$schedule_list="";
$taskname="shedule_test_none";

#create task none
$param="group=\"{$group}\"&groupname=\"{$groupname}\"&taskname=\"{$taskname}\"&hosts=\"{$hosts}\"&schdule_type=\"{$schedule_type}\"&schedule_time=\"{$schedule_time}\"&schedule_list=\"{$schedule_list}\"";
call_c_func("create_task", "$param");

#################################
$schedule_type=1; 
$schedule_time="20220112T130000";
$schedule_list="";
$taskname="shedule_test_once";

#create task once
$param="group=\"{$group}\"&groupname=\"{$groupname}\"&taskname=\"{$taskname}\"&hosts=\"{$hosts}\"&schdule_type=\"{$schedule_type}\"&schedule_time=\"{$schedule_time}\"&schedule_list=\"{$schedule_list}\"";
call_c_func("create_task", "$param");

#############################
$schedule_type=2; 
$schedule_time="20220111T180000";
$schedule_list="";
$taskname="shedule_test_daily";

#create task daily
$param="group=\"{$group}\"&groupname=\"{$groupname}\"&taskname=\"{$taskname}\"&hosts=\"{$hosts}\"&schdule_type=\"{$schedule_type}\"&schedule_time=\"{$schedule_time}\"&schedule_list=\"{$schedule_list}\"";
call_c_func("create_task", "$param");

#############################
$schedule_type=3; 
$schedule_time="20220111T180000";
$schedule_list="MO,FR";
$taskname="shedule_test_weekly";

#create task weekly
$param="group=\"{$group}\"&groupname=\"{$groupname}\"&taskname=\"{$taskname}\"&hosts=\"{$hosts}\"&schdule_type=\"{$schedule_type}\"&schedule_time=\"{$schedule_time}\"&schedule_list=\"{$schedule_list}\"";
call_c_func("create_task", "$param");

##########################
$schedule_type=4; 
$schedule_time="20220111T180000";
$schedule_list="2,16,-1";
$taskname="shedule_test_monthly";

#create task monthly
$param="group=\"{$group}\"&groupname=\"{$groupname}\"&taskname=\"{$taskname}\"&hosts=\"{$hosts}\"&schdule_type=\"{$schedule_type}\"&schedule_time=\"{$schedule_time}\"&schedule_list=\"{$schedule_list}\"";
call_c_func("create_task", "$param");

echo ("====End======\n");

?>
