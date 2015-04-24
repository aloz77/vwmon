<?php

// Vaillant Monitor Server
// Alexey Ozerov (c) 2014
// GNU GENERAL PUBLIC LICENSE
// Ver. 0.10c

// Configuration

$db_host	= 'localhost';
$db_name	= 'dbname';
$db_user	= 'user';
$db_pass	= 'password';

$db_table_history  = 'vwmon_history';
$db_table_commands = 'vwmon_commands';

$serverkey	= 'somekey';			// Set a fairly long random server key to avoid misuse
$admin_email	= 'mein@email.de';		// This will be the sender address for all emails sent

//********************************************************************
// DO NOT EDIT BELOW THIS LINE
//********************************************************************

if ($serverkey == 'somekey')
  die('Please change the serverkey');

// Swith to test table on test installation

if (strstr($_SERVER['PHP_SELF'],'test-vwmon-server.php'))
	$db_table_history = 'test_vwmon_history';

// Check server key

if (!isset($_GET["serverkey"]) || $_GET["serverkey"]!=$serverkey || $_GET["serverkey"]=="")
	error('VWMon ERROR: wrong or empty server key');
else
	unset($_GET["serverkey"]);

// Open mySQL

$mysqli = new mysqli($db_host, $db_user, $db_pass, $db_name);
if (mysqli_connect_error())
  error ("Database connection failed due to ".mysqli_connect_error());

// *********************************************************
// Action addrecord
// *********************************************************

if (isset($_GET["action"]) && $_GET["action"]=="addrecord")
{

// Check datetime

	if (!isset($_GET["datetime"]))
		error('VWMon ERROR: Empty datetime parameter');

	$recordtime=strtotime($_GET["datetime"]);

	if (!$recordtime || $recordtime==-1)
		error('Frewe ERROR: Malformed datetime parameter, use YYYY-MM-DD HH:MM:SS');
	else
	{

// Build query

		$query = "INSERT INTO $db_table_history SET ";

		foreach ($_GET as $field => $value)
		{	if ($field!="action")
			{
				if (isset($value) && $value!="NULL")
					$query .= sprintf("`%s` = '%s', ", $mysqli->real_escape_string($field), $mysqli->real_escape_string($value));
				else
					$query .= sprintf("`%s` = %s, ", $mysqli->real_escape_string($field), "NULL");
			}
		}

		$query = substr($query,0,-2);

		$result = $mysqli->query($query);
		if (!$result)
			error('VWMon ERROR: Dabase query fault: ' . $mysqli->error. ' query: '. $query);

		echo ("OK");
	}
}

// *********************************************************
// Action getcommand
// *********************************************************

else if (isset($_GET["action"]) && $_GET["action"]=="getcommand")
{

	$query = "SELECT id,command FROM $db_table_commands WHERE status=0 ORDER BY `datetime_created` ASC LIMIT 1";
	
	$result = $mysqli->query($query);
	if (!$result)
		error('Frewe ERROR dabase query fault: ' . $mysqli->error. ' query: '. $query);
	$lastrow = $result->fetch_assoc();

	if ($lastrow) 
	{	echo ($lastrow['id'].":".$lastrow['command']);
		$query = "UPDATE $db_table_commands SET status=1 WHERE id='".$lastrow['id']."'";
		$result = $mysqli->query($query);
		if (!$result)
			error('Frewe ERROR dabase query fault: ' . $mysqli->error. ' query: '. $query);
	}
	else
		echo ("Not found");
}

// *********************************************************
// Action setfeedback
// *********************************************************

else if (isset($_GET["action"]) && $_GET["action"]=="setfeedback" && isset($_GET["id"]) && isset($_GET["feedback"]))
{
	$query = "UPDATE $db_table_commands SET status=2, feedback = '".$mysqli->real_escape_string($_GET["feedback"])."' WHERE id='".$mysqli->real_escape_string($_GET["id"])."'";
	$result = $mysqli->query($query);
	if (!$result)
		error('Frewe ERROR dabase query fault: ' . $mysqli->error. ' query: '. $query);
	echo ("OK");
}


// *********************************************************
// Action submiterror
// *********************************************************

else if (isset($_GET["action"]) && $_GET["action"]=="submiterror" && $admin_email!='')
{
	if (!isset($_GET["email"]) || $_GET["email"]=="")
		$receiver_email=$admin_email;	// Compatibility
	else
		$receiver_email=$_GET["email"];
	
	$header = "From: $admin_email";
	mail($receiver_email, "vwmon-client error", isset($_GET["errortext"])?$_GET["errortext"]:"???", $header);
	echo ("OK");
}

// *********************************************************
// No action found
// *********************************************************

else
	error ('VWMon ERROR: Action not set');

// *********************************************************
// Log error and exit
// *********************************************************

function error($message)
{
	global $link,$admin_email;
	if ($link) mysql_close($link);

	error_log ($message);
	echo ($message);

	$header = "From: $admin_email";
	mail($admin_email, "vwmon-client error", $message, $header);
	
	exit(1);
}

?>
