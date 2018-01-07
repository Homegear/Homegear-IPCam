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

#include "IpCamPeer.h"

#include "GD.h"
#include "IpCamPacket.h"
#include "IpCamCentral.h"

namespace IpCam
{
std::shared_ptr<BaseLib::Systems::ICentral> IpCamPeer::getCentral()
{
	try
	{
		if(_central) return _central;
		_central = GD::family->getCentral();
		return _central;
	}
	catch(const std::exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(BaseLib::Exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(...)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
	}
	return std::shared_ptr<BaseLib::Systems::ICentral>();
}

IpCamPeer::IpCamPeer(uint32_t parentID, IPeerEventSink* eventHandler) : BaseLib::Systems::Peer(GD::bl, parentID, eventHandler)
{
	init();
}

IpCamPeer::IpCamPeer(int32_t id, std::string serialNumber, uint32_t parentID, IPeerEventSink* eventHandler) : BaseLib::Systems::Peer(GD::bl, id, -1, serialNumber, parentID, eventHandler)
{
	init();
}

IpCamPeer::~IpCamPeer()
{
	try
	{
		dispose();
	}
	catch(const std::exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(BaseLib::Exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(...)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
	}
}

void IpCamPeer::worker()
{
	try
	{
		if(_disposing) return;
		if(_motion && _motionTime + _resetMotionAfter <= BaseLib::HelperFunctions::getTime())
		{
			BaseLib::Systems::RpcConfigurationParameter& parameter = valuesCentral[1]["MOTION"];
			if(parameter.rpcParameter)
			{
				_motion = false;
				std::shared_ptr<std::vector<std::string>> valueKeys(new std::vector<std::string>{ "MOTION" });
				std::shared_ptr<std::vector<PVariable>> values(new std::vector<PVariable> { PVariable(new Variable(false)) });
				std::vector<uint8_t> parameterData{ 0 };
				parameter.setBinaryData(parameterData);
				if(parameter.databaseId > 0) saveParameter(parameter.databaseId, parameterData);
				else saveParameter(0, ParameterGroup::Type::Enum::variables, 1, "MOTION", parameterData);
				if(_bl->debugLevel >= 4) GD::out.printInfo("Info: MOTION of peer " + std::to_string(_peerID) + " with serial number " + _serialNumber + ":1 was set to false.");
				std::string address(_serialNumber + ":1");
				raiseEvent(_peerID, 1, valueKeys, values);
				raiseRPCEvent(_peerID, 1, address, valueKeys, values);
			}
		}
	}
	catch(const std::exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(BaseLib::Exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(...)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
	}
}

void IpCamPeer::init()
{
	_binaryEncoder.reset(new BaseLib::Rpc::RpcEncoder(_bl));
	_binaryDecoder.reset(new BaseLib::Rpc::RpcDecoder(_bl));
	_httpClient.reset(new BaseLib::HttpClient(_bl, "ipcam", 65635, false));
	raiseAddWebserverEventHandler(this);
	std::string httpOkHeader("HTTP/1.1 200 OK\r\nConnection: close\r\n\r\n");
	_httpOkHeader.insert(_httpOkHeader.end(), httpOkHeader.begin(), httpOkHeader.end());
}

void IpCamPeer::dispose()
{
	if(_disposing) return;
	Peer::dispose();
	GD::out.printInfo("Info: Removing Webserver hooks. If Homegear hangs here, Sockets are still open.");
	removeHooks();
}

void IpCamPeer::removeHooks()
{
	raiseRemoveWebserverEventHandler();
}

void IpCamPeer::homegearStarted()
{
	try
	{
		Peer::homegearStarted();
		raiseAddWebserverEventHandler(this);
		initHttpClient();
	}
	catch(const std::exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(BaseLib::Exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(...)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
	}
}

void IpCamPeer::homegearShuttingDown()
{
	try
	{
		_shuttingDown = true;
		Peer::homegearShuttingDown();
		removeHooks();
	}
	catch(const std::exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(BaseLib::Exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(...)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
	}
}

std::string IpCamPeer::handleCliCommand(std::string command)
{
	try
	{
		std::ostringstream stringStream;

		if(command == "help")
		{
			stringStream << "List of commands:" << std::endl << std::endl;
			stringStream << "For more information about the individual command type: COMMAND help" << std::endl << std::endl;
			stringStream << "unselect\t\tUnselect this peer" << std::endl;
			stringStream << "channel count\t\tPrint the number of channels of this peer" << std::endl;
			stringStream << "config print\t\tPrints all configuration parameters and their values" << std::endl;
			return stringStream.str();
		}
		if(command.compare(0, 13, "channel count") == 0)
		{
			std::stringstream stream(command);
			std::string element;
			int32_t index = 0;
			while(std::getline(stream, element, ' '))
			{
				if(index < 2)
				{
					index++;
					continue;
				}
				else if(index == 2)
				{
					if(element == "help")
					{
						stringStream << "Description: This command prints this peer's number of channels." << std::endl;
						stringStream << "Usage: channel count" << std::endl << std::endl;
						stringStream << "Parameters:" << std::endl;
						stringStream << "  There are no parameters." << std::endl;
						return stringStream.str();
					}
				}
				index++;
			}

			stringStream << "Peer has " << _rpcDevice->functions.size() << " channels." << std::endl;
			return stringStream.str();
		}
		else if(command.compare(0, 12, "config print") == 0)
		{
			std::stringstream stream(command);
			std::string element;
			int32_t index = 0;
			while(std::getline(stream, element, ' '))
			{
				if(index < 2)
				{
					index++;
					continue;
				}
				else if(index == 2)
				{
					if(element == "help")
					{
						stringStream << "Description: This command prints all configuration parameters of this peer. The values are in BidCoS packet format." << std::endl;
						stringStream << "Usage: config print" << std::endl << std::endl;
						stringStream << "Parameters:" << std::endl;
						stringStream << "  There are no parameters." << std::endl;
						return stringStream.str();
					}
				}
				index++;
			}

			return printConfig();
		}
		else return "Unknown command.\n";
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return "Error executing command. See log file for more details.\n";
}

std::string IpCamPeer::printConfig()
{
	try
	{
		std::ostringstream stringStream;
		stringStream << "MASTER" << std::endl;
		stringStream << "{" << std::endl;
		for(std::unordered_map<uint32_t, std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>>::iterator i = configCentral.begin(); i != configCentral.end(); ++i)
		{
			stringStream << "\t" << "Channel: " << std::dec << i->first << std::endl;
			stringStream << "\t{" << std::endl;
			for(std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>::iterator j = i->second.begin(); j != i->second.end(); ++j)
			{
				stringStream << "\t\t[" << j->first << "]: ";
				if(!j->second.rpcParameter) stringStream << "(No RPC parameter) ";
				std::vector<uint8_t> parameterData = j->second.getBinaryData();
				for(std::vector<uint8_t>::const_iterator k = parameterData.begin(); k != parameterData.end(); ++k)
				{
					stringStream << std::hex << std::setfill('0') << std::setw(2) << (int32_t)*k << " ";
				}
				stringStream << std::endl;
			}
			stringStream << "\t}" << std::endl;
		}
		stringStream << "}" << std::endl << std::endl;

		stringStream << "VALUES" << std::endl;
		stringStream << "{" << std::endl;
		for(std::unordered_map<uint32_t, std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>>::iterator i = valuesCentral.begin(); i != valuesCentral.end(); ++i)
		{
			stringStream << "\t" << "Channel: " << std::dec << i->first << std::endl;
			stringStream << "\t{" << std::endl;
			for(std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>::iterator j = i->second.begin(); j != i->second.end(); ++j)
			{
				stringStream << "\t\t[" << j->first << "]: ";
				if(!j->second.rpcParameter) stringStream << "(No RPC parameter) ";
				std::vector<uint8_t> parameterData = j->second.getBinaryData();
				for(std::vector<uint8_t>::const_iterator k = parameterData.begin(); k != parameterData.end(); ++k)
				{
					stringStream << std::hex << std::setfill('0') << std::setw(2) << (int32_t)*k << " ";
				}
				stringStream << std::endl;
			}
			stringStream << "\t}" << std::endl;
		}
		stringStream << "}" << std::endl << std::endl;

		return stringStream.str();
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return "";
}


void IpCamPeer::loadVariables(BaseLib::Systems::ICentral* central, std::shared_ptr<BaseLib::Database::DataTable>& rows)
{
	try
	{
		if(!rows) rows = _bl->db->getPeerVariables(_peerID);
		Peer::loadVariables(central, rows);
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

bool IpCamPeer::load(BaseLib::Systems::ICentral* central)
{
	try
	{
		std::shared_ptr<BaseLib::Database::DataTable> rows;
		loadVariables(central, rows);

		_rpcDevice = GD::family->getRpcDevices()->find(_deviceType, _firmwareVersion, -1);
		if(!_rpcDevice)
		{
			GD::out.printError("Error loading IpCam peer " + std::to_string(_peerID) + ": Device type not found: 0x" + BaseLib::HelperFunctions::getHexString(_deviceType) + " Firmware version: " + std::to_string(_firmwareVersion));
			return false;
		}
		initializeTypeString();
		std::string entry;
		loadConfig();
		initializeCentralConfig();

		serviceMessages.reset(new BaseLib::Systems::ServiceMessages(_bl, _peerID, _serialNumber, this));
		serviceMessages->load();

		BaseLib::Systems::RpcConfigurationParameter& parameter = valuesCentral[1]["MOTION"];
		if(parameter.rpcParameter)
		{
			std::vector<uint8_t> parameterData = parameter.getBinaryData();
			if(!parameterData.empty() && parameterData.at(0))
			{
				_motion = true;
				_motionTime = BaseLib::HelperFunctions::getTime();
				parameter.rpcParameter->convertToPacket(BaseLib::PVariable(new BaseLib::Variable(true)), parameterData);
				if(parameter.databaseId > 0) saveParameter(parameter.databaseId, parameterData);
				else saveParameter(0, ParameterGroup::Type::Enum::variables, 1, "MOTION", parameterData);
			}
		}
		parameter = configCentral[0]["RESET_MOTION_AFTER"];
		if(parameter.rpcParameter)
		{
			std::vector<uint8_t> parameterData = parameter.getBinaryData();
			_resetMotionAfter = parameter.rpcParameter->convertFromPacket(parameterData)->integerValue * 1000;
			if(_resetMotionAfter < 5000) _resetMotionAfter = 5000;
			else if(_resetMotionAfter > 3600000) _resetMotionAfter = 3600000;
		}

		return true;
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return false;
}

void IpCamPeer::saveVariables()
{
	try
	{
		if(_peerID == 0) return;
		Peer::saveVariables();
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

PParameterGroup IpCamPeer::getParameterSet(int32_t channel, ParameterGroup::Type::Enum type)
{
	try
	{
		PFunction rpcChannel = _rpcDevice->functions.at(channel);
		if(type == ParameterGroup::Type::Enum::variables) return rpcChannel->variables;
		else if(type == ParameterGroup::Type::Enum::config) return rpcChannel->configParameters;
		else if(type == ParameterGroup::Type::Enum::link) return rpcChannel->linkParameters;
	}
	catch(const std::exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(BaseLib::Exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(...)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
	}
	return PParameterGroup();
}

// {{{ Webserver events
	bool IpCamPeer::onGet(BaseLib::Rpc::PServerInfo& serverInfo, BaseLib::Http& httpRequest, std::shared_ptr<BaseLib::TcpSocket>& socket, std::string& path)
	{
		if(path == "/ipcam/" + std::to_string(_peerID) + "/stream.mjpeg")
		{
			if(_streamUrlInfo.ip.empty())
			{
				GD::out.printWarning("Warning: Can't open stream for peer with id " + std::to_string(_peerID) + ": IP address is empty.");
				return false;
			}
			BaseLib::TcpSocket cameraSocket(_bl, _streamUrlInfo.ip, std::to_string(_streamUrlInfo.port), _streamUrlInfo.ssl, _caFile, _verifyCertificate);
			try
			{
				std::string response;
				cameraSocket.open();
				cameraSocket.setReadTimeout(30000000);
				int32_t bufferMax = 2048;
				char buffer[bufferMax + 1];
				ssize_t receivedBytes;
				std::string getRequest = "GET " + _streamUrlInfo.path + " HTTP/1.1\r\nUser-Agent: Homegear\r\nHost: " + _streamUrlInfo.ip + ":" + std::to_string(_streamUrlInfo.port) + "\r\nConnection: " + "Close" + "\r\n";
				for(std::map<std::string, std::string>::iterator i = httpRequest.getHeader().fields.begin(); i != httpRequest.getHeader().fields.end(); ++i)
				{
					if(i->first == "user-agent" || i->first == "host" || i->first == "connection") continue;
					getRequest += i->first + ": " + i->second + "\r\n";
				}
				getRequest += "\r\n";
				cameraSocket.proofwrite(getRequest);
				while(!_disposing && !deleting && !_shuttingDown)
				{
					receivedBytes = cameraSocket.proofread(buffer, bufferMax);
					socket->proofwrite(buffer, receivedBytes);
				}
				cameraSocket.close();
				socket->close();
			}
			catch(BaseLib::SocketDataLimitException& ex)
			{
				GD::out.printWarning("Warning: " + ex.what());
			}
			catch(BaseLib::SocketClosedException& ex)
			{
				GD::out.printInfo("Info: " + ex.what());
			}
			catch(const BaseLib::SocketOperationException& ex)
			{
				GD::out.printError("Error: " + ex.what());
			}
			return true;
		}
		else if(path == "/ipcam/" + std::to_string(_peerID) + "/snapshot.jpg")
		{
			if(_snapshotUrlInfo.ip.empty())
			{
				GD::out.printWarning("Warning: Can't open stream for peer with id " + std::to_string(_peerID) + ": IP address is empty.");
				return false;
			}
			try
			{
				_httpClient.reset(new BaseLib::HttpClient(_bl, _snapshotUrlInfo.ip, _snapshotUrlInfo.port, false, _snapshotUrlInfo.ssl, _caFile, _verifyCertificate));
				std::string getRequest = "GET " + _snapshotUrlInfo.path + " HTTP/1.1\r\nUser-Agent: Homegear\r\nHost: " + _snapshotUrlInfo.ip + ":" + std::to_string(_snapshotUrlInfo.port) + "\r\nConnection: " + "Close" + "\r\n\r\n";
				Http response;
				_httpClient->sendRequest(getRequest, response, false);
				socket->proofwrite(response.getRawHeader());
				socket->proofwrite(response.getContent());
			}
			catch(BaseLib::HttpClientException& ex)
			{
				GD::out.printWarning("Warning" + ex.what());
			}
			catch(BaseLib::Exception& ex)
			{
				GD::out.printWarning("Warning" + ex.what());
			}
			return true;
		}
		else if(path == "/ipcam/" + std::to_string(_peerID) + "/motion")
		{
			try
			{
				socket->proofwrite(_httpOkHeader);
				socket->close();
			}
			catch(BaseLib::SocketDataLimitException& ex)
			{
				GD::out.printWarning("Warning: " + ex.what());
			}
			catch(const BaseLib::SocketOperationException& ex)
			{
				GD::out.printError("Error: " + ex.what());
			}
			BaseLib::Systems::RpcConfigurationParameter& parameter = valuesCentral[1]["MOTION"];
			if(!parameter.rpcParameter) return true;
			std::vector<uint8_t> parameterData{ 1 };
			parameter.setBinaryData(parameterData);
			if(parameter.databaseId > 0) saveParameter(parameter.databaseId, parameterData);
			else saveParameter(0, ParameterGroup::Type::Enum::variables, 1, "MOTION", parameterData);
			if(_bl->debugLevel >= 4) GD::out.printInfo("Info: MOTION of peer " + std::to_string(_peerID) + " with serial number " + _serialNumber + ":1 was set to true.");
			std::shared_ptr<std::vector<std::string>> valueKeys(new std::vector<std::string>{ "MOTION" });
			std::shared_ptr<std::vector<PVariable>> values(new std::vector<PVariable> { parameter.rpcParameter->convertFromPacket(parameterData, true) });
			std::string address(_serialNumber + ":1");
			_motion = true;
			_motionTime = BaseLib::HelperFunctions::getTime();
			raiseEvent(_peerID, 1, valueKeys, values);
			raiseRPCEvent(_peerID, 1, address, valueKeys, values);
			parameter = configCentral[0]["RESET_MOTION_AFTER"];
			if(parameter.rpcParameter)
			{
				parameterData = parameter.getBinaryData();
				_resetMotionAfter = parameter.rpcParameter->convertFromPacket(parameterData)->integerValue * 1000;
				if(_resetMotionAfter < 5000) _resetMotionAfter = 5000;
				else if(_resetMotionAfter > 3600000) _resetMotionAfter = 3600000;
			}
			return true;
		}
		return false;
	}
// }}}

IpCamPeer::UrlInfo IpCamPeer::getUrlInfo(std::string url)
{
	UrlInfo urlInfo;
	try
	{
		if(url.size() > 8)
		{
			std::string temp = url.substr(0, 5);
			_bl->hf.toLower(temp);
			if(temp == "https")
			{
				urlInfo.ssl = true;
				url = url.substr(8);
			}
			else if(temp == "http:")
			{
				urlInfo.ssl = false;
				url = url.substr(7);
			}
			else
			{
				GD::out.printWarning("Warning: STREAM_URL does not start with \"http\" or \"https\".");
				return urlInfo;
			}
			std::pair<std::string, std::string> parts = _bl->hf.splitFirst(url, ':');
			if(parts.second.empty() || (!parts.second.empty() && parts.first.find('/') != std::string::npos))
			{
				urlInfo.ip = _bl->hf.splitFirst(parts.first, '/').first;
				urlInfo.port = urlInfo.ssl ? 443 : 80;
			}
			else
			{
				urlInfo.ip = parts.first;
				parts = _bl->hf.splitFirst(parts.second, '/');
				urlInfo.port = BaseLib::Math::getNumber(parts.first);
			}
			parts = _bl->hf.splitFirst(url, '/');
			urlInfo.path = '/' + parts.second;
		}

		return urlInfo;
	}
	catch(const std::exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(BaseLib::Exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(...)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
	}
	return urlInfo;
}

void IpCamPeer::initHttpClient()
{
	try
	{
		{
			BaseLib::Systems::RpcConfigurationParameter& parameter = configCentral[0]["STREAM_URL"];
			if(parameter.rpcParameter)
			{
				std::vector<uint8_t> parameterData = parameter.getBinaryData();
				std::string streamUrl = parameter.rpcParameter->convertFromPacket(parameterData)->stringValue;
				_streamUrlInfo = getUrlInfo(streamUrl);
			}
		}

		{
			BaseLib::Systems::RpcConfigurationParameter& parameter = configCentral[0]["SNAPSHOT_URL"];
			if(parameter.rpcParameter)
			{
				std::vector<uint8_t> parameterData = parameter.getBinaryData();
				std::string streamUrl = parameter.rpcParameter->convertFromPacket(parameterData)->stringValue;
				_snapshotUrlInfo = getUrlInfo(streamUrl);
			}
		}

		{
			BaseLib::Systems::RpcConfigurationParameter& parameter = configCentral[0]["CA_FILE"];
			std::vector<uint8_t> parameterData = parameter.getBinaryData();
			if(parameter.rpcParameter) _caFile = parameter.rpcParameter->convertFromPacket(parameterData)->stringValue;
			parameter = configCentral[0]["VERIFY_CERTIFICATE"];
			parameterData = parameter.getBinaryData();
			if(parameter.rpcParameter) _verifyCertificate = parameter.rpcParameter->convertFromPacket(parameterData)->booleanValue;
		}

		if(_streamUrlInfo.ip.empty())
		{
			GD::out.printWarning("Warning: Can't init HTTP client of peer with id " + std::to_string(_peerID) + ": Please set STREAM_URL to a valid value.");
			return;
		}

		BaseLib::Systems::RpcConfigurationParameter& parameter = valuesCentral[1]["STREAM_URL"];
		if(parameter.rpcParameter && _bl->rpcPort != 0)
		{
			std::vector<uint8_t> parameterData = parameter.getBinaryData();
			BaseLib::PVariable variable = parameter.rpcParameter->convertFromPacket(parameterData, true);
			std::string newPrefix("http://" + GD::physicalInterface->listenAddress() + (GD::bl->rpcPort != 80 ? ":" + std::to_string(GD::bl->rpcPort) : "") + "/ipcam/" + std::to_string(_peerID) + "/");
			std::string newStreamUrl(newPrefix + "stream.mjpeg");
			if(variable->stringValue != newStreamUrl)
			{
				variable->stringValue = newStreamUrl;
				parameter.rpcParameter->convertToPacket(variable, parameterData);
				parameter.setBinaryData(parameterData);
				if(parameter.databaseId > 0) saveParameter(parameter.databaseId, parameterData);
				else saveParameter(0, ParameterGroup::Type::Enum::variables, 1, "STREAM_URL", parameterData);
				std::shared_ptr<std::vector<std::string>> valueKeys(new std::vector<std::string>{ "STREAM_URL" });
				std::shared_ptr<std::vector<PVariable>> values(new std::vector<PVariable> { variable });
				std::string address(_serialNumber + ":1");
				if(_bl->debugLevel >= 4) GD::out.printInfo("Info: STREAM_URL of peer " + std::to_string(_peerID) + " with serial number " + _serialNumber + ":1 was set to " + variable->stringValue + ".");

				BaseLib::Systems::RpcConfigurationParameter& parameter2 = valuesCentral[1]["SNAPSHOT_URL"];
				if(parameter2.rpcParameter)
				{
					variable = std::make_shared<Variable>(newPrefix + "snapshot.jpg");
					parameter2.rpcParameter->convertToPacket(variable, parameterData);
					parameter2.setBinaryData(parameterData);
					if(parameter2.databaseId > 0) saveParameter(parameter2.databaseId, parameterData);
					else saveParameter(0, ParameterGroup::Type::Enum::variables, 1, "SNAPSHOT_URL", parameterData);
					valueKeys->push_back("SNAPSHOT_URL");
					values->push_back(variable);
					std::string address(_serialNumber + ":1");
					if(_bl->debugLevel >= 4) GD::out.printInfo("Info: SNAPSHOT_URL of peer " + std::to_string(_peerID) + " with serial number " + _serialNumber + ":1 was set to " + variable->stringValue + ".");
				}

				raiseEvent(_peerID, 1, valueKeys, values);
				raiseRPCEvent(_peerID, 1, address, valueKeys, values);
			}
		}
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

PVariable IpCamPeer::getDeviceInfo(BaseLib::PRpcClientInfo clientInfo, std::map<std::string, bool> fields)
{
	try
	{
		PVariable info(Peer::getDeviceInfo(clientInfo, fields));
		return info;
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return PVariable();
}

PVariable IpCamPeer::getParamset(BaseLib::PRpcClientInfo clientInfo, int32_t channel, ParameterGroup::Type::Enum type, uint64_t remoteID, int32_t remoteChannel)
{
	try
	{
		if(_disposing) return Variable::createError(-32500, "Peer is disposing.");
		if(channel < 0) channel = 0;
		if(remoteChannel < 0) remoteChannel = 0;
		Functions::iterator functionIterator = _rpcDevice->functions.find(channel);
		if(functionIterator == _rpcDevice->functions.end()) return Variable::createError(-2, "Unknown channel.");
		if(type == ParameterGroup::Type::none) type = ParameterGroup::Type::link;
		PFunction rpcFunction = functionIterator->second;
		PParameterGroup parameterGroup = rpcFunction->getParameterGroup(type);
		if(!parameterGroup) return Variable::createError(-3, "Unknown parameter set.");
		PVariable variables(new Variable(VariableType::tStruct));

		for(Parameters::iterator i = parameterGroup->parameters.begin(); i != parameterGroup->parameters.end(); ++i)
		{
			if(!i->second || i->second->id.empty() || !i->second->visible) continue;
			if(!i->second->visible && !i->second->service && !i->second->internal && !i->second->transform)
			{
				GD::out.printDebug("Debug: Omitting parameter " + i->second->id + " because of it's ui flag.");
				continue;
			}
			PVariable element;
			if(type == ParameterGroup::Type::Enum::variables)
			{
				if(!i->second->readable) continue;
				if(valuesCentral.find(channel) == valuesCentral.end()) continue;
				if(valuesCentral[channel].find(i->second->id) == valuesCentral[channel].end()) continue;
				std::vector<uint8_t> parameterData = valuesCentral[channel][i->second->id].getBinaryData();
				element = i->second->convertFromPacket(parameterData);
			}
			else if(type == ParameterGroup::Type::Enum::config)
			{
				if(configCentral.find(channel) == configCentral.end()) continue;
				if(configCentral[channel].find(i->second->id) == configCentral[channel].end()) continue;
				std::vector<uint8_t> parameterData = configCentral[channel][i->second->id].getBinaryData();
				element = i->second->convertFromPacket(parameterData);
			}
			else if(type == ParameterGroup::Type::Enum::link)
			{
				return Variable::createError(-3, "Parameter set type is not supported.");
			}

			if(!element) continue;
			if(i->second->password) element.reset(new BaseLib::Variable(element->type));
			if(element->type == VariableType::tVoid) continue;
			variables->structValue->insert(StructElement(i->second->id, element));
		}
		return variables;
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return Variable::createError(-32500, "Unknown application error.");
}

PVariable IpCamPeer::getParamsetDescription(BaseLib::PRpcClientInfo clientInfo, int32_t channel, ParameterGroup::Type::Enum type, uint64_t remoteID, int32_t remoteChannel)
{
	try
	{
		if(_disposing) return Variable::createError(-32500, "Peer is disposing.");
		if(channel < 0) channel = 0;
		Functions::iterator functionIterator = _rpcDevice->functions.find(channel);
		if(functionIterator == _rpcDevice->functions.end()) return Variable::createError(-2, "Unknown channel");
		PParameterGroup parameterGroup = functionIterator->second->getParameterGroup(type);
		if(!parameterGroup) return Variable::createError(-3, "Unknown parameter set");
		if(type == ParameterGroup::Type::link && remoteID > 0)
		{
			std::shared_ptr<BaseLib::Systems::BasicPeer> remotePeer = getPeer(channel, remoteID, remoteChannel);
			if(!remotePeer) return Variable::createError(-2, "Unknown remote peer.");
		}

		return Peer::getParamsetDescription(clientInfo, parameterGroup);
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return Variable::createError(-32500, "Unknown application error.");
}

PVariable IpCamPeer::getValue(BaseLib::PRpcClientInfo clientInfo, uint32_t channel, std::string valueKey, bool requestFromDevice, bool asynchronous)
{
	try
	{
		if(_disposing) return Variable::createError(-32500, "Peer is disposing.");
		if(!_rpcDevice) return Variable::createError(-32500, "Unknown application error.");
		if(valuesCentral.find(channel) == valuesCentral.end()) return Variable::createError(-2, "Unknown channel.");
		if(valuesCentral[channel].find(valueKey) == valuesCentral[channel].end()) return Variable::createError(-5, "Unknown parameter.");
		if(_rpcDevice->functions.find(channel) == _rpcDevice->functions.end()) return Variable::createError(-2, "Unknown channel.");
		PFunction rpcFunction = _rpcDevice->functions.at(channel);
		PParameterGroup parameterGroup = getParameterSet(channel, ParameterGroup::Type::Enum::variables);
		if(!parameterGroup) return Variable::createError(-3, "Unknown parameter set.");
		PParameter parameter = parameterGroup->parameters.at(valueKey);
		if(!parameter) return Variable::createError(-5, "Unknown parameter.");
		if(!parameter->readable) return Variable::createError(-6, "Parameter is not readable.");

		BaseLib::PVariable variable;
		if(parameter->getPackets.empty())
		{
			std::vector<uint8_t> parameterData = valuesCentral[channel][valueKey].getBinaryData();
			variable = parameter->convertFromPacket(parameterData);
			if(parameter->password) variable.reset(new Variable(variable->type));
			return variable;
		}
		else
		{
			variable = getValueFromDevice(parameter, channel, asynchronous);
			if(parameter->password) variable.reset(new Variable(variable->type));
			return variable;
		}
	}
	catch(const std::exception& ex)
    {
    	_bl->out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	_bl->out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_bl->out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return Variable::createError(-32500, "Unknown application error.");
}

PVariable IpCamPeer::putParamset(BaseLib::PRpcClientInfo clientInfo, int32_t channel, ParameterGroup::Type::Enum type, uint64_t remoteID, int32_t remoteChannel, PVariable variables, bool onlyPushing)
{
	try
	{
		if(_disposing) return Variable::createError(-32500, "Peer is disposing.");
		if(channel < 0) channel = 0;
		if(remoteChannel < 0) remoteChannel = 0;
		Functions::iterator functionIterator = _rpcDevice->functions.find(channel);
		if(functionIterator == _rpcDevice->functions.end()) return Variable::createError(-2, "Unknown channel.");
		if(type == ParameterGroup::Type::none) type = ParameterGroup::Type::link;
		PParameterGroup parameterGroup = functionIterator->second->getParameterGroup(type);
		if(!parameterGroup) return Variable::createError(-3, "Unknown parameter set.");
		if(variables->structValue->empty()) return PVariable(new Variable(VariableType::tVoid));

		if(type == ParameterGroup::Type::Enum::config)
		{
			bool reloadHttpClient = false;
			std::map<int32_t, std::map<int32_t, std::vector<uint8_t>>> changedParameters;
			//allParameters is necessary to temporarily store all values. It is used to set changedParameters.
			//This is necessary when there are multiple variables per index and not all of them are changed.
			std::map<int32_t, std::map<int32_t, std::vector<uint8_t>>> allParameters;
			for(Struct::iterator i = variables->structValue->begin(); i != variables->structValue->end(); ++i)
			{
				if(i->first.empty() || !i->second) continue;
				std::vector<uint8_t> value;
				if(configCentral[channel].find(i->first) == configCentral[channel].end()) continue;
				BaseLib::Systems::RpcConfigurationParameter& parameter = configCentral[channel][i->first];
				if(!parameter.rpcParameter) continue;
				if(parameter.rpcParameter->password && i->second->stringValue.empty()) continue; //Don't safe password if empty
				parameter.rpcParameter->convertToPacket(i->second, value);
				std::vector<uint8_t> shiftedValue = value;
				parameter.rpcParameter->adjustBitPosition(shiftedValue);
				int32_t intIndex = (int32_t)parameter.rpcParameter->physical->index;
				int32_t list = parameter.rpcParameter->physical->list;
				if(list == -1) list = 0;
				if(allParameters[list].find(intIndex) == allParameters[list].end()) allParameters[list][intIndex] = shiftedValue;
				else
				{
					uint32_t index = 0;
					for(std::vector<uint8_t>::iterator j = shiftedValue.begin(); j != shiftedValue.end(); ++j)
					{
						if(index >= allParameters[list][intIndex].size()) allParameters[list][intIndex].push_back(0);
						allParameters[list][intIndex].at(index) |= *j;
						index++;
					}
				}
				parameter.setBinaryData(value);
				if(parameter.databaseId > 0) saveParameter(parameter.databaseId, value);
				else saveParameter(0, ParameterGroup::Type::Enum::config, channel, i->first, value);

				if(channel == 0 && (i->first == "STREAM_URL" || i->first == "SNAPSHOT_URL" || i->first == "CA_FILE" || i->first == "VERIFY_CERTIFICATE")) reloadHttpClient = true;

				GD::out.printInfo("Info: Parameter " + i->first + " of peer " + std::to_string(_peerID) + " and channel " + std::to_string(channel) + " was set to 0x" + BaseLib::HelperFunctions::getHexString(allParameters[list][intIndex]) + ".");
				//Only send to device when parameter is of type config
				if(parameter.rpcParameter->physical->operationType != IPhysical::OperationType::Enum::config && parameter.rpcParameter->physical->operationType != IPhysical::OperationType::Enum::configString) continue;
				changedParameters[list][intIndex] = allParameters[list][intIndex];
			}

			if(reloadHttpClient) initHttpClient();
			if(!changedParameters.empty() && !changedParameters.begin()->second.empty()) raiseRPCUpdateDevice(_peerID, channel, _serialNumber + ":" + std::to_string(channel), 0);
		}
		else if(type == ParameterGroup::Type::Enum::variables)
		{
			for(Struct::iterator i = variables->structValue->begin(); i != variables->structValue->end(); ++i)
			{
				if(i->first.empty() || !i->second) continue;
				setValue(clientInfo, channel, i->first, i->second, false);
			}
		}
		else
		{
			return Variable::createError(-3, "Parameter set type is not supported.");
		}
		return PVariable(new Variable(VariableType::tVoid));
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return Variable::createError(-32500, "Unknown application error.");
}

PVariable IpCamPeer::setValue(BaseLib::PRpcClientInfo clientInfo, uint32_t channel, std::string valueKey, PVariable value, bool wait)
{
	try
	{
		if(!clientInfo) clientInfo.reset(new BaseLib::RpcClientInfo());
		Peer::setValue(clientInfo, channel, valueKey, value, wait); //Ignore result, otherwise setHomegerValue might not be executed
		if(_disposing) return Variable::createError(-32500, "Peer is disposing.");
		if(valueKey.empty()) return Variable::createError(-5, "Value key is empty.");
		if(valuesCentral.find(channel) == valuesCentral.end()) return Variable::createError(-2, "Unknown channel.");

		if(valueKey.size() == 18 && valueKey.compare(0, 16, "OPEN_CUSTOM_URL_") == 0)
		{
			std::string number = valueKey.substr(16);
			BaseLib::Systems::RpcConfigurationParameter& parameter = configCentral[0]["CUSTOM_URL_" + number];
			if(parameter.rpcParameter)
			{
				std::vector<uint8_t> parameterData = parameter.getBinaryData();
				std::string customUrl = parameter.rpcParameter->convertFromPacket(parameterData)->stringValue;
				UrlInfo info = getUrlInfo(customUrl);
				if(customUrl.empty()) return Variable::createError(-1, "CUSTOM_URL_" + number + " is not set.");
				else if(info.ip.empty()) return Variable::createError(-1, "Could not get IP address from custom URL.");
				_httpClient.reset(new BaseLib::HttpClient(_bl, info.ip, info.port, false, info.ssl, _caFile, _verifyCertificate));
				std::string getRequest = "GET " + info.path + " HTTP/1.1\r\nUser-Agent: Homegear\r\nHost: " + info.ip + ":" + std::to_string(info.port) + "\r\nConnection: " + "Close" + "\r\n\r\n";
				Http response;
				GD::out.printInfo("Info: Calling URL: " + customUrl);
				_httpClient->sendRequest(getRequest, response, false);
				GD::out.printInfo("Info: HTTP result code: " + std::to_string(response.getHeader().responseCode));
			}
			return PVariable(new Variable(VariableType::tVoid));
		}
		else return Variable::createError(-5, "Unknown parameter.");
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return Variable::createError(-32500, "Unknown application error. See error log for more details.");
}

}
