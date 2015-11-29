#include "Server.h"

Server::Server(unsigned int port)
{
	this->port = port;
	
	CreateUDPSocket();
	CreateTCPSocket();
	Bind(this->_udp_socket);
	Bind(this->_tcp_socket);
	SetSendTimeout(this->_tcp_socket);
	SetReceiveTimeout(this->_udp_socket, GetTimeout(100));
	Listen();

	FD_ZERO(&this->clientsSet);
	FD_ZERO(&this->serverSet);
	FD_SET(this->_tcp_socket, &this->serverSet);
	FD_SET(this->_udp_socket, &this->serverSet);

	cout << "Server started at port: " << this->port << endl;
}

sockaddr_in* Server::CreateAddressInfoForServer()
{
	auto addressInfo = CreateAddressInfo(DEFAULT_IP, this->port);
	addressInfo->sin_addr.s_addr = ADDR_ANY;
	return addressInfo;
}

void Server::Bind(SOCKET socket)
{
	auto serverAddress = CreateAddressInfoForServer();
	auto result = bind(socket, (sockaddr*)serverAddress, sizeof(*serverAddress));
	if (result == SOCKET_ERROR)
	{
		throw runtime_error(EX_BIND_ERROR);
	}
}

SOCKET Server::Accept()
{
	auto client = accept(this->_tcp_socket, NULL, NULL);
	if (client == INVALID_SOCKET) {
		throw runtime_error(EX_ACCEPT_ERROR);
	}
	return client;
}

fpos_t Server::GetFileSize(fstream *file)
{
	fpos_t currentPosition = file->tellg();
	file->seekg(0, ios::end);
	fpos_t size = file->tellg();
	file->seekg(currentPosition);
	return size;
}

TCPMetadata Server::ExtractMetadata(string metadata)
{
	TCPMetadata metadata_st;
	string value;
	stringstream ss(metadata);
	getline(ss, value, METADATA_DELIM);
	metadata_st.fileName = value;
	getline(ss, value, PATH_DELIM);
	metadata_st.progress = stoll(value);
	return metadata_st;
}

UDPMetadata* Server::ExtractMetadataUDP(char* rawMetadata)
{
	auto metadata = new UDPMetadata();
	auto index = 0;
	while (rawMetadata[index] != METADATA_DELIM) metadata->fileName += rawMetadata[index++];
	metadata->requestFileSize = rawMetadata[++index] == 1;
	auto missedPackagesCount = GetNumber(rawMetadata, ++index);
	metadata->returnAllPackages = missedPackagesCount == REQUEST_ALL_PACKAGES;
	if (metadata->returnAllPackages) return metadata;
	for (auto count = 0; count < missedPackagesCount; count++) {
		index += UDP_NUMBER_SIZE;
		metadata->missedPackages.push_back(GetNumber(rawMetadata, index));
	}
	cout << metadata->missedPackages[0] << endl;
	return metadata;
}

void Server::AddUDPClient()
{
	try
	{		
		auto clientsInfo = new sockaddr();
		auto rawMetadata = ReceiveRawDataFrom(this->_udp_socket, clientsInfo)->data;
		
		auto metadata = ExtractMetadataUDP(rawMetadata);

		if (IsACK(clientsInfo, metadata) || memcmp(rawMetadata, ACK, 3) == 0) return;
						
		metadata->file = new fstream();
		try	{
			OpenFile(metadata->file, metadata->fileName);
		} catch (runtime_error e) {
			SendMessageTo(this->_udp_socket, e.what(), clientsInfo);
			throw;
		}

		if (metadata->requestFileSize)
		{
			auto fileSize = GetFileSize(metadata->file);
			SendMessageTo(this->_udp_socket, to_string(fileSize), clientsInfo);
		}
		
		metadata->file->seekg(metadata->progress);
		metadata->packagesTillDrop = PACKAGES_TILL_DROP;
		metadata->addr = clientsInfo;
		metadata->delay = 100;
		metadata->currentDelay = 10000;

		this->udpClients.push_back(metadata);				
	}
	catch (runtime_error e)
	{
		//file not found
	}
}

void Server::SendFilePartsUDP()
{
	for (auto client = this->udpClients.begin(); client != this->udpClients.end(); ++client)
	{
		auto metadata = *client;
		if (--metadata->currentDelay != 0) continue;
		
		metadata->currentDelay = metadata->delay;
		auto file = metadata->file;
		if (metadata->missedPackages.size() > 0) {
			file->seekg(metadata->missedPackages[0] * UDP_BUFFER_SIZE);
			metadata->missedPackages.erase(metadata->missedPackages.begin());
		}
		else {
			file->seekg(metadata->progress);
			metadata->progress += UDP_BUFFER_SIZE;
		}
		
		auto packageNumber = file->tellg() / UDP_BUFFER_SIZE;
		file->read(buffer, UDP_BUFFER_SIZE);
		auto dataSize = file->gcount();
		//cout << packageNumber << endl;

		AddNumberToDatagram(buffer, dataSize, packageNumber);
		SendRawDataTo(this->_udp_socket, buffer, dataSize + UDP_NUMBER_SIZE, metadata->addr);
		
		if (--metadata->packagesTillDrop <= 0 ||
			(!metadata->returnAllPackages && metadata->missedPackages.size() == 0)) {
			RemoveUDPClient(client);
			cout << "UDP client disconnected." << endl;
			if (client == this->udpClients.end()) break;
		}
		if (file->eof()) {
			RemoveUDPClient(client);
			cout << "UDP sending finished." << endl;
			if (client == this->udpClients.end()) break;
		}
	}
}

void Server::RemoveUDPClient(vector<UDPMetadata*>::iterator& iter)
{
	auto clientInfo = *iter;
	clientInfo->file->close();
	delete clientInfo->file;
	delete clientInfo->addr;
	iter = this->udpClients.erase(iter);
}

bool Server::IsACK(sockaddr* client, UDPMetadata* metadata) {

	auto _client = find_if(this->udpClients.begin(), this->udpClients.end(), [&](UDPMetadata* clientMetadata)
	{
		return memcmp(clientMetadata->addr->sa_data, client->sa_data, 14) == 0;
	});
	if (_client != this->udpClients.end()) {	//ACK
		auto clientMeta = (*_client);
		clientMeta->packagesTillDrop = PACKAGES_TILL_DROP;
		clientMeta->missedPackages = metadata->missedPackages;
		clientMeta->delay = 100;
		clientMeta->currentDelay = clientMeta->delay;
		clientMeta->lastProgress = clientMeta->progress;
		return true;
	}
	return false;
}

void Server::Listen()
{
	if (listen(this->_tcp_socket, SOMAXCONN) < 0) {
		throw runtime_error(EX_LISTEN_ERROR);
	}
}

void Server::OpenFile(fstream *file, string fileName)
{
	file->open(fileName, ios::binary | ios::in);
	if (!file->is_open())
	{
		throw runtime_error(EX_FILE_NOT_FOUND);
	}
}

void Server::RemoveTCPClient(vector<CLIENT_INFO>::iterator& iter)
{
	auto clientInfo = *iter;
	FD_CLR(clientInfo->first, &this->clientsSet);
	clientInfo->second->close();
	shutdown(clientInfo->first, SD_BOTH);
	closesocket(clientInfo->first);
	iter = this->tcpClients.erase(iter);
}

void Server::SendBlock(CLIENT_INFO clientInfo)
{
	auto file = clientInfo->second;
	if (file)
	{
		file->read(buffer, BUFFER_SIZE);
		size_t read = file->gcount();
		size_t realySent = SendRawData(clientInfo->first, buffer, read);
		if (realySent != read)
		{
			fpos_t pos = file->tellg();
			file->seekg(pos - (read - realySent));
		}
	}
	else
	{
		throw runtime_error(EX_SENDING_DONE);
	}
}

void Server::SendFilePartsTCP(fd_set& clients)
{
	for (auto client = this->tcpClients.begin(); client != this->tcpClients.end(); ++client)
	{
		if (FD_ISSET((*client)->first, &clients) > 0)
		{
			try {
				SendBlock(*client);
			}
			catch (runtime_error e) {
				cout << e.what() << endl;
				RemoveTCPClient(client);
				cout << CLIENTS_ONLINE;
				if (client == this->tcpClients.end()) break;
			}
		}
	}
}

void Server::AddTCPClient()
{
	auto client = Accept();
	auto metadata = ExtractMetadata(ReceiveMessage(client));
	auto file = new fstream();
	try {
		OpenFile(file, metadata.fileName);
	}
	catch (runtime_error e) {
		SendMessage(client, e.what());
		throw;
	}
	SendMessage(client, to_string(GetFileSize(file)));
	file->seekg(metadata.progress);

	this->tcpClients.push_back(new pair<SOCKET, fstream*>(client, file));
	FD_SET(client, &this->clientsSet);
}

void Server::Run()
{
	auto nullDelay = new timeval();
	//nullDelay->tv_usec = 1;
	while (true)
	{
		auto clients = this->clientsSet;
		auto servers = this->serverSet;
		auto count = select(FD_SETSIZE, &servers, &clients, NULL, this->udpClients.size() != 0 ? nullDelay : NULL);
		if (this->udpClients.size() != 0) SendFilePartsUDP();
		if (count <= 0) continue;
		if (FD_ISSET(this->_tcp_socket, &servers) > 0)
		{
			count--;
			AddTCPClient();
			//cout << CLIENTS_ONLINE;
		}
		if (FD_ISSET(this->_udp_socket, &servers) > 0)
		{
			count--;
			AddUDPClient();
			//cout << CLIENTS_ONLINE;
		}
		if (count > 0)
		{
			SendFilePartsTCP(clients);
		}
	}
}