function MijiaService()
{
	this.url = "/js/index.php";

	/**
	* @param string $soundfile .
	* @return boolean .
	*/

	this.PlaySound = function(cb_success, cb_failure, soundfile)
	{
		$.ajax({
			type: 'POST',
			url: this.url,
			data: {'method': 'PlaySound', 'soundfile': soundfile},
			dataType: 'json',
			success: cb_success,
			error: cb_failure
		});
	}

	/**
	*/
}
