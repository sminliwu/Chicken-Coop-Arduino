// web portal functionality

$(document).ready(function() {
	// door status can be: open, closed, opening, closing, ERROR

	// update HTML page
	function setAutoMode(str) {
		// STR is either "AUTO" or "MANUAL"
		$("#door-mode").html('<h1>Mode: '+ str + '</h1>')
		if (str == 'AUTO') {
			$('#manual').html('Switch to MANUAL Mode');
			$('#door-controls').hide();
		} else if (str == 'MANUAL') {
			$('#manual').html('Switch to AUTO Mode');
			$('#door-controls').show();
		}
	}

	function setDoorStatus(str) {
		$("#door-status").html('<h1>Door status: ' + str + '</h1>');
		if (str == "closed") {
			$('#open').attr('disabled', false);
			$('#close').attr('disabled', true);
		} else if (str == "open") {
			$('#open').attr('disabled', true);
			$('#close').attr('disabled', false);
		} else {
			// door is moving
			$('#open').attr('disabled', true);
			$('#close').attr('disabled', true);		}
	}

	function setTime(str) {
		$("#time").html('<h1>Time: '+str+'</h1>');
	}

	function setDate(str) {
		$("#date").html('<h1>Date: '+str+'</h1>')
	}

	function setSunTimes(str) {
		$("#sunrise-sunset").html('<h1>Sunrise/Sunset: '+str+'</h1>');
	}

	setDoorStatus();

	$('#open').click(function(evt) {
		console.log("user input opening door");
		$.get('/open'); // send server request, ESP will start opening
		setDoorStatus("opening");
	});

	$('#close').click(function(evt) {
		console.log("user input closing door")
		$.get('/close');
		setDoorStatus("closing");
	});

	$('#manual').click(function(evt) {
		console.log("user toggling manual/auto");
		$.get('/manual');
	})

	var connection = new WebSocket('ws://'+ location.hostname +':81/', ['arduino']);
	connection.onopen = function () {
    	connection.send('Connect ' + new Date());
  	};
  	connection.onerror = function (error) {
    	console.log('WebSocket Error ', error);
  	};
  	connection.onmessage = function (e) {
  		var payload = String(e.data);
    	console.log(payload);
    	if (payload.startsWith("control")) {
    		var x = payload.indexOf(" ");
    		var mode = payload.substring(x+1);
    		setAutoMode(mode);
    	} else if (payload.startsWith("open") || payload.startsWith("clos")) {
			setDoorStatus(payload);
		} else {
			var x = payload.indexOf(" ");
			var data = payload.substring(x+1);
			console.log(data);
			if (payload.startsWith("sun")) {
				//console.log("data contains sun times");
				setSunTimes(data);
			} else if (payload.startsWith("time")) {
				setTime(data);
			} else if (payload.startsWith("date")) {
				setDate(data);
			}
		}
  	};
  	connection.onclose = function () {
   		console.log('WebSocket connection closed');
  	};

});