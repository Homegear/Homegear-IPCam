AUTOMAKE_OPTIONS = subdir-objects

AM_CPPFLAGS = -Wall -std=c++11 -DFORTIFY_SOURCE=2 -DGCRYPT_NO_DEPRECATED
AM_LDFLAGS = -Wl,-rpath=/lib/homegear -Wl,-rpath=/usr/lib/homegear -Wl,-rpath=/usr/local/lib/homegear

libdir = $(localstatedir)/lib/homegear/modules
lib_LTLIBRARIES = mod_ipcam.la
mod_ipcam_la_SOURCES = IpCam.cpp IpCam.h IpCamPacket.cpp IpCamPacket.h IpCamPeer.cpp IpCamPeer.h Factory.cpp Factory.h GD.cpp GD.h IpCamCentral.cpp IpCamCentral.h PhysicalInterfaces/EventServer.cpp PhysicalInterfaces/EventServer.h PhysicalInterfaces/IIpCamInterface.cpp PhysicalInterfaces/IIpCamInterface.h Interfaces.h Interfaces.cpp
mod_ipcam_la_LDFLAGS =-module -avoid-version -shared
install-exec-hook:
	rm -f $(DESTDIR)$(libdir)/mod_ipcam.la
