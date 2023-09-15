#!/bin/sh

# Install raylib and its dependencies
START_DIR=`pwd`
# the libraries after the # are needed only on desktop platform installs
echo "================Installing Raylib and its dependencies================"
#sudo apt install make git vim cmake libgles2-mesa-dev libgbm-dev libdrm-dev -y # libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev
git clone https://github.com/raysan5/raylib.git
cd raylib
# choose one depending on environment
#cmake -DPLATFORM=Desktop -DOPENGL_VERSION=2.1 -DPLATFORM_CPP=PLATFORM_DESKTOP -DGRAPHICS=GRAPHICS_API_OPENGL_21 .
cmake -DPLATFORM=DRM -DOPENGL_VERSION="ES 2.0" -DPLATFORM_CPP=PLATFORM_DRM -DGRAPHICS=GRAPHICS_API_OPENGL_ES2 .
#make
#make install

echo "================Installing 0MQ===================="
apt install libzmq-dev -y
# The rest may not be necessary?
#cd $START_DIR
#apt install libtool -y
#git clone https://github.com/zeromq/libzmq.git
#cd libzmq
#./autogen.sh
#./configure --with-libsodium && make && make install
#sudo ldconfig

echo "================Installing ZMQPP==================="
cd $START_DIR
git clone https://github.com/zeromq/zmqpp.git
cd zmqpp
make
make check
make install
make installcheck

# This is needed by 0MQ for unclear reasons.
apt install salt-master -y

# Build the demo program
echo "================Building demo program================"
cd $START_DIR
make
EXEC_FILE=rpi-cluster

# Make it run at boot time
echo "================Creating and enabling service================"
SERVICE_FILE=/lib/systemd/system/$EXEC_FILE.service
touch $SERVICE_FILE
echo "[Unit]" > $SERVICE_FILE
echo "Description=Raylib Demo" >> $SERVICE_FILE
echo "After=multi-user.target\n" >> $SERVICE_FILE
echo "[Service]" >> $SERVICE_FILE
echo "ExecStart=$START_DIR/$EXEC_FILE\n" >> $SERVICE_FILE
echo "[Install]" >> $SERVICE_FILE
echo "WantedBy=multi-user.target" >> $SERVICE_FILE
systemctl daemon-reload
systemctl enable $EXEC_FILE.service

# Install PIP
apt install python3-pip -y

# Install and set up GPS libraries
apt install gpsd gpsd-clients -y
systemctl stop gpsd.socket
systemctl disable gpsd.socket
# TODO verify this is the correct tty file
gpsd /dev/ttyUSB0 -F /var/run/gpsd.sock
pip install gps

# Install python serial communication library
pip install pyserial
# Install 0MQ for Python
pip install pyzmq

#apt install libsodium-dev -y
