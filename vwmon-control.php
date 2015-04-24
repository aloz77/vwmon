<?php

// Vaillant Monitor Control
// Alexey Ozerov (c) 2014
// GNU GENERAL PUBLIC LICENSE
// Ver. 0.10c


// Database configuration

$db_host	= 'localhost';
$db_name	= 'dbname';
$db_user	= 'user';
$db_pass	= 'password';

// Zugangsdaten username => password

$users = array('user1' => 'mypass', 'user2' => 'mypass');


//********************************************************************
// DO NOT EDIT BELOW THIS LINE
//********************************************************************


if (isset ($users['user1']) && $users['user1']=='mypass' || isset ($users['user2']) && $users['user2']=='mypass')
  die('Bitte konfigurieren Sie Zugangsdaten ein.');

$error = false;

// Auth related stuff (can be removed if using .htaccess rules)

$realm = 'VWMon Steuerung';

if (empty($_SERVER['PHP_AUTH_DIGEST'])) {
    header('HTTP/1.1 401 Unauthorized');
    header('WWW-Authenticate: Digest realm="'.$realm.
           '",qop="auth",nonce="'.uniqid().'",opaque="'.md5($realm).'"');

    die('Bitte geben Sie username/password ein');
}


// analyze the PHP_AUTH_DIGEST variable
if (!($data = http_digest_parse($_SERVER['PHP_AUTH_DIGEST'])) ||
    !isset($users[$data['username']]))
    die('Falsche Zugangsdaten');


// generate the valid response
$A1 = md5($data['username'] . ':' . $realm . ':' . $users[$data['username']]);
$A2 = md5($_SERVER['REQUEST_METHOD'].':'.$data['uri']);
$valid_response = md5($A1.':'.$data['nonce'].':'.$data['nc'].':'.$data['cnonce'].':'.$data['qop'].':'.$A2);

if ($data['response'] != $valid_response)
    die('Falsche Zugangsdaten');

// function to parse the http auth header
function http_digest_parse($txt)
{
    // protect against missing data
    $needed_parts = array('nonce'=>1, 'nc'=>1, 'cnonce'=>1, 'qop'=>1, 'username'=>1, 'uri'=>1, 'response'=>1);
    $data = array();
    $keys = implode('|', array_keys($needed_parts));

    preg_match_all('@(' . $keys . ')=(?:([\'"])([^\2]+?)\2|([^\s,]+))@', $txt, $matches, PREG_SET_ORDER);

    foreach ($matches as $m) {
        $data[$m[1]] = $m[3] ? $m[3] : $m[4];
        unset($needed_parts[$m[1]]);
    }

    return $needed_parts ? false : $data;
}

?>

<!DOCTYPE html>
<html>
  <head>
    <title>VWMon Steuerung</title>
    <meta charset="UTF-8"> 
  </head>
  <style>
    
    body { font-family:"Arial",Georgia,Serif; }
    
    @media only screen and (min-device-width: 1000px) 
    { body
      { width:2000px;
      	font-size: 100%;
      }
    }

    @media only screen and (max-device-width: 800px) 
    { body { font-size: 120%; }
      h1 { font-size: 128%; }
      h2 { font-size: 123%; }
    }


    h1,h2,.error,div.break  { clear:both;  }
    h1,h2  { padding-top:0.5em; }
    
    table.details td, table.details th, div.cell
    { border-width:1px;
      border-style:solid;
      border-color:blue;
      padding:0.25em;
      text-align:right;
    }
    table.details th
    { background-color:#1E90FF;
    }
    tr:nth-child(odd) 
    { background: #eeeeee; 
    }
    div.cell2cell
    { border-color:blue;
      border-style:solid;
      border-width:2px;
      padding:0.05em;
      float:left;
      display:inline-block;
      margin-right:1em;
      margin-bottom:1em;
    }
  	
  </style>

  <body>

<?php	

// Check if mysql extension is available

if (!$error && !function_exists('mysqli_connect'))
{ echo ("<p class=\"error\">There is no mySQLi extension available in your PHP installation. Sorry!</p>\n");
  $error=true;
}

// Connect to database

if (!$error)
{ 
	$mysqli = new mysqli($db_host, $db_user, $db_pass, $db_name);
  if (mysqli_connect_errno())
  { echo ("<p class=\"error\">Database connection failed due to ".mysqli_connect_error()."</p>");
    $error=true;
  }
}

// *********************************************************
// Show actual Heat pump state
// Status 0: AT Off, 3: Heizung, 8: Warmwasser, 7: Solevorlauf, 6: Pause, 12: ???
// *********************************************************

echo ("<h2>Wärmepumpe: Status.</h2>\n");

//$query = "SELECT CONVERT_TZ(`datetime`,'GMT','MET') AS `Letzter Eintrag`, vwmon_wp_stat.description AS `Status`, amu_comp_h_sum AS `Kompressorlauf, Std.`, amu_comp_starts AS `Kompressor Starts`, mv_yield_sum AS `Ertrag, kWh`, mv_heat_press_press AS `Heizanlagendruck, bar`, mv_brine_press_press AS `Soledruck, bar`, mv_EI_current AS `Energieintegral, min°`, mv_at_temp_temp AS `Außentemperatur, °C`, cir2_at_off AS `Außenabschalttemperatur, °C`, mv_boiler_temp_temp AS `Warmwassertemperatur, °C`, ci_cir2_set_temp AS `Vorlaufsolltemperatur, °C`, mv_VF2_temp_temp AS `VF2-Sensor, °C`, cir2_rt_day AS `Raumsolltemperatur, °C`, cir2_rt_night AS `Absenktemperatur, °C` FROM `vwmon_history` INNER JOIN `vwmon_wp_stat` ON `vwmon_history`.amu_wp_stat=`vwmon_wp_stat`.id ORDER BY `datetime` DESC LIMIT 0,1";
$query = "SELECT @last_datetime := MAX(`datetime`) FROM `vwmon_history`;SELECT CONVERT_TZ(`datetime`,'GMT','MET') AS `Letzter Eintrag`, vwmon_wp_stat.description AS `Status`, amu_comp_h_sum AS `Kompressorlauf, Std.`, amu_comp_starts AS `Kompressor Starts`, mv_yield_sum AS `Ertrag, kWh`, mv_heat_press_press AS `Heizanlagendruck, bar`, mv_brine_press_press AS `Soledruck, bar`, mv_EI_current AS `Energieintegral, min°`, mv_at_temp_temp AS `Außentemperatur, °C`, cir2_at_off AS `Außenabschalttemperatur, °C`, mv_boiler_temp_temp AS `Warmwassertemperatur, °C`, ci_cir2_set_temp AS `Vorlaufsolltemperatur, °C`, mv_VF2_temp_temp AS `VF2-Sensor, °C`, cir2_rt_day AS `Raumsolltemperatur, °C`, cir2_rt_night AS `Absenktemperatur, °C` FROM `vwmon_history` INNER JOIN `vwmon_wp_stat` ON `vwmon_history`.amu_wp_stat=`vwmon_wp_stat`.id WHERE `datetime` = @last_datetime";

// Execute multi query and print the 2nd result set
if ($mysqli->multi_query($query) && ($result = $mysqli->use_result()))
{
  $result->close();
	$mysqli->next_result();
	
  if (($result = $mysqli->use_result()) && ($row = $result->fetch_assoc()))
  {	echo ("<div class=\"cell2cell\"><table class=\"details\">\n");
    foreach ($row as $key => $value) echo ("<tr><td>$key</td><td>$value</td></tr>\n");
    echo ("</table></div>\n");
    $result->close();
	}
	else
	{ echo ("<p class=\"error\">No heat pump data found.</p>\n");
	}
}
else
{ echo ("<p class=\"error\">Error using mysqli.</p>\n");
	echo ("<p class=\"error\">MySQL: ".$mysqli->error."</p>\n");
	$error=true;
}

//$query = "SELECT CONVERT_TZ(`datetime`,'GMT','MET') AS `Letzter Kompressorlauf`, vwmon_wp_stat.description AS  `Status`, mv_brine_in_temp AS `Soletemperatur, °C`, mv_high_press_press AS `Hochdruck Kältekreis, bar`, mv_low_press_press AS `Niederdruck Kältekreis, bar`, mv_overheat_temp AS `Überhitzung, K`, mv_undercool_temp AS `Unterkühlung, K`, mv_EI_current AS `Energieintegral, min°`, mv_comp_in_temp_temp AS `Kompressoreingang, °C`, mv_comp_out_temp_temp AS `Kompressorausgang, °C`, mv_tev_in_temp_temp AS `TEV-Eintritt, °C`, ci_cir2_set_temp AS `Vorlaufsolltemperatur, °C`, mv_VF2_temp_temp AS `VF2-Sensor, °C`, mv_T6_temp_temp AS `Vorlauftemperatur, °C`, mv_T5_temp_temp AS `Rücklauftemperatur, °C`  FROM `vwmon_history` INNER JOIN `vwmon_wp_stat` ON `vwmon_history`.amu_wp_stat=`vwmon_wp_stat`.id WHERE mv_comp_io>0 ORDER BY `datetime` DESC LIMIT 0,1";
$query = "SELECT @last_datetime := MAX(`datetime`) FROM `vwmon_history` WHERE mv_comp_io>0; SELECT CONVERT_TZ(`datetime`,'GMT','MET') AS `Letzter Kompressorlauf`, vwmon_wp_stat.description AS  `Status`, mv_brine_in_temp AS `Soletemperatur, °C`, mv_high_press_press AS `Hochdruck Kältekreis, bar`, mv_low_press_press AS `Niederdruck Kältekreis, bar`, mv_overheat_temp AS `Überhitzung, K`, mv_undercool_temp AS `Unterkühlung, K`, mv_EI_current AS `Energieintegral, min°`, mv_comp_in_temp_temp AS `Kompressoreingang, °C`, mv_comp_out_temp_temp AS `Kompressorausgang, °C`, mv_tev_in_temp_temp AS `TEV-Eintritt, °C`, ci_cir2_set_temp AS `Vorlaufsolltemperatur, °C`, mv_VF2_temp_temp AS `VF2-Sensor, °C`, mv_T6_temp_temp AS `Vorlauftemperatur, °C`, mv_T5_temp_temp AS `Rücklauftemperatur, °C`  FROM `vwmon_history` INNER JOIN `vwmon_wp_stat` ON `vwmon_history`.amu_wp_stat=`vwmon_wp_stat`.id WHERE `datetime` = @last_datetime";

// echo ($query);

// Execute multi query and print the 2nd result set
if ($mysqli->multi_query($query) && ($result = $mysqli->use_result()))
{
  $result->close();
	$mysqli->next_result();
	
  if (($result = $mysqli->use_result()) && ($row = $result->fetch_assoc()))
  {	echo ("<div class=\"cell2cell\"><table class=\"details\">\n");
    foreach ($row as $key => $value) echo ("<tr><td>$key</td><td>$value</td></tr>\n");
    echo ("</table></div>\n");
    $result->close();
	}
	else
	{ echo ("<p class=\"error\">No heat pump data found.</p>\n");
	}
}
else
{ echo ("<p class=\"error\">Error using mysqli.</p>\n");
	echo ("<p class=\"error\">MySQL: ".$mysqli->error."</p>\n");
	$error=true;
}


// *********************************************************
// Add command to command table
// *********************************************************

if (!$error && isset($_REQUEST["action"]) && $_REQUEST["action"]=="addcommand" && isset($_REQUEST["command"]) && $_REQUEST["command"]!="")
{
	$query = "INSERT INTO vwmon_commands SET command='".$mysqli->real_escape_string($_REQUEST["command"])."',`datetime_created`=UTC_TIMESTAMP(),status=0";
	$result=$mysqli->query($query);
	if (!$result)
	{ echo ("<p class=\"error\">Error with query.</p>\n");
		echo ("<p class=\"error\">Query: ".trim(nl2br(htmlentities($query)))."</p>\n");
		echo ("<p class=\"error\">MySQL: ".$mysqli->error."</p>\n");
		$error=true;
	}
}
else
{
?>

<script language="JavaScript">
 setTimeout("window.location.reload();",60000);
</script>

<?php

}

// *********************************************************
// Show currently stored commands
// *********************************************************

echo ("<h2>Wärmepumpe: Steuerung.</h2>\n");
?>

<form action="vwmon-control.php" method="post">
 <input type="hidden" name="action" value="addcommand" />
 <p>Kommando: <input type="text" name="command" size="30" maxlength="255" /><input type="submit" value="Hinzufügen"/></p>
</form>

<?php
$query = "SELECT CONVERT_TZ(`datetime_created`,'GMT','MET') AS `datetime_created`,command,status,feedback  FROM `vwmon_commands` WHERE 1 ORDER BY `datetime_created` DESC LIMIT 0,10";

// echo ($query);

if (!$error && !($result=$mysqli->query($query)))
{ echo ("<p class=\"error\">Error with query.</p>\n");
	echo ("<p class=\"error\">Query: ".trim(nl2br(htmlentities($query)))."</p>\n");
	echo ("<p class=\"error\">MySQL: ".$mysqli->error."</p>\n");
	$error=true;
}
else if (!$error && $result->num_rows==0)
{ // echo ("<p class=\"error\">Keine Kommandos gefunden.</p>\n");
}
else if (!$error)
{ echo ("<div class=\"cell2cell\"><table class=\"details\">\n");
  echo ("<tr><th>Datum/Zeit</th><th>Kommando</th><th>Status</th><th>Antwort</th></tr>\n");
  while ($row = $result->fetch_assoc())
  { 
    if ($row['status']==0) $row['status']='Neu...';
    if ($row['status']==1) $row['status']='Abgeschickt...';
    if ($row['status']==2) $row['status']='Fertig';
    	
    echo ("<tr><td>".$row['datetime_created']."</td><td>".htmlentities($row['command'])."</td><td>".$row['status']."</td><td>".htmlentities($row['feedback'])."</td></tr>\n");
  }
  echo ("</table></div>\n");
  $result->close();
}

?>

</body>
</html>

<?php


$mysqli->close();

?>