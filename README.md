# Meds-reminder

This is basically a fancy alarm-clock, with an incremental visual alert. The code that I started with is a clock project by Ruben Marc Speybrouck on Instructables. http://www.instructables.com/id/TESTED-Timekeeping-on-ESP8266-Arduino-Uno-WITHOUT-/
However, I may add RTC functionality in the future — this project is unfinalised after all!        

It runs on an ESP8266 device and synchronises via NTP over the internet.

I have used a Wemos D1 Pro with an Adafruit Neopixel ring mounted inside a 60mm arcade button. I have avoided the use of interrupts as I'm led to believe that there can be issues with some ESP8266 chips.
