# Chicken Coop Assist

Automation/reminder system for a backyard chicken coop.

## System Description

- Automatically open/close the run door (unless user sets it to manual mode)
- Sends out reminder at sunrise to open coop
	- Provides options: just the run (if run door is in manual mode) OR the run + front door
	- Asks for confirmation 15 min after 1st reminder
- Sends out reminder at sunset to close up coop
	- Asks for confirmation 15 min after 1st reminder
	- Sends out more urgent reminder 1 hour after 1st reminder
- Tracks cleaning schedule

## Human Tasks

### Opening (Day)
- Open run door
- Uncap food bin
- Remove bin from nest
- Check food + water levels
- (optional, or at least not every day) Open main door to let them out in yard

### Locking Up (Night)
- Close run door
- Cap food bin
- Put recycling bin in nest box
- (if a yard day) Close main door

## Web Integrations
EXISTING:
- NTP date/time
- Sunrise-sunset
FUTURE:
- Google Calendar API? https://developers.google.com/calendar
- Discord bot?

## Hardware
- ESP32 (SparkFun ESP32 Thing)
- Motor for auto run door
- 2 press buttons (door up / door down)
- button to toggle manual/auto run door

# References
- https://www.instructables.com/id/Irrigation-Using-Google-Calendar/
- https://randomnerdtutorials.com/esp32-date-time-ntp-client-server-arduino/
- https://github.com/sparkfun/SparkFun_TB6612FNG_Arduino_Library