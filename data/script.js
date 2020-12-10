$(document).ready(function () {
	function setAutoMode(str) {
		$("#d-mode").html('Mode: '+ str);
		if (str == 'AUTO') {
			$('#mode').html('Switch to MANUAL Mode');
			$('#c-manual').hide();
			$('#c-auto').show();
		} else if (str == 'MANUAL') {
			$('#mode').html('Switch to AUTO Mode');
			$('#c-manual').show();
			$('#c-auto').hide();
		}
	}
	function setGoogleMode(str) {
		if (str == 'ON') {
			$('#g').html('Sending to Google. Turn OFF?');
		} else if (str == 'OFF') {
			$('#g').html('Not sending to Google. Turn ON?');
		}
	}
	var ws = new WebSocket('ws://'+ location.hostname +':81/', ['arduino']);
	ws.onopen = function () {
		ws.send('Connect ' + new Date());
	};
	ws.onerror = function (error) {
		console.log('WebSocket Error ', error);
	};
	ws.onmessage = function (e) {
		var payload = String(e.data);
		console.log(payload);
		var x = payload.indexOf(" ");
		var data = payload.substring(x+1);
		if (payload.startsWith("open") || payload.startsWith("clos")) {
			$("#d-status").text('Run door: '+payload);
		} else if (payload.startsWith("flock")) {
			$('#f').val(data);
		} else if (payload.startsWith("e")) {
			if (payload.charAt(1) == "o") {
				$("#eo").val(parseInt(data));
			} else {$("#ec").val(parseInt(data));}
		} else if (payload.startsWith("m")) {
			if (payload.charAt(1) == "o") {
				$("#mo").val(parseInt(data));
			} else {$("#mc").val(parseInt(data));}
		} else if (payload.startsWith("control")) {
			setAutoMode(data);
		} else if (payload.startsWith("google")) {
			setGoogleMode(data);
		} else if (payload.startsWith("time")) {
			$("#time").text('Time: '+data);
		} else if (payload.startsWith("date")) {
			$("#date").text('Date: '+data)
		} else if (payload.startsWith("sun")) {
			$("#sun").text('Sunrise/Sunset: '+data);
		} else {
			ws.send('Unknown server message: ' + payload);
		}
	};
	ws.onclose = function (e) {
		console.log('WebSocket connection closed', e);
	};
	$('#f').change(function(evt) {
		// console.log('f'+evt.target.value);
		ws.send('f' + evt.target.value);
	})
	$('#o').click(function() {
		if ($('#o').html() == "Open") {
			ws.send('o');
			$('#o').html("Stop");
			$('#c').prop("disabled",true);
		} else {
			ws.send('h');
			$('#o').html("Open");
			$('#c').prop("disabled",false);
		}
	});
	$('#c').click(function() {
		if ($('#c').html() == "Close") {
			ws.send('c');
			$('#c').html("Stop");
			$('#o').prop("disabled",true);
		} else {
			ws.send('h');
			$('#c').html("Close");
			$('#o').prop("disabled",false);
		}
	});
	$('#mo').change(function() {
		ws.send('mo' + $('#mo').val());
	})
	$('#mc').change(function() {
		ws.send('mc' + $('#mc').val());
	})
	$('#eo').change(function() {
		ws.send('eo' + $('#eo').val());
	})
	$('#ec').change(function() {
		ws.send('ec' + $('#ec').val());
	})
	$('#so').click(function() {
		ws.send('so');
	})
	$('#sc').click(function() {
		ws.send('sc');
	})
	$('#mode').click(function() {
		ws.send('a');
	});
	$('#g').click(function() {
		ws.send('g');
	});
});