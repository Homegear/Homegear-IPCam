/* Copyright 2013-2017 Sathya Laufer
 *
 * Homegear is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Homegear is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Homegear.  If not, see <http://www.gnu.org/licenses/>.
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your
 * version.  If you delete this exception statement from all source
 * files in the program, then also delete it here.
 */

#ifndef IPCAMPEER_H_
#define IPCAMPEER_H_

#include <homegear-base/BaseLib.h>

#include <list>

using namespace BaseLib;
using namespace BaseLib::DeviceDescription;

namespace IpCam
{
class IpCamCentral;

class IpCamPeer : public BaseLib::Systems::Peer, public BaseLib::Rpc::IWebserverEventSink
{
public:
	IpCamPeer(uint32_t parentID, IPeerEventSink* eventHandler);
	IpCamPeer(int32_t id, std::string serialNumber, uint32_t parentID, IPeerEventSink* eventHandler);
	virtual ~IpCamPeer();
	void removeHooks();
	void init();
	void dispose();

	//Features
	virtual bool wireless() { return false; }
	//End features

	virtual void worker();
	virtual std::string handleCliCommand(std::string command);

	virtual bool load(BaseLib::Systems::ICentral* central);
    virtual void savePeers() {}

	virtual int32_t getChannelGroupedWith(int32_t channel) { return -1; }
	virtual int32_t getNewFirmwareVersion() { return 0; }
	virtual std::string getFirmwareVersionString(int32_t firmwareVersion) { return "1.0"; }
    virtual bool firmwareUpdateAvailable() { return false; }

    std::string printConfig();

    /**
	 * {@inheritDoc}
	 */
    virtual void homegearStarted();

    /**
	 * {@inheritDoc}
	 */
    virtual void homegearShuttingDown();

    // {{{ Webserver events
		bool onGet(BaseLib::Rpc::PServerInfo& serverInfo, BaseLib::Http& httpRequest, std::shared_ptr<BaseLib::TcpSocket>& socket, std::string& path);
	// }}}

	//RPC methods
	virtual PVariable getDeviceInfo(BaseLib::PRpcClientInfo clientInfo, std::map<std::string, bool> fields);
	virtual PVariable getParamsetDescription(BaseLib::PRpcClientInfo clientInfo, int32_t channel, ParameterGroup::Type::Enum type, uint64_t remoteID, int32_t remoteChannel, bool checkAcls);
	virtual PVariable getParamset(BaseLib::PRpcClientInfo clientInfo, int32_t channel, ParameterGroup::Type::Enum type, uint64_t remoteID, int32_t remoteChannel, bool checkAcls);
	virtual PVariable getValue(BaseLib::PRpcClientInfo clientInfo, uint32_t channel, std::string valueKey, bool requestFromDevice, bool asynchronous);
	virtual PVariable putParamset(BaseLib::PRpcClientInfo clientInfo, int32_t channel, ParameterGroup::Type::Enum type, uint64_t remoteID, int32_t remoteChannel, PVariable variables, bool checkAcls, bool onlyPushing = false);
	virtual PVariable setValue(BaseLib::PRpcClientInfo clientInfo, uint32_t channel, std::string valueKey, PVariable value, bool wait);
	//End RPC methods
protected:
	struct UrlInfo
	{
		std::string ip;
		int32_t port;
		std::string path;
		bool ssl;
	};

	bool _shuttingDown = false;
	std::shared_ptr<BaseLib::Rpc::RpcEncoder> _binaryEncoder;
	std::shared_ptr<BaseLib::Rpc::RpcDecoder> _binaryDecoder;
	std::shared_ptr<BaseLib::HttpClient> _httpClient;
	UrlInfo _streamUrlInfo;
	UrlInfo _snapshotUrlInfo;
	std::string _caFile;
	bool _verifyCertificate = false;
	std::vector<char> _httpOkHeader;

	uint32_t _resetMotionAfter = 30;
	int64_t _motionTime = 0;
	bool _motion = false;

	virtual void loadVariables(BaseLib::Systems::ICentral* central, std::shared_ptr<BaseLib::Database::DataTable>& rows);
    virtual void saveVariables();

	virtual std::shared_ptr<BaseLib::Systems::ICentral> getCentral();

	UrlInfo getUrlInfo(std::string url);
	virtual PParameterGroup getParameterSet(int32_t channel, ParameterGroup::Type::Enum type);
	void initHttpClient();
};

}

#endif
