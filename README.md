This is my digital racecar dashboard, designed to run on a Raspberry Pi connected to a screen in the car.

[![Demo](https://i.ytimg.com/vi/86SsIc0zO8w/hq720.jpg)](https://youtu.be/86SsIc0zO8w)

The main parts of the application are as follows:
- cluster.cpp: Main application responsible for drawing images on the screen. Uses raylib for graphics and communicates via ZMQ with the Python application. 
- read_sensors.py: Handles communication with sensors in the car and data logging. Initially it was responsible for reading the data itself (hence the filename), however it is now a central application which handles other threads which communicate with the devices in the car.

This is a bit of a work in progress, and has lots of kruft due to haste to get it to a working state prior to a race weekend. Cleaning that up is the biggest item on the to do list.
