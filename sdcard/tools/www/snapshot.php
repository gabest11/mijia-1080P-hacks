<?php

if(isset($_GET['rand']))
{
	header('Content-Type: image/jpeg');

	//$ret = 0;

	//passthru('/tmp/sd/tools/bin/snapshot 75', $ret);

	$ret = exec('/tmp/sd/tools/bin/snapshot > /tmp/sd/tools/tmp/snapshot.jpg');

	readfile('/tmp/sd/tools/tmp/snapshot.jpg', $ret);

	exit;
}

?>

<html>
<head>
	<title>Mijia</title>
	<script src="/js/jquery.js" type="text/javascript"></script>
	<script src="/js/svc.js" type="text/javascript"></script>
	<script type="text/javascript">

	function UpdateCamera()	
	{
		$('#camera img').attr('src', function(i, old) { return old.replace(/\?.+/,"?rand=" + Date.now()); });
	}

	function Load()
	{
		// don't go lower than a 1000ms, the camera will hang

		setInterval(UpdateCamera, <?php echo max(isset($_GET['rate']) ? (int)$_GET['rate'] : 3000, 1000) ?>);
	}

	</script>
</head>
<body onLoad="Load()">
	This will break the mi home app until the next reboot or '/etc/init.d/S60miio_avstreamer restart'
	<div id="camera">
		<img src="snapshot.php?rand=<?php echo time(); ?>" />
	</div>
</body>
</html>