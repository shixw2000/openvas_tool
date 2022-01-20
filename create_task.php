<?php
require 'vuln_parse.php';

function creat_task($hosts, $schedule_type, $schedule_list) {
	$taskname="test_".$hosts."_{$schedule_type}";
	$group="daba56c8-73ec-11df-a475-002264764cea";
	$groupname="所有漏洞";
	$schedule_time="2022-01-20 18:30:00";

	$ret=creatTask($taskname, $group, $groupname, $hosts, $schedule_type, $schedule_time, $schedule_list);
	return $ret;
}

# daily
creat_task("172.16.3.203", 2, ""); 

#weekly
creat_task("172.16.3.200", 3, "MO,SA");

#monthly
creat_task("172.16.3.1-5", 4, "3,17,-1"); 

echo ("====End======\n");

?>
