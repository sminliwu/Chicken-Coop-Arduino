/* File: doorControl.ino
 * Short desc: Handles auto/manual controls for the door motor.
 * IN AUTO MODE:
 *	- runs sunrise/sunset state machines
 *	- when opening/closing door, runs motor state machine 
 *	  with fixed motor interval
 * IN MANUAL MODE:
 *	- lets user run motor up/down manually
 *		> web client buttons
 *		> physical press buttons
 *	- lets user manually set open/close status if hook was used
 *	  (WEB CLIENT ONLY)
 */

// start running motor to open door
bool openDoor() {
  motorStartMillis = millis(); // log when the motor started
  motorDir = true;
  motor.drive(OPEN_DR); // eventually change this to a sensor stopping condition
  return true;
}

// start closing door
bool closeDoor() {
  motorStartMillis = millis(); // log when the motor started
  motorDir = false;
  motor.drive(CLOSE_DR); // also change this to a sensor stop condition
  return true;
}

bool stopDoor() {
	motor.brake();
	if (!autoMode) {
		webSocket.broadcastTXT("motor time " + String(motorTime));
	}
	motorTime = 0; // reset motor time counter
	return false;
}

// when done running the motor, update door status
bool updateDoorStatus() {
  if (motorOn) {
    String timeStr = String(motorTime);
    if (motorDir) {
      webSocket.broadcastTXT("opening " + timeStr);
    } else {
      webSocket.broadcastTXT("closing " + timeStr);
    }
    return false;
  } else {
    if (motorDir) { // if door was opening, is now open
      doorStatus = true;
      webSocket.broadcastTXT("open");
    } else {
      doorStatus = false;
      webSocket.broadcastTXT("closed");
    }
    return true;
  }
}