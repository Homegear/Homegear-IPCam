/* Copyright 2013-2019 Homegear GmbH
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

#include "IpCamCentral.h"
#include "GD.h"

#include <iomanip>

namespace IpCam {

IpCamCentral::IpCamCentral(ICentralEventSink* eventHandler) : BaseLib::Systems::ICentral(IPCAM_FAMILY_ID, GD::bl, eventHandler)
{
	init();
}

IpCamCentral::IpCamCentral(uint32_t deviceID, std::string serialNumber, ICentralEventSink* eventHandler) : BaseLib::Systems::ICentral(IPCAM_FAMILY_ID, GD::bl, deviceID, serialNumber, -1, eventHandler)
{
	init();
}

IpCamCentral::~IpCamCentral()
{
	dispose();
}

void IpCamCentral::dispose(bool wait)
{
	try
	{
		if(_disposing) return;
		_disposing = true;

		_stopWorkerThread = true;
		GD::bl->threadManager.join(_workerThread);
	}
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void IpCamCentral::init()
{
	try
	{
		if(_initialized) return; //Prevent running init two times
		_initialized = true;

		_stopWorkerThread = false;

		_bl->threadManager.start(_workerThread, true, _bl->settings.workerThreadPriority(), _bl->settings.workerThreadPolicy(), &IpCamCentral::worker, this);
	}
	catch(const std::exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
}

void IpCamCentral::worker()
{
	try
	{
		std::chrono::milliseconds sleepingTime(10);
		uint32_t counter = 0;
		uint64_t lastPeer = 0;
		//One loop on the Raspberry Pi takes about 30Âµs
		while(!_stopWorkerThread)
		{
			try
			{
				std::this_thread::sleep_for(sleepingTime);
				if(_stopWorkerThread) return;
				if(counter > 10000)
				{
					counter = 0;
					_peersMutex.lock();
					if(_peersById.size() > 0)
					{
						int32_t windowTimePerPeer = _bl->settings.workerThreadWindow() / _peersById.size();
						if(windowTimePerPeer > 2) windowTimePerPeer -= 2;
						sleepingTime = std::chrono::milliseconds(windowTimePerPeer);
					}
					_peersMutex.unlock();
				}
				_peersMutex.lock();
				if(!_peersById.empty())
				{
					std::map<uint64_t, std::shared_ptr<BaseLib::Systems::Peer>>::iterator nextPeer = _peersById.find(lastPeer);
					if(nextPeer != _peersById.end())
					{
						nextPeer++;
						if(nextPeer == _peersById.end()) nextPeer = _peersById.begin();
					}
					else nextPeer = _peersById.begin();
					lastPeer = nextPeer->first;
				}
				_peersMutex.unlock();
				std::shared_ptr<IpCamPeer> peer(getPeer(lastPeer));
				if(peer && !peer->deleting) peer->worker();
				counter++;
			}
			catch(const std::exception& ex)
			{
				_peersMutex.unlock();
				GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
		}
	}
    catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void IpCamCentral::loadPeers()
{
	try
	{
		std::shared_ptr<BaseLib::Database::DataTable> rows = _bl->db->getPeers(_deviceId);
		for(BaseLib::Database::DataTable::iterator row = rows->begin(); row != rows->end(); ++row)
		{
			int32_t peerID = row->second.at(0)->intValue;
			GD::out.printMessage("Loading IpCam peer " + std::to_string(peerID));
			std::shared_ptr<IpCamPeer> peer(new IpCamPeer(peerID, row->second.at(3)->textValue, _deviceId, this));
			if(!peer->load(this)) continue;
			if(!peer->getRpcDevice()) continue;
			_peersMutex.lock();
			if(!peer->getSerialNumber().empty()) _peersBySerial[peer->getSerialNumber()] = peer;
			_peersById[peerID] = peer;
			_peersMutex.unlock();
		}
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    	_peersMutex.unlock();
    }
}

std::shared_ptr<IpCamPeer> IpCamCentral::getPeer(uint64_t id)
{
	try
	{
		_peersMutex.lock();
		if(_peersById.find(id) != _peersById.end())
		{
			std::shared_ptr<IpCamPeer> peer(std::dynamic_pointer_cast<IpCamPeer>(_peersById.at(id)));
			_peersMutex.unlock();
			return peer;
		}
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    _peersMutex.unlock();
    return std::shared_ptr<IpCamPeer>();
}

std::shared_ptr<IpCamPeer> IpCamCentral::getPeer(std::string serialNumber)
{
	try
	{
		_peersMutex.lock();
		if(_peersBySerial.find(serialNumber) != _peersBySerial.end())
		{
			std::shared_ptr<IpCamPeer> peer(std::dynamic_pointer_cast<IpCamPeer>(_peersBySerial.at(serialNumber)));
			_peersMutex.unlock();
			return peer;
		}
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    _peersMutex.unlock();
    return std::shared_ptr<IpCamPeer>();
}

void IpCamCentral::savePeers(bool full)
{
	try
	{
		_peersMutex.lock();
		for(std::map<uint64_t, std::shared_ptr<BaseLib::Systems::Peer>>::iterator i = _peersById.begin(); i != _peersById.end(); ++i)
		{
			//Necessary, because peers can be assigned to multiple virtual devices
			if(i->second->getParentID() != _deviceId) continue;
			//We are always printing this, because the init script needs it
			GD::out.printMessage("(Shutdown) => Saving IpCam peer " + std::to_string(i->second->getID()));
			i->second->save(full, full, full);
		}
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
	_peersMutex.unlock();
}

void IpCamCentral::deletePeer(uint64_t id)
{
	try
	{
		std::shared_ptr<IpCamPeer> peer(getPeer(id));
		if(!peer) return;
		peer->deleting = true;
		peer->removeHooks();
		PVariable deviceAddresses(new Variable(VariableType::tArray));
		deviceAddresses->arrayValue->push_back(PVariable(new Variable(peer->getSerialNumber())));

		PVariable deviceInfo(new Variable(VariableType::tStruct));
		deviceInfo->structValue->insert(StructElement("ID", PVariable(new Variable((int32_t)peer->getID()))));
		PVariable channels(new Variable(VariableType::tArray));
		deviceInfo->structValue->insert(StructElement("CHANNELS", channels));

		for(Functions::iterator i = peer->getRpcDevice()->functions.begin(); i != peer->getRpcDevice()->functions.end(); ++i)
		{
			deviceAddresses->arrayValue->push_back(PVariable(new Variable(peer->getSerialNumber() + ":" + std::to_string(i->first))));
			channels->arrayValue->push_back(PVariable(new Variable(i->first)));
		}

        std::vector<uint64_t> deletedIds{ id };
		raiseRPCDeleteDevices(deletedIds, deviceAddresses, deviceInfo);

		{
			std::lock_guard<std::mutex> peersGuard(_peersMutex);
			if(_peersBySerial.find(peer->getSerialNumber()) != _peersBySerial.end()) _peersBySerial.erase(peer->getSerialNumber());
			if(_peersById.find(id) != _peersById.end()) _peersById.erase(id);
		}

		int32_t i = 0;
		while(peer.use_count() > 1 && i < 600)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			i++;
		}
		if(i == 600) GD::out.printError("Error: Peer deletion took too long.");

		peer->deleteFromDatabase();

		GD::out.printMessage("Removed IpCam peer " + std::to_string(peer->getID()));
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

std::string IpCamCentral::handleCliCommand(std::string command)
{
	try
	{
		std::ostringstream stringStream;
		if(command == "help" || command == "h")
		{
			stringStream << "List of commands:" << std::endl << std::endl;
			stringStream << "For more information about the individual command type: COMMAND help" << std::endl << std::endl;
			stringStream << "peers create (pc)\t\tCreates a new peer" << std::endl;
			stringStream << "peers list (ls)\t\tList all peers" << std::endl;
			stringStream << "peers remove (pr)\tRemove a peer" << std::endl;
			stringStream << "peers select (ps)\tSelect a peer" << std::endl;
			stringStream << "peers setname (pn)\tName a peer" << std::endl;
			stringStream << "unselect (u)\t\tUnselect this device" << std::endl;
			return stringStream.str();
		}
		if(command.compare(0, 12, "peers create") == 0 || command.compare(0, 2, "pc") == 0)
		{
			uint32_t deviceType = 0;
			std::string serialNumber;

			std::stringstream stream(command);
			std::string element;
			int32_t offset = (command.at(1) == 'c') ? 0 : 1;
			int32_t index = 0;
			while(std::getline(stream, element, ' '))
			{
				if(index < 1 + offset)
				{
					index++;
					continue;
				}
				else if(index == 1 + offset)
				{
					if(element == "help") break;
					int32_t temp = BaseLib::Math::getNumber(element, true);
					if(temp == 0) return "Invalid device type. Device type has to be provided in hexadecimal format.\n";
					deviceType = temp;
				}
				else if(index == 2 + offset)
				{
					if(element.length() != 10) return "Invalid serial number. Please provide a serial number with a length of 10 characters.\n";
					serialNumber = element;
				}
				index++;
			}
			if(index < 3 + offset)
			{
				stringStream << "Description: This command create a new peer." << std::endl;
				stringStream << "Usage: peers create DEVICETYPE SERIALNUMBER" << std::endl << std::endl;
				stringStream << "Parameters:" << std::endl;
				stringStream << "  DEVICETYPE:\t\tThe 2 byte device type of the peer to add in hexadecimal format. Example: 0192" << std::endl;
				stringStream << "  SERIALNUMBER:\t\tThe 10 character long serial number of the peer to add. Example: 8G647D825Q" << std::endl;
				return stringStream.str();
			}
			if(peerExists(serialNumber)) stringStream << "This peer is already paired to this central." << std::endl;
			else
			{
				std::shared_ptr<IpCamPeer> peer = createPeer(deviceType, serialNumber, false);
				if(!peer || !peer->getRpcDevice()) return "Device type not supported.\n";
				try
				{
					_peersMutex.lock();
					if(!peer->getSerialNumber().empty()) _peersBySerial[peer->getSerialNumber()] = peer;
					_peersMutex.unlock();
					peer->save(true, true, false);
					peer->initializeCentralConfig();
					_peersMutex.lock();
					_peersById[peer->getID()] = peer;
					_peersMutex.unlock();
				}
				catch(const std::exception& ex)
				{
					_peersMutex.unlock();
					GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
				}

				PVariable deviceDescriptions(new Variable(VariableType::tArray));
				deviceDescriptions->arrayValue = peer->getDeviceDescriptions(nullptr, true, std::map<std::string, bool>());
                std::vector<uint64_t> newIds{ peer->getID() };
				raiseRPCNewDevices(newIds, deviceDescriptions);
				GD::out.printMessage("Added peer 0x" + BaseLib::HelperFunctions::getHexString(peer->getID()) + ".");
				stringStream << "Added peer " + std::to_string(peer->getID()) + " of type 0x" << BaseLib::HelperFunctions::getHexString(deviceType) << " with serial number " << serialNumber << "." << std::dec << std::endl;
			}
			return stringStream.str();
		}
		else if(command.compare(0, 12, "peers remove") == 0 || command.compare(0, 2, "pr") == 0)
		{
			uint64_t peerID = 0;

			std::stringstream stream(command);
			std::string element;
			int32_t offset = (command.at(1) == 'r') ? 0 : 1;
			int32_t index = 0;
			while(std::getline(stream, element, ' '))
			{
				if(index < 1 + offset)
				{
					index++;
					continue;
				}
				else if(index == 1 + offset)
				{
					if(element == "help") break;
					peerID = BaseLib::Math::getNumber(element, false);
					if(peerID == 0) return "Invalid id.\n";
				}
				index++;
			}
			if(index == 1 + offset)
			{
				stringStream << "Description: This command removes a peer." << std::endl;
				stringStream << "Usage: peers unpair PEERID" << std::endl << std::endl;
				stringStream << "Parameters:" << std::endl;
				stringStream << "  PEERID:\tThe id of the peer to remove. Example: 513" << std::endl;
				return stringStream.str();
			}

			if(!peerExists(peerID)) stringStream << "This peer is not paired to this central." << std::endl;
			else
			{
				stringStream << "Removing peer " << std::to_string(peerID) << std::endl;
				deletePeer(peerID);
			}
			return stringStream.str();
		}
		else if(command.compare(0, 10, "peers list") == 0 || command.compare(0, 2, "pl") == 0 || command.compare(0, 2, "ls") == 0)
		{
			try
			{
				std::string filterType;
				std::string filterValue;

				std::stringstream stream(command);
				std::string element;
				int32_t offset = (command.at(1) == 'l' || command.at(1) == 's') ? 0 : 1;
				int32_t index = 0;
				while(std::getline(stream, element, ' '))
				{
					if(index < 1 + offset)
					{
						index++;
						continue;
					}
					else if(index == 1 + offset)
					{
						if(element == "help")
						{
							index = -1;
							break;
						}
						filterType = BaseLib::HelperFunctions::toLower(element);
					}
					else if(index == 2 + offset)
					{
						filterValue = element;
						if(filterType == "name") BaseLib::HelperFunctions::toLower(filterValue);
					}
					index++;
				}
				if(index == -1)
				{
					stringStream << "Description: This command lists information about all peers." << std::endl;
					stringStream << "Usage: peers list [FILTERTYPE] [FILTERVALUE]" << std::endl << std::endl;
					stringStream << "Parameters:" << std::endl;
					stringStream << "  FILTERTYPE:\tSee filter types below." << std::endl;
					stringStream << "  FILTERVALUE:\tDepends on the filter type. If a number is required, it has to be in hexadecimal format." << std::endl << std::endl;
					stringStream << "Filter types:" << std::endl;
					stringStream << "  ID: Filter by id." << std::endl;
					stringStream << "      FILTERVALUE: The id of the peer to filter (e. g. 513)." << std::endl;
					stringStream << "  SERIAL: Filter by serial number." << std::endl;
					stringStream << "      FILTERVALUE: The serial number of the peer to filter (e. g. JEQ0554309)." << std::endl;
					stringStream << "  NAME: Filter by name." << std::endl;
					stringStream << "      FILTERVALUE: The part of the name to search for (e. g. \"1st floor\")." << std::endl;
					stringStream << "  TYPE: Filter by device type." << std::endl;
					stringStream << "      FILTERVALUE: The 2 byte device type in hexadecimal format." << std::endl;
					return stringStream.str();
				}

				if(_peersById.empty())
				{
					stringStream << "No peers are paired to this central." << std::endl;
					return stringStream.str();
				}
				std::string bar(" │ ");
				const int32_t idWidth = 8;
				const int32_t nameWidth = 25;
				const int32_t serialWidth = 13;
				const int32_t typeWidth1 = 4;
				const int32_t typeWidth2 = 25;
				std::string nameHeader("Name");
				nameHeader.resize(nameWidth, ' ');
				std::string typeStringHeader("Type String");
				typeStringHeader.resize(typeWidth2, ' ');
				stringStream << std::setfill(' ')
					<< std::setw(idWidth) << "ID" << bar
					<< nameHeader << bar
					<< std::setw(serialWidth) << "Serial Number" << bar
					<< std::setw(typeWidth1) << "Type" << bar
					<< typeStringHeader
					<< std::endl;
				stringStream << "─────────┼───────────────────────────┼───────────────┼──────┼───────────────────────────" << std::endl;
				stringStream << std::setfill(' ')
					<< std::setw(idWidth) << " " << bar
					<< std::setw(nameWidth) << " " << bar
					<< std::setw(serialWidth) << " " << bar
					<< std::setw(typeWidth1) << " " << bar
					<< std::setw(typeWidth2)
					<< std::endl;
				_peersMutex.lock();
				for(std::map<uint64_t, std::shared_ptr<BaseLib::Systems::Peer>>::iterator i = _peersById.begin(); i != _peersById.end(); ++i)
				{
					if(filterType == "id")
					{
						uint64_t id = BaseLib::Math::getNumber(filterValue, false);
						if(i->second->getID() != id) continue;
					}
					else if(filterType == "name")
					{
						std::string name = i->second->getName();
						if((signed)BaseLib::HelperFunctions::toLower(name).find(filterValue) == (signed)std::string::npos) continue;
					}
					else if(filterType == "serial")
					{
						if(i->second->getSerialNumber() != filterValue) continue;
					}
					else if(filterType == "type")
					{
						int32_t deviceType = BaseLib::Math::getNumber(filterValue, true);
						if((int32_t)i->second->getDeviceType() != deviceType) continue;
					}

					stringStream << std::setw(idWidth) << std::setfill(' ') << std::to_string(i->second->getID()) << bar;
					std::string name = i->second->getName();
					size_t nameSize = BaseLib::HelperFunctions::utf8StringSize(name);
					if(nameSize > (unsigned)nameWidth)
					{
						name = BaseLib::HelperFunctions::utf8Substring(name, 0, nameWidth - 3);
						name += "...";
					}
					else name.resize(nameWidth + (name.size() - nameSize), ' ');
					stringStream << name << bar
						<< std::setw(serialWidth) << i->second->getSerialNumber() << bar
						<< std::setw(typeWidth1) << BaseLib::HelperFunctions::getHexString(i->second->getDeviceType(), 4) << bar;
					if(i->second->getRpcDevice())
					{
						PSupportedDevice type = i->second->getRpcDevice()->getType(i->second->getDeviceType(), i->second->getFirmwareVersion());
						std::string typeID;
						if(type) typeID = type->id;
						if(typeID.size() > (unsigned)typeWidth2)
						{
							typeID.resize(typeWidth2 - 3);
							typeID += "...";
						}
						else typeID.resize(typeWidth2, ' ');
						stringStream << typeID;
					}
					else stringStream << std::setw(typeWidth2);
					stringStream << std::endl << std::dec;
				}
				_peersMutex.unlock();
				stringStream << "─────────┴───────────────────────────┴───────────────┴──────┴───────────────────────────" << std::endl;

				return stringStream.str();
			}
			catch(const std::exception& ex)
			{
				_peersMutex.unlock();
				GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
		}
		else if(command.compare(0, 13, "peers setname") == 0 || command.compare(0, 2, "pn") == 0)
		{
			uint64_t peerID = 0;
			std::string name;

			std::stringstream stream(command);
			std::string element;
			int32_t offset = (command.at(1) == 'n') ? 0 : 1;
			int32_t index = 0;
			while(std::getline(stream, element, ' '))
			{
				if(index < 1 + offset)
				{
					index++;
					continue;
				}
				else if(index == 1 + offset)
				{
					if(element == "help") break;
					else
					{
						peerID = BaseLib::Math::getNumber(element, false);
						if(peerID == 0) return "Invalid id.\n";
					}
				}
				else if(index == 2 + offset) name = element;
				else name += ' ' + element;
				index++;
			}
			if(index == 1 + offset)
			{
				stringStream << "Description: This command sets or changes the name of a peer to identify it more easily." << std::endl;
				stringStream << "Usage: peers setname PEERID NAME" << std::endl << std::endl;
				stringStream << "Parameters:" << std::endl;
				stringStream << "  PEERID:\tThe id of the peer to set the name for. Example: 513" << std::endl;
				stringStream << "  NAME:\tThe name to set. Example: \"1st floor light switch\"." << std::endl;
				return stringStream.str();
			}

			if(!peerExists(peerID)) stringStream << "This peer is not paired to this central." << std::endl;
			else
			{
				std::shared_ptr<IpCamPeer> peer = getPeer(peerID);
				peer->setName(name);
				stringStream << "Name set to \"" << name << "\"." << std::endl;
			}
			return stringStream.str();
		}
		else return "Unknown command.\n";
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    return "Error executing command. See log file for more details.\n";
}

std::shared_ptr<IpCamPeer> IpCamCentral::createPeer(uint32_t deviceType, std::string serialNumber, bool save)
{
	try
	{
		std::shared_ptr<IpCamPeer> peer(new IpCamPeer(_deviceId, this));
		peer->setDeviceType(deviceType);
		peer->setSerialNumber(serialNumber);
		peer->setRpcDevice(GD::family->getRpcDevices()->find(deviceType, 0x10, -1));
		if(!peer->getRpcDevice()) return std::shared_ptr<IpCamPeer>();
		if(save) peer->save(true, true, false); //Save and create peerID
		return peer;
	}
    catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    return std::shared_ptr<IpCamPeer>();
}

PVariable IpCamCentral::createDevice(BaseLib::PRpcClientInfo clientInfo, int32_t deviceType, std::string serialNumber, int32_t address, int32_t firmwareVersion, std::string interfaceId)
{
	try
	{
		if(serialNumber.size() != 10) return Variable::createError(-1, "The serial number needs to have a size of 10.");
		if(peerExists(serialNumber)) return Variable::createError(-5, "This peer is already paired to this central.");

		std::shared_ptr<IpCamPeer> peer = createPeer(deviceType, serialNumber, false);
		if(!peer || !peer->getRpcDevice()) return Variable::createError(-6, "Unknown device type.");

		try
		{
			_peersMutex.lock();
			if(!peer->getSerialNumber().empty()) _peersBySerial[peer->getSerialNumber()] = peer;
			_peersMutex.unlock();
			peer->save(true, true, false);
			peer->initializeCentralConfig();
			_peersMutex.lock();
			_peersById[peer->getID()] = peer;
			_peersMutex.unlock();
		}
		catch(const std::exception& ex)
		{
			_peersMutex.unlock();
			GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
		}

		PVariable deviceDescriptions(new Variable(VariableType::tArray));
		deviceDescriptions->arrayValue = peer->getDeviceDescriptions(clientInfo, true, std::map<std::string, bool>());
        std::vector<uint64_t> newIds{ peer->getID() };
		raiseRPCNewDevices(newIds, deviceDescriptions);
		GD::out.printMessage("Added peer 0x" + BaseLib::HelperFunctions::getHexString(peer->getID()) + ".");

		return PVariable(new Variable((uint32_t)peer->getID()));
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    return Variable::createError(-32500, "Unknown application error.");
}

PVariable IpCamCentral::deleteDevice(BaseLib::PRpcClientInfo clientInfo, std::string serialNumber, int32_t flags)
{
	try
	{
		if(serialNumber.empty()) return Variable::createError(-2, "Unknown device.");

		uint64_t peerId = 0;

		{
			std::shared_ptr<IpCamPeer> peer = getPeer(serialNumber);
			if(!peer) return PVariable(new Variable(VariableType::tVoid));
			peerId = peer->getID();
		}

		return deleteDevice(clientInfo, peerId, flags);
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    return Variable::createError(-32500, "Unknown application error.");
}

PVariable IpCamCentral::deleteDevice(BaseLib::PRpcClientInfo clientInfo, uint64_t peerId, int32_t flags)
{
	try
	{
		if(peerId == 0) return Variable::createError(-2, "Unknown device.");

		{
			std::shared_ptr<IpCamPeer> peer = getPeer(peerId);
			if(!peer) return PVariable(new Variable(VariableType::tVoid));
		}

		deletePeer(peerId);

		if(peerExists(peerId)) return Variable::createError(-1, "Error deleting peer. See log for more details.");

		return PVariable(new Variable(VariableType::tVoid));
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    return Variable::createError(-32500, "Unknown application error.");
}

}
