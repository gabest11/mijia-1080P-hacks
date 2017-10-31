<?php ?>
<html>
<head>
	<title>Mijia</title>
	<script src="/js/jquery.js" type="text/javascript"></script>
	<script src="/js/svc.js" type="text/javascript"></script>
	<script type="text/javascript">

	function cb_success(xhr, ajaxOptions, thrownError) {$('#result').text('OK');}
	function cb_failure(xhr, ajaxOptions, thrownError) {$('#result').text(xhr.responseText);}

	function PlaySound(s) 
	{
		var svc = new MijiaService(); 

		$('#result').text('');

		svc.PlaySound(
			cb_success, 
			cb_failure, 
			s);
	}

	</script>
</head>
<body>
	<a href="/media">media</a>
	<a href="phpinfo.php">phpinfo</a>
	<a href="javascript:void(0);" onclick="PlaySound('ding'); return false;">ding</a>
	<a href="snapshot.php?rate=3000">snapshot</a>
	<div id="result"></div>
</body>
</html>