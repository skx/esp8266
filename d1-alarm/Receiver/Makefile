
install:
	sudo cp d1-alarm.service /lib/systemd/system/d1-alarm.service
	sudo systemctl enable d1-alarm.service
	sudo mkdir -p /opt/d1-alarm
	sudo cp receiver.pl /opt/d1-alarm/receiver
	sudo mkdir -p /opt/d1-alarm/part.d/ || true
	sudo systemctl start d1-alarm.service
	sudo cp ./alarm.sh /opt/d1-alarm/part.d/sound-alarm
	sudo cp ./alarm.mp3 /opt/d1-alarm/part.d/
