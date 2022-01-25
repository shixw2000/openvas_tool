<?php

function read_nvt_file($filepath) {
	$data = array();
	
	if (is_file($filepath)) {
		$file = fopen($filepath, "r");
		$ctx = fread($file, filesize($filepath));
        fclose($file);
		
		$keywords = preg_split('/\n\s?end\s*\n/', $ctx);
		
		foreach ($keywords as $content) {
			
			if(preg_match('/(\b|^)id\s?=\s?([[:digit:]]{8,})/',$content,$matchs)) {
				$id=$matchs[2];
				
				if (preg_match('/(\b|^)group\s?=\s?([[:digit:]]+)/',$content,$matchs)) {
					$group = $matchs[2];
				} else {
					$group = 0;
				}
				
				if (preg_match('/(\b|^)level\s?=(.*)/',$content,$matchs)) {
					$level = trim($matchs[2]);
				} else {
					$level = 100;
				}
				
				if (preg_match('/(\b|^)name\s?=(.*)/',$content,$matchs)) {
					$name = trim($matchs[2]);
				} else {
					$name = '';
				}
				
				if (preg_match('/(\b|^)comment\s?=(.*)/',$content,$matchs)) {
					$comment = trim($matchs[2]);
				} else {
					$comment = '';
				}
				
				if (preg_match('/(\b|^)affect\s?=(.*)/',$content,$matchs)) {
					$affect = trim($matchs[2]);
				} else {
					$affect = '';
				}
				
				if (preg_match('/(\b|^)CVE\s?=(.*)/',$content,$matchs)) {
					$CVE = trim($matchs[2]);
				} else {
					$CVE = '';
				}
				
				if (preg_match('/(\b|^)CNVD\s?=(.*)/',$content,$matchs)) {
					$CNVD = trim($matchs[2]);
				} else {
					$CNVD = '';
				}
				
				if (preg_match('/(\b|^)CNNVD\s?=(.*)/',$content,$matchs)) {
					$CNNVD = trim($matchs[2]);
				} else {
					$CNNVD = '';
				}
				
				if (preg_match('/(\b|^)sourde_path\s?=(.*)/',$content,$matchs)) {
					$nasl_path = trim($matchs[2]);
				} else {
					$nasl_path = '';
				}
				
				if (preg_match('/(\b|^)reference\s?=(.*)/',$content,$matchs)) {
					$reference = trim($matchs[2]);
				} else {
					$reference = '';
				}
				
				if (preg_match('/(\b|^)Bugtraq\s?=(.*)/',$content,$matchs)) {
					$Bugtraq = trim($matchs[2]);
				} else {
					$Bugtraq = '';
				}
				
				$item=array();
				$item['id'] = $id;
				$item['group'] = $group;
				$item['level'] = $level;
				$item['name'] = $name;
				$item['comment'] = $comment;
				$item['affect'] = $affect;
				$item['CVE'] = $CVE;
				$item['CNVD'] = $CNVD;
				$item['CNNVD'] = $CNNVD;
				$item['nasl_path'] = $nasl_path;
				$item['reference'] = $reference;
				$item['Bugtraq'] = $reference;
				
				if ($level < 3) {
					$item['grade'] = '低';
				} elseif ($level < 6) {
					$item['grade'] = '中';
				} else {
					$item['grade'] = '高';
				}
				
				$reference_number = '';
				
				if ($CVE) {
					$reference_number .= $CVE;
				}
				if ($CNVD) {
					if (!empty($reference_number)) {
						$reference_number .= "," . $CNVD;
					} else {
						$reference_number .= $CNVD;
					}
				}
				if ($CNNVD) {
					if (!empty($reference_number)) {
						$reference_number .= "," . $CNNVD;
					} else {
						$reference_number .= $CNNVD;
					}
				}
				
				if ($Bugtraq) {
					if (!empty($reference_number)) {
						$reference_number .= "," . $Bugtraq;
					} else {
						$reference_number .= $Bugtraq;
					}
				}
				
				$item["reference_number"] = $reference_number;
				
				$data[$id] = $item;
			}
		}
	}
	
	return $data;
}

function print_nvt($data) {
	foreach($data as $key => $value) {
		echo("id={$value['id']}| group={$value['group']}| level={$value['level']}| nasl_path={$value['nasl_path']}|"
			."  group_name={$value['group_name']}| grade={$value['grade']}|"
			."  CVE={$value['CVE']}| CNVD={$value['CNVD']}| CNNVD={$value['CNNVD']}|"
			."\n");
	}
}

function read_group_file($filepath) {
	$data = array();
	
	if (is_file($filepath)) {
		$file = fopen($filepath, "r");
		$ctx = fread($file, filesize($filepath));
        fclose($file);
		
		$arr=json_decode($ctx, true);
		if (is_array($arr)) {
			foreach ($arr as $key => $value) {
				$id = $value['id'];
				
				$data[$id] = $value;
			}
		}
	}
	
	return $data;
}

function filter_nvt($nvt_data, $query_group, $query_type, $query_value) {
	$ret_data = array();
	
	#no filter condition
	if (empty($query_group) && (empty($query_type) || empty($query_value))) {
		return $nvt_data;
	}
	
	foreach($nvt_data as $key => $nvt) {
		$group_id = $nvt['group'];
		
		#filter group id
		if (!empty($query_group) && $query_group != $group_id) {
			continue;
		}
		
		#filter type
		if (!empty($query_type) && !empty($query_value)) {
			
			if (array_key_exists($query_type, $nvt) 
				&& preg_match('/'.$query_value.'/i', $nvt[ $query_type ], $matches)) {
					#ok to match
			} else {
				continue;
			}
			
		} 
		
		$ret_data[$key] = $nvt;
	}
	
	return $ret_data;
}

function get_group_info($group_data) {
	$group_info = array();
	
	foreach ($group_data as $key => $value) {
		$group_info[ $key ] = $value['name'];
	}
	
	#add all id to group
	$group_info[ 0 ] = "所有漏洞";
	return $group_info;
}

function print_nvt_group($data) {
	foreach($data as $key => $value) {
		echo("group_id={$value['id']}| group_name={$value['name']}| group_desc={$value['desc']}|\n");
	}
}

function index_nvt_with_group($nvt_data, $group_data) {
	$cnt = 0;
	$new_nvt_data = array();
	
	foreach($nvt_data as $key => $nvt) {
		
		$group_id = $nvt['group'];
		
		$group = $group_data[$group_id];
		if (!empty($group)) {
			$nvt['group_name'] = $group['name'];
		} else {
			$nvt['group_name'] = "group_{$group_id}";
		}
		
		++$cnt;
		$new_nvt_data[$cnt] = $nvt;
	}
	
	return $new_nvt_data;
}

function parse_nvt_all($nvt_filepath, $group_filepath) {
	$nvt_data = read_nvt_file($nvt_filepath);
	$group_data = read_group_file($group_filepath);
	
	$ret_data = index_nvt_with_group($nvt_data, $group_data);
	return $ret_data;
}

function read_nasl($plugindir, $file) {
	$data = array();
	$oid = '';
	$filepath = $plugindir . "/" . $file;
	
	if (is_file($filepath)) {
		$file = fopen($filepath, "r");
		$ctx = fread($file, filesize($filepath));
        fclose($file);
		
		if (preg_match('/(\b|^)script_oid\(\"(.*)\"\)/',$ctx,$matchs)) {
			$oid = trim($matchs[2]);
		} 
		
		if (!empty($oid)) {
			$data['oid'] = $oid;
		} 
	}
	
	return $data;
}

function add_nvt_with_nasl($nvt_data, $plugindir) {
	$new_nvt_data = array();
	
	if (!empty($nvt_data)) {
		foreach($nvt_data as $key => $nvt) {
			$id = $nvt['id'];
			$nasl_path = $nvt['nasl_path'];
			
			$nasl_data = read_nasl($plugindir, $nasl_path);
			
			#ignore empty oids
			if (!empty($nasl_data)) {
				$oid = $nasl_data['oid'];
				
				$nvt['oid'] = $oid;
				$new_nvt_data[$key] = $nvt;
			}
		}
	}
	
	return $new_nvt_data;
}

function print_nvt_config($group_nvt_data) {
	foreach($group_nvt_data as $group_id => $group_info) {
		echo ("==group_id={$group_id}| group_name={$group_info['name']}| group_desc={$group_info['desc']}|==\n");
		
			$nvts = $group_info['nvts'];
			foreach ($nvts as $id => $value) {
				echo("  id={$value['id']}| name={$value['name']}| oid={$value['oid']}|"
					."  grade={$value['grade']}|"
					."  CVE={$value['CVE']}| CNVD={$value['CNVD']}| CNNVD={$value['CNNVD']}|"
					."  \n");
			}
	}
}

#return a 3d{group_id-'nvt'-nvt_id} array
function divide_nvt_by_group($nvt_data) {
	$group_nvt_data = array();
	
	foreach ($nvt_data as $key => $nvt) {
		$group_id = $nvt['group'];
		$nvt_id = $nvt['id'];
		
		$group_nvt_data[$group_id]['nvts'][$nvt_id] = $nvt;
	}
	
	return $group_nvt_data;
}

function add_group_with_name($group_nvt_data, $group_ctx) {
	$new_group_nvt_data = array();
	
	foreach ($group_nvt_data as $group_id => $value) {
		$item = array();
		$group_info = $group_ctx[ $group_id ];
		
		if (!empty($group_info)) {
			$item['name'] = $group_info['name'];
			$item['desc'] = $group_info['desc'];
		} else {
			$item['name'] = "group_{$group_id}";
			$item['desc'] = "group_{$group_id}";
		}
		
		$item['nvts'] = $value['nvts'];
		
		$new_group_nvt_data[$group_id] = $item;
	}
	
	return $new_group_nvt_data;
}

function parse_nvt_config($nvt_filepath, $group_file, $plugin_dir) {
	$nvt_data = read_nvt_file($nvt_filepath);
	$group_ctx =  read_group_file($group_file);
	
	$new_nvt_data = add_nvt_with_nasl($nvt_data, $plugin_dir);
	
	$group_nvt_data = divide_nvt_by_group($new_nvt_data);
	$new_group_nvt_data = add_group_with_name($group_nvt_data, $group_ctx);
	
	return $new_group_nvt_data;
}

function write_nvt_config($group_nvt_data, $basedir) {
	$dir = $basedir . "/";
	
	foreach($group_nvt_data as $group_id => $group_info) {
		$index=sprintf("%08d-0000-0000-0000-000000000000", $group_id);
		$filepath = $dir . "policy_{$index}.xml";

		$group_name = $group_info['name'];
		$group_desc = $group_info['desc'];
		$nvts = $group_info['nvts'];
		
		$file = fopen($filepath, "w");
		if (!empty($file)) {
			fwrite($file, 
			    "<config id=\"{$index}\">\n"
			  . "  <name>{$group_name}</name>\n"
			  . "  <comment>{$group_desc}</comment>\n"
			  . "  <type>0</type>\n"
			  . "  <usage_type>scan</usage_type>\n"
			  . "  <preferences>\n"
			  . "  </preferences>\n"
			  . "  <nvt_selectors>\n");
			
			foreach ($nvts as $id => $value) {
				fwrite($file, 
					"    <nvt_selector>\n"
				  . "      <include>1</include>\n"
				  . "      <type>2</type>\n"
				  . "      <family_or_nvt>{$value['oid']}</family_or_nvt>\n"
				  . "    </nvt_selector>\n");
			}
			
			fwrite($file, 
				"  </nvt_selectors>\n"
			  . "</config>\n");
			
			fclose($file);
		}
	}
}

function get_gvm_run_status_name($status) {
	if (1 >= $status) {
		return "未启动";
	} else if (2 == $status) {
		return "等待运行";
	} else if (3 == $status) {
		return "正在运行";
	} else if (4 == $status) {
		return "已完成";
	} else if (5 == $status) {
		return "手动停止";
	} else {
		return "终止运行";
	}
}

function chk_run_status($status) {
	if ((2 == $status) or (3 == $status)) {
		return 1;
	} else {
		return 0;
	}
}

function get_gvm_schedule_name($schedule_type) {
	if (0 == $schedule_type) {
		return "关闭定时";
	} else if (1 == $schedule_type) {
		return "一次性";
	} else if (2 == $schedule_type) {
		return "每天";
	} else if (3 == $schedule_type) {
		return "每周";
	} else if (4 == $schedule_type) {
		return "每月";
	} else {
		return "";
	}
}

function read_gvm_task_status_file($dir, $taskname, $uuid) {
	$data = array();
	$filepath = $dir . '/task_' . $taskname;
	
	if (is_file($filepath)) {
		$file = fopen($filepath, "r");
		$ctx = fread($file, filesize($filepath));
        fclose($file);
		
		$keywords = preg_split('/\n==========end==========\n/', $ctx);
		
		foreach ($keywords as $content) {
			
			if(preg_match('/\ntask_id=\"([0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})\"\n/',$content,$matchs)) {
				$task_id=$matchs[1];
				
				if ($task_id == $uuid) {
				
					if(preg_match('/\nstatus=\"([0-9]+)\"\n/',$content,$matchs)) {
						$status=$matchs[1];
					}
					
					if(preg_match('/\nprogress=\"([0-9]+)\"\n/',$content,$matchs)) {
						$progress=$matchs[1];
					}
					
					if(preg_match('/\nreport_id=\"([0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})\"\n/',$content,$matchs)) {
						$report_id=$matchs[1];
					}
					
					if(preg_match('/\nresults=\"(.*)\"\n/',$content,$matchs)) {
						$results=$matchs[1];
					}
					
					$data['status'] = $status;
					$data['progress'] = $progress;
					$data['report_id'] = $report_id;
					$data['result'] = $results;
					
					break;
				}
			}
		}
	} else {
		# if there is no status file
		$data['status'] = '1';
		$data['progress'] = '0';
		$data['report_id'] = '';
		$data['result'] = '';
	}
	
	return $data;
}

function getNvtInfoById($id) {
	$results = array(
	"TCP timestamps"=>"漏洞名称:TCP时间戳漏洞,端口:80/tcp,漏洞等级:中,CVE编号:NOCVE", 
	"Cleartext Transmission of Sensitive Information via HTTP"=>"漏洞名称:HTTP敏感信息明文传输漏洞,端口:80/tcp,漏洞等级:中,CVE编号:NOCVE", 
	"Boa Webserver Terminal Escape Sequence in Logs Command Injection Vulnerability"=>"漏洞名称:命令注入漏洞|端口:80/tcp,漏洞等级:中,CVE编号:CVE-2009-4496"
	);
	
	if (array_key_exists("{$id}", $results)) {
		return $results[ "{$id}" ];
	} else {
		return '';
	}
}

function readNvtResult($dir, $taskname) {
	$res = '';
	$cnt = 0;
	
	$filepath = $dir . '/result_' . $taskname;
	
	if (is_file($filepath)) {
		$file = fopen($filepath, "r");
		$ctx = fread($file, filesize($filepath));
        fclose($file);
		
		$texts = preg_split('/\n/', $ctx, 0, PREG_SPLIT_NO_EMPTY);
		
		foreach ($texts as $line) {
			/* host|name|threat|severity|port|cvss|create_time */
			$nvt = preg_split('/\|/', $line, 0, PREG_SPLIT_NO_EMPTY);
			if (7 == count($nvt)) {
				$nvt_info = implode('|', $nvt);
				
				if (!empty($nvt_info)) {
					++$cnt;
					$res = "{$res}($cnt){$nvt_info}<br/>";
				} 
			}
		}
	}
	
	if (!empty($res)) {
		return $res;
	} else {
		return "空<br/>";
	}
}

function read_gvm_tasks($dir, $taskfile) {
	$data = array();
	
	$filepath = $dir . '/' . $taskfile;
	if (is_file($filepath)) {
		$file = fopen($filepath, "r");
		$ctx = fread($file, filesize($filepath));
        fclose($file);

		$keywords = preg_split('/\n==========end==========\n/', $ctx);
		
		foreach ($keywords as $content) {
			
			if(preg_match('/\ntask_id=\"([0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})\"\n/',$content,$matchs)) {
				$task_id=$matchs[1];
				
				if(preg_match('/\ntask_name=\"(.*)\"\n/',$content,$matchs)) {
					$task_name=$matchs[1];
				}
				
				if(preg_match('/\ngroup_id=\"([0-9]+)\"\n/',$content,$matchs)) {
					$group_id=$matchs[1];
				}
				
				if(preg_match('/\ngroup_name=\"(.*)\"\n/',$content,$matchs)) {
					$group_name=$matchs[1];
				}
				
				
				if(preg_match('/\ntarget_id=\"(.*)\"\n/',$content,$matchs)) {
					$target_id=$matchs[1];
				}
				
				if(preg_match('/\nhosts=\"(.*)\"\n/',$content,$matchs)) {
					$hosts=$matchs[1];
				}
				
				if(preg_match('/\nschedule_type=\"(.*)\"\n/',$content,$matchs)) {
					$schedule_type=$matchs[1];
				}
				
				if(preg_match('/\nschedule_time=\"(.*)\"\n/',$content,$matchs)) {
					$schedule_time=$matchs[1];
				}
				
				if(preg_match('/\nschedule_list=\"(.*)\"\n/',$content,$matchs)) {
					$schedule_list=$matchs[1];
				}
				
				if(preg_match('/\ncreate_time=\"(.*)\"\n/',$content,$matchs)) {
					$create_time=$matchs[1];
				}
				
				$task=array();
				$task['task_id'] = $task_id;
				$task['task_name'] = $task_name;
				$task['group_id'] = $group_id;
				$task['target_id'] = $target_id;
				$task['group_name'] = $group_name;
				$task['hosts'] = $hosts;
				
				$task['schedule_type'] = $schedule_type;
				$task['schedule_time'] = $schedule_time;
				$task['schedule_list'] = $schedule_list;
				
				$task['create_time'] = $create_time;
				
				$task['chedule_type_name'] = get_gvm_schedule_name($schedule_type);
				
				/* get status and fill */
				$statusItem = read_gvm_task_status_file($dir, $task_name, $task_id);
				$task['status'] = $statusItem['status'];
				$task['progress'] = $statusItem['progress'];
				$task['report_id'] = $statusItem['report_id'];
				
				$nvt_res = readNvtResult($dir, $task_name);
				$task['result'] = $nvt_res;
				
				$task['status_name'] = get_gvm_run_status_name( $task['status'] );
				
				$data[$task_name] = $task;
			} 
		}
	}
	
	return $data;
}

function read_gvm_config_file($filepath) {
	$item=array();
	
	if (is_file($filepath)) {
		$file = fopen($filepath, "r");
		$content = fread($file, filesize($filepath));
        fclose($file);
			
		if(preg_match('/<config id=\"(([0-9a-f]{8})-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})\">/',$content,$matchs)) {
			$config_id=$matchs[1];
			$group_part=$matchs[2];
			
			if(preg_match('/0*(.*)/',$group_part,$matchs)) {
				$group_id=$matchs[1];
			}
			
			if(preg_match('/<name>(.*)<\/name>/',$content,$matchs)) {
				$group_name=$matchs[1];
			}
			
			$item['config_id'] = $config_id;
			$item['group_id'] = $group_id;
			$item['group_name'] = $group_name;
		} 
		
	}
	
	return $item;
}

function read_gvm_task_group($dir) {
	$data = array();
	
	#read config dir
	$handle = opendir($dir);
    if ($handle){
        while (($file = readdir($handle)) !== false ){
			
			if(preg_match('/^policy_[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}.xml$/', $file, $matchs)) {
				
                $cur_path = $dir . '/' . $file;
				$item = read_gvm_config_file($cur_path);
				
				if (!empty($item)) {
					$config_id = $item['config_id'];
					$data[ $config_id ] = $item;
				}
            }
        }
		
        closedir($handle);
    }
	
	#default for all of nvts
	$item['config_id'] = 'daba56c8-73ec-11df-a475-002264764cea';
	$item['group_id'] = '0';
	$item['group_name'] = '所有漏洞';
	
	$data[ '0' ] = $item;
	return $data;
}

function print_gvm_task_group($gvm_task_group) {
	foreach($gvm_task_group as $group_id => $group_info) {
		echo ("group_id={$group_id}| group_name={$group_info['group_name']}| config_id={$group_info['config_id']}|\n");	
	}
}

function print_gvm_task_info($gvm_task) {
	
	foreach($gvm_task as $task_name => $task_info) {
		echo ("task_name={$task_info['task_name']}| task_id={$task_info['task_id']}|"
			. " group_id={$task_info['group_id']}| group_name={$task_info['group_name']}|"
			. " hosts={$task_info['hosts']}| create_time={$task_info['create_time']}|"
			
			. " status={$task_info['status_name']}| progress={$task_info['progress']}|"
			. " report_id={$task_info['report_id']}| result={$task_info['result']}|"
			. "\n");	
	}
}

function index_data($data) {
	$cnt = 0;
	$new_data = array();
	
	foreach($data as $key => $val) {
		++$cnt;
		$new_data[$cnt] = $val;
	}
	
	return $new_data;
}

function getResult($retcode) {
	$results = array("0"=>"操作成功", "-1"=>"操作失败", "1"=>"参数无效");
	
	if (array_key_exists("{$retcode}", $results)) {
		return $results[ "{$retcode}" ];
	} else {
		return $results['-1'];
	}
}

function call_c_func($cmd, $param) {
	#echo ("start:call_c_func[{$cmd}]|\n");
	$retcode = php_gvm_cmd_entry($cmd, $param);
	$retmsg=getResult($retcode);
	#echo ("end:call_c_func[{$cmd}]:{$param}:{$retmsg}|\n");
	
	return $retmsg;
}

##########
#this is used to create a task for simple
#schedule_type: 0-none, 1-once, 2:daily, 3:weekly, 4:monthly
#schedule_time: 2022-01-20 12:10:08, if none must be empty.
#schedule_list: if weekly:(MO,TU,WE,TH,FR,SA,SU), if monthly(1,...,20,...,30,31,-1), if other must be empty.
#example:createTask("test_weekly", "daba56c8-73ec-11df-a475-002264764cea", "所有漏洞", "172.16.3.3",
#   "3", "2022-01-20 12:10:08", "WE,SA,SU");
###########
function creatTask($taskname, $group, $groupname, $hosts, $schedule_type, $schedule_time, $schedule_list) {
	$param="taskname=\"{$taskname}\"&group=\"{$group}\"&groupname=\"{$groupname}\""
		. "&hosts=\"{$hosts}\"&schdule_type=\"{$schedule_type}\""
		. "&schedule_time=\"{$schedule_time}\"&schedule_list=\"{$schedule_list}\"";
	
	$ret=call_c_func("create_task", "$param");
	return $ret;
}

#this is used to start a task for simple
function startTask($taskname, $taskid, $targetid) {
	$param="taskname=\"{$taskname}\"&taskid=\"{$taskid}\"&targetid=\"{$targetid}\"";
	
	$ret=call_c_func("start_task", "$param");
	return $ret;
}

#this is used to stop a task for simple
function stopTask($taskname, $taskid, $targetid) {
	$param="taskname=\"{$taskname}\"&taskid=\"{$taskid}\"&targetid=\"{$targetid}\"";
	
	$ret=call_c_func("stop_task", "$param");
	return $ret;
}

#this is used to delete_task a task for simple
function deleteTask($taskname, $taskid, $targetid) {
	$param="taskname=\"{$taskname}\"&taskid=\"{$taskid}\"&targetid=\"{$targetid}\"";
	
	$ret=call_c_func("delete_task", "$param");
	return $ret;
}

#$plugin_dir="/usr/local/openvas/gvm/var/lib/openvas/plugins";
#$dir="/usr/local/openvas/gvm/var/lib/openvas/plugins";
#$config_dir="/tmp";
#$nvt_filepath = $dir . "/" . "vas_pattern_map.pat";
#$group_filepath=$dir . "/" . "group.pat";

#$nvts = parse_nvt_all($nvt_filepath, $group_filepath);
#print_nvt($nvts);

#$group_nvt_data = parse_nvt_config($nvt_filepath, $group_filepath, $plugin_dir);
#print_nvt_config($group_nvt_data);
#write_nvt_config($group_nvt_data, $config_dir);

#$gvm_task_group_dir="/usr/local/openvas/gvm/var/lib/gvm/data-objects/gvmd/21.04/configs";
#$gvm_task_priv_dir="/usr/local/openvas/gvm/var/private";
#$gvm_task_file_name="gvm_task_file";

#$gvm_task_group=read_gvm_task_group($gvm_task_group_dir);
#print_gvm_task_group($gvm_task_group);

#$gvm_task=read_gvm_tasks($gvm_task_priv_dir, $gvm_task_file_name);
#print_gvm_task_info($gvm_task);

#$nvt_res = readNvtResult($gvm_task_priv_dir, 'test_172.16.13.3');
#echo ("{$nvt_res}");

#$cmd="start_task";
#$taskname="shixw_000";
#$taskid="58bb1e0b-882e-42bf-a2f2-c111fb97432c";
#$targetid="601526ec-1e20-411a-956d-7e2f212a95a5";
#$param="taskname=\"{$taskname}\"&taskid=\"{$targetid}\"&targetid=\"{$targetid}\"";
#$retmsg=call_c_func($cmd, $param);
?>
