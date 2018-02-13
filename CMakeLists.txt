cmake_minimum_required(VERSION 3.9)
project(homegear_ipcam)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES
        src/PhysicalInterfaces/EventServer.cpp
        src/PhysicalInterfaces/EventServer.h
        src/PhysicalInterfaces/IIpCamInterface.cpp
        src/PhysicalInterfaces/IIpCamInterface.h
        src/Factory.cpp
        src/Factory.h
        src/GD.cpp
        src/GD.h
        src/Interfaces.cpp
        src/Interfaces.h
        src/IpCam.cpp
        src/IpCam.h
        src/IpCamCentral.cpp
        src/IpCamCentral.h
        src/IpCamPacket.cpp
        src/IpCamPacket.h
        src/IpCamPeer.cpp
        src/IpCamPeer.h)

add_custom_target(homegear COMMAND ../../makeAll.sh SOURCES ${SOURCE_FILES})

add_library(homegear_ipcam ${SOURCE_FILES})