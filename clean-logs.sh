echo "Cleaning logs..."
DATE=`date +%Y%m%d`
TIME=`date +%H%M%S`
OLD_LOGDIR="/home/pi/old_logs/$DATE/$TIME"
mkdir -p $OLD_LOGDIR
mv ~/log/* $OLD_LOGDIR
mkdir -p /home/pi/log/read_sensors
mkdir -p /home/pi/log/telemetry
mkdir -p /home/pi/log/racebox
