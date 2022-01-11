<?php
require 'vuln_parse.php';

$group="0";
$groupname="所有漏洞";
$taskname="2_测试_weekly";
$hosts="172.16.13.11,172.16.13.82";

#0-once, 1:daily, 2:weekly, 3:monthly
$schedule_type=2; 
$schedule_time="20220111T180000";
$schedule_list="MO,WE,FR";

#create task
$param="group=\"{$group}\"&groupname=\"{$groupname}\"&taskname=\"{$taskname}\"&hosts=\"{$hosts}\"&schdule_type=\"{$schedule_type}\"&schedule_time=\"{$schedule_time}\"&schedule_list=\"{$schedule_list}\"";

call_c_func("create_task", "$param");

echo ("==========\n");

?>
