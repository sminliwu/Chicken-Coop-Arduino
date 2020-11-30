// web portal functionality

var debug = true;
var autoMode = true;

$(document).ready(function() {
	// proxy credit: https://medium.com/@dtkatz/3-ways-to-fix-the-cors-error-and-how-access-control-allow-origin-works-d97d55946d9
	// const googleUrl = "https://cors-anywhere.herokuapp.com/https://script.google.com/macros/s/AKfycby1Ib2GtFfhYq1k_1Y0q2O3VoZLeC6gqPpVQpyfheJZM1T46iU/exec";
	// const dataString = "some message";
	// $.post(url, postData, function(data, status) {
	//  console.log(`${data} result and status is ${status}`);
	// }, "text");
	// $.ajax({
	//  type: "POST",
	//  dataType: "text",
	//  url: googleUrl,
	//  data: postData,
	//  success: function(response) {
	//    console.log(`${response} received`);
	//  }
	// })
	// var postReq = new XMLHttpRequest();
	// postReq.open("POST", googleUrl);
	// postReq.setRequestHeader("Content-Type", "text/plain");
	// postReq.send(dataString);

	function setAutoMode(str) {
		$("#door-mode").html('<h1>Mode: '+ str + '</h1>');
		if (str == 'AUTO') {
			$('#manual').html('Switch to MANUAL Mode');
			$('#door-controls').hide();
		} else if (str == 'MANUAL') {
			$('#manual').html('Switch to AUTO Mode');
			$('#door-controls').show();
		}
	}
	function setGoogleMode(str) {
		if (str == 'ON') {
			$('#google').html('Sending to Google. Turn OFF?');
		} else if (str == 'OFF') {
			$('#google').html('Not sending to Google. Turn ON?');
		}
	}
	function setDoorStatus(str) {
		$("#door-status").text('Run door: '+str);
	}
	function setTime(str) {
		$("#time").text('Time: '+str);
	}
	function setDate(str) {
		$("#date").text('Date: '+str);
	}
	function setSunTimes(str) {
		$("#sunrise-sunset").text('Sunrise/Sunset: '+str);
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
		if (payload.startsWith("control")) {
			var x = payload.indexOf(" ");
			var mode = payload.substring(x+1);
			setAutoMode(mode);
		} else if (payload.startsWith("google")) {
			var x = payload.indexOf(" ");
			var mode = payload.substring(x+1);
			setGoogleMode(mode);
		} else if (payload.startsWith("open") || payload.startsWith("clos")) {
			setDoorStatus(payload);
		} else {
			var x = payload.indexOf(" ");
			var data = payload.substring(x+1);
			if (payload.startsWith("sun")) {
				setSunTimes(data);
			} else if (payload.startsWith("time")) {
				setTime(data);
			} else if (payload.startsWith("date")) {
				setDate(data);
			}
		}
	};
	ws.onclose = function () {console.log('WebSocket connection closed');
	};
	$('#open').mousedown(function(evt) {
		ws.send('open');
		setDoorStatus("opening");
	});
	$('#open').mouseup(function(evt) {
		ws.send('stop');
	})
	$('#close').mousedown(function(evt) {
		ws.send('close');
		setDoorStatus("closing");
	});
	$('#close').mouseup(function(evt) {
		ws.send('stop');
	});
	$('#set-open').click(function(evt) {
		ws.send('set-open');
	})
	$('#set-close').click(function(evt) {
		ws.send('set-close');
	})

	$('#manual').click(function(evt) {
		ws.send('manual');
	});
	$('#google').click(function(evt) {
		ws.send('google');
	});
});