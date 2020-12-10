# Chicken Coop Management

Automation/reminder system for a backyard chicken coop.

## Network Actors
- ESP32
- Google Services
- Humans
- Flock

## System Tasks

### ESP32
- Automatically open/close the run door (unless human sets it to manual mode)
- Track door status (with human input in manual)
- Send POST requests to Google Cloud Services (if enabled)

### Google Services
- Sends out reminder at sunrise to open coop
	- Provides options: just the run (if run door is in manual mode) OR the run + front door
	- Asks for confirmation 15 min after 1st reminder
- Sends out reminder at sunset to close up coop
	- Asks for confirmation 15 min after 1st reminder
	- Sends out more urgent reminder 1 hour after 1st reminder
- Listen for updates from ESP32
- Tracks cleaning schedule? (future)

### Humans

#### Opening (Day)
- Open run door
- Uncap food bin by UNSCREWING lids
- Check food + water levels, replenish if needed
- Check for eggs
- (optional, or at least not every day) Open main door to let them out in yard

#### Locking Up (Night)
- Close run door
- Cap food bin and pour tray food back into the bin
- Close the run access gate
- (if a yard day) Close main door

#### Maintenance (schedule TBD)
- Clean poop from roosts/floor
- Clean run
- Replace water in pool
- Replace bedding
- Winterproof with cardboard

### Flock
- Lay eggs
- Eat food and drink water

## Web Integrations
EXISTING:
- NTP date/time
- Sunrise-sunset
IN PROGRESS:
- Google App Script running as web app: Google Calendar + Sheets
POSSIBLE:
- Discord bot?

## Hardware
- ESP32 (SparkFun ESP32 Thing)
- Motor for auto run door
- 2 press buttons (door up / door down)
- button to toggle manual/auto run door

## ESP Status Codes
Category 		Assc. Variables 		Broadcast Change?
(code)									WS 	Google
-------------------------------------------------------------------
door (d) 		doorStatus 				y 	d 		y = yes always
				motorOn					 	 		d = debug only
				motorDir 				 			x = never
control (c) 	autoMode 				y 	y
motor time (m)	motorInterval_open		y 	y 
				motorInterval_close
auto-offset (e)	offset_open				y 	y
				offset_close
flock (f) 		flockStatus 			y 	y
google (g) 		googleEnabled			y 	y
state (s) 		state 					d 	d
time (t) 		time 					y 	x
day-night (n)	sunrise/sunset 			y 	x
				date 				 	

## ESP32-Client Communications

(Messages sent over WebSocket)
Client sends		ESP	sends				Context
-------------------------------------------------------------------
open (o)									Client clicked 
											"open" button
					opening [##]
close (c)									Client clicked 
											"close" button
					closing [##]
stop (h)
set-open (so)
					open
set-close (sc)
					closed
manual (a)
					control [AUTO/MANUAL]
google (g)
					google [ON/OFF]
m[o/c] \[##\] (m)							Client adjusted motor 
											interval time
					m[o/c] \[##\]			
e[o/c] \[##\]								Client adjusted offset 
											auto open/close time
					e[o/c] \[##\]
					time: [HH:MM AM/PM]		ESP updates client with 
											time data
					date: [MM/DD/YYYY]
					sunrise/sunset: 
					[time/time]

## ESP32-Google Communications

(ESP sends HTTP POST requests to a Google web app)
ESP sends			Google response
------------------------------------------------------------------
c[a/m] 				control mode: switches calendar events 
					to auto/manual
f[c/r/y] ### 		logs the change in flock status with IP address

# References
- https://www.instructables.com/id/Irrigation-Using-Google-Calendar/
- https://randomnerdtutorials.com/esp32-date-time-ntp-client-server-arduino/
- https://randomnerdtutorials.com/esp32-http-get-post-arduino/
- Library I'm using to control my motor - https://github.com/sparkfun/SparkFun_TB6612FNG_Arduino_Library
- Tutorial for using Google script - https://www.dfrobot.com/blog-917.html
- https://developers.google.com/apps-script
- https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date
- https://medium.com/@dtkatz/3-ways-to-fix-the-cors-error-and-how-access-control-allow-origin-works-d97d55946d9