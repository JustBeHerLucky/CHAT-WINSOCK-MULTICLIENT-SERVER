// Server.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "Server.h"



Server::Server(Log& l):Logger(l)
{
	LoadUser();
}


Server::~Server()
{
	WriteUser();
	
	m_SVSocket.Close();

	Socket::ShutdownWinSock();
}


bool Server::Init()
{
	

	
	return m_SVSocket.Connect();
}

void Server::Update(float dt)
{
		
}

void Server::DisconnectClient(SOCKET client)
{
	int ID = FindClient(client);
	if (ID == -1)
	{
		// shutdown the connection since we're done
		int iResult = shutdown(client, SD_SEND);

		if (iResult == SOCKET_ERROR) {
			printf("shutdown failed with error: %d\n", WSAGetLastError());
		}

		closesocket(client);
		for (auto it = m_Connecting.begin(); it!=m_Connecting.end(); it++)
		{
			if (it->Get() == client)
			{
				m_Connecting.erase(it);
				break;
			}
		}
		Logger.AddLog("[Info] Client %d has been disconnect.", client);
		
	}
	else
	{
		Client* cl = m_Clients[ID].get();
		cl->SetStatus(Client::OFFLINE);
	}
}

void Server::ProcessEvent(WPARAM wParam, LPARAM lParam)
{
	static int iResult;
	static char recvbuf[MAX_BUFFER_LEN];
	static int recvbuflen = MAX_BUFFER_LEN;
	if (WSAGETSELECTERROR(lParam))
	{
		//printf("Server::ProcessEvent() Accept failed with error: %d\n", WSAGetLastError());
		Logger.AddLog("[Error] Server::ProcessEvent() Accept failed with error: %d\n", WSAGetLastError());
		closesocket(wParam);
		return;
	}
	switch (WSAGETSELECTEVENT(lParam))
	{
	case FD_ACCEPT:
	{
		// Accept a client socket
		SOCKET ClientSocket = accept(wParam, NULL, NULL);
		if (ClientSocket == INVALID_SOCKET) {
			Logger.AddLog("[Error] Accept failed with error: %d\n", WSAGetLastError());
		}
		else
		{
			Logger.AddLog("[Info] Accept connecting with socket: %d\n", wParam);
			m_Connecting.push_back(Socket(ClientSocket));
		}
		break;
	}
	case FD_READ:
	{
		iResult = recv(wParam, recvbuf, recvbuflen, 0);
		if (iResult > 0) {
			ProcessCommand(recvbuf, iResult, wParam);
		
			Logger.AddLog("[Info] Reive from %d. Num byte: %d\n", wParam, iResult);
		}
		else if (iResult == 0)
		{
			Logger.AddLog("[Info] Client %d disconnect.\n",wParam);
			//DisconnectClient(m_Clients[i]);
		}
		else {
			Logger.AddLog("[Error] Recv failed with error: %d\n", WSAGetLastError());
			//closesocket(m_Clients[i]);
			//WSACleanup();
		}
		break;
	}
	case FD_CLOSE:
	{
		DisconnectClient(wParam);
		break;
	}
	}
}

void Server::ProcessCommand(char * cmd, int len,SOCKET sk)
{
	

	Buffer bf(cmd, len);

	char* a = bf.ReadChar();
	bf.IncPos(1);
	Command type = (Command)(*a);

	switch (type)
	{
	case CMD_REG_USER:
	{
		char username[MAX_USERNAME_LEN]; int unlen;
		char password[MAX_USERNAME_LEN]; int pwlen;

		// Read username and pass word from buffer
		unlen = bf.ReadInt();
		memcpy(username, bf.ReadChar(), unlen);
		bf.IncPos(unlen);
		pwlen = bf.ReadInt();
		memcpy(password, bf.ReadChar(), pwlen);
		bf.IncPos(pwlen);

		if (!CheckUser(username))
		{
			m_Clients.push_back(std::make_unique<Client>(sk, username, password));

			Buffer stbf(MAX_STATUS_LEN);
			stbf.WriteChar(CMD_STATUS);
			stbf.WriteChar(REG_USER);
			stbf.WriteChar(1);
			stbf.WriteInt(24);
			stbf.WriteChar("Register user success!", 24);

			SendClient(sk, stbf.Get(),stbf.GetPos());


			Logger.AddLog("[Info] Register success: %s %s\n", username, password);

		}
		else
		{
			Buffer stbf(MAX_STATUS_LEN);
			stbf.WriteChar(CMD_STATUS);
			stbf.WriteChar(REG_USER);
			stbf.WriteChar(0);
			stbf.WriteInt(38);
			stbf.WriteChar("Register failer. Username has exits!", 38);

			SendClient(sk, stbf.Get(), stbf.GetPos());
			Logger.AddLog("[Info] %d Register failed.\n", sk);
		}
		break;
	}
	case CMD_LOGIN_USER:
	{
		char username[MAX_USERNAME_LEN]; int unlen;
		char password[MAX_USERNAME_LEN]; int pwlen;

		// Read username and pass word from buffer
		unlen = bf.ReadInt();
		memcpy(username, bf.ReadChar(), unlen);
		bf.IncPos(unlen);
		pwlen = bf.ReadInt();
		memcpy(password, bf.ReadChar(), pwlen);
		bf.IncPos(pwlen);

		int id = FindClient(username, password);
		if (id == -1)
		{
			Buffer stbf(MAX_STATUS_LEN);
			stbf.WriteChar(CMD_STATUS);
			stbf.WriteChar(LOGIN_USER);
			stbf.WriteChar(0);
			stbf.WriteInt(18);
			stbf.WriteChar("Login user fail!", 18);

			SendClient(sk, stbf.Get(), stbf.GetPos());
		}
		else
		{
			Buffer stbf(MAX_STATUS_LEN);
			stbf.WriteChar(CMD_STATUS);
			stbf.WriteChar(LOGIN_USER);
			stbf.WriteChar(1);
			stbf.WriteInt(21);
			stbf.WriteChar("Login user success!", 21);
			SendClient(sk, stbf.Get(), stbf.GetPos());

			m_Clients[id]->SetStatus(Client::ONLINE);
			m_Clients[id]->SetSocket(sk);
			Logger.AddLog("[Info] User %s login success with socket %d.\n", username, sk);
		}
		break;
	}
	case CMD_REG_GROUP:
	{
		char grname[MAX_USERNAME_LEN]; int grlen;
		char username[MAX_USERNAME_LEN]; int unlen;

		grlen = bf.ReadInt();
		memcpy(grname, bf.ReadChar(), grlen);
		bf.IncPos(grlen);

		unlen = bf.ReadInt();
		memcpy(username, bf.ReadChar(), unlen);
		bf.IncPos(unlen);

		int s = 0;
		if (FindGroup(grname) == -1)
		{
			int IDClient = FindClient(username);
			if (IDClient != -1)
			{
				m_Groups.push_back(std::make_unique<GroupChatSV>(grname, m_Clients[IDClient].get()));
				s = 1;
			}
		}

		// send status success
		Buffer stbf(MAX_BUFFER_LEN);
		stbf.WriteChar(CMD_STATUS);
		stbf.WriteChar(REG_GROUP);
		stbf.WriteChar((char)s);
		if (s)
		{
			stbf.WriteInt(grlen);
			stbf.WriteChar(grname, grlen);
			Logger.AddLog("[Info] %s Register group success.", grname);
		}
		
		SendClient(sk, stbf.Get(), stbf.GetPos());

		break;

		
	}
	case CMD_SEND_USER:
	{
		Client* cl = m_Clients[FindClient(sk)].get();
		char username[MAX_USERNAME_LEN]; int unlen;
		
		// Read username 
		unlen = bf.ReadInt();
		memcpy(username, bf.ReadChar(), unlen);
		bf.IncPos(unlen);

		int TargetID = FindClient(username);
		if (TargetID == -1)
		{
			Buffer stbf(MAX_STATUS_LEN);
			stbf.WriteChar(CMD_STATUS);
			stbf.WriteChar(SEND_TEXT);
			stbf.WriteChar(0);
			stbf.WriteInt(23);
			stbf.WriteChar("Can't found username.", 23);
			SendClient(cl, stbf.Get(), stbf.GetPos());
			Logger.AddLog("[Error] Socket: %d Can't found username: %s", sk, username);
		}
		else
		{
			Client* Target = m_Clients[TargetID].get();
			if (Target->GetStatus() == Client::OFFLINE)
			{
				Buffer stbf(MAX_STATUS_LEN);
				stbf.WriteChar(CMD_STATUS);
				stbf.WriteChar(SEND_TEXT);
				stbf.WriteChar(0);
				stbf.WriteInt(18);
				stbf.WriteChar("User not online.", 18);
				SendClient(cl, stbf.Get(), stbf.GetPos());
				Logger.AddLog("[Error] Socket: %d User not online.", sk);
			}
			else if(Target->GetStatus() == Client::ONLINE)
			{
				char messenger[MAX_BUFFER_LEN]; int textlen;
				textlen = bf.ReadInt();
				memcpy(messenger, bf.ReadChar(), textlen);

				Buffer stbf(MAX_BUFFER_LEN);
				stbf.WriteChar(CMD_SEND_USER);
				stbf.WriteInt(cl->GetUsername().size()+1);
				stbf.WriteChar(cl->GetUsername().c_str(), cl->GetUsername().size() + 1);
				stbf.WriteInt(textlen);
				stbf.WriteChar(messenger, textlen);
				SendClient(Target, stbf.Get(), stbf.GetPos());
				Logger.AddLog("[Info] Socket: %d messenger %s send.", sk, messenger);
			}
		}
		break;
	}
	case CMD_SEND_GROUP:
	{
		char grname[MAX_USERNAME_LEN]; int grlen;
		char msg[MAX_BUFFER_LEN]; int msglen;
		char username[MAX_USERNAME_LEN]; int unlen;

		grlen = bf.ReadInt();
		memcpy(grname, bf.ReadChar(), grlen);
		bf.IncPos(grlen);

		unlen = bf.ReadInt();
		memcpy(username, bf.ReadChar(), unlen);
		bf.IncPos(unlen);

		msglen = bf.ReadInt();
		memcpy(msg, bf.ReadChar(), msglen);
		bf.IncPos(msglen);

		int ID = FindGroup(grname);
		if (ID != -1)
		{
			m_Groups[ID]->SendMsg(username,msg);
			Logger.AddLog("[Info] Msg form %s to %s\n", username, grname);
		}
		break;
	}
	case CMD_SEND_FILES:
		break;
	case CMD_ADD_USER:
	{
		char grname[MAX_USERNAME_LEN]; int grlen;
		char username[MAX_BUFFER_LEN]; int unlen;

		unlen = bf.ReadInt();
		memcpy(username, bf.ReadChar(), unlen);
		bf.IncPos(unlen);

		grlen = bf.ReadInt();
		memcpy(grname, bf.ReadChar(), grlen);
		bf.IncPos(grlen);

		int IDClient = FindClient(username);

		int s = 0;
		if (IDClient != -1)
		{
			int IDGroup = FindGroup(grname);
			if (IDGroup != -1)
			{
				m_Groups[IDGroup]->AddUser(m_Clients[IDClient].get());
				s = 1;
			}
		}

		Buffer stbf(MAX_BUFFER_LEN);
		stbf.WriteChar(CMD_STATUS);
		stbf.WriteChar(ADD_USER);
		stbf.WriteChar((char)s);
		stbf.WriteInt(grlen);
		stbf.WriteChar(grname, grlen);

		SendClient(sk, stbf.Get(), stbf.GetPos());

		if (s) Logger.AddLog("[Info] Add user %s to group %s\n", username, grname);
		break;
	}

	case CMD_GET_GRINFO:
	{
		char grname[MAX_USERNAME_LEN]; int grlen;
		char username[MAX_USERNAME_LEN]; int unlen;

		grlen = bf.ReadInt();
		memcpy(grname, bf.ReadChar(), grlen);
		bf.IncPos(grlen);

		unlen = bf.ReadInt();
		memcpy(username, bf.ReadChar(), unlen);
		bf.IncPos(unlen);

		int IDGroup = FindGroup(grname);
		int IDClient = FindClient(username);
		int num = 0;
		if (IDGroup != -1 && IDClient !=-1)
		{
			num = m_Groups[IDGroup]->GetUserInfo().size();
		}

		Buffer rqbf(MAX_BUFFER_LEN);
		rqbf.WriteChar(CMD_GET_GRINFO);
		rqbf.WriteInt(grlen);
		rqbf.WriteChar(grname, grlen);

		rqbf.WriteInt(num);
		for (int i = 0; i < num; i++)
		{
			const UserInfo& ui = m_Groups[IDGroup]->GetUserInfo()[i];
			if (ui.Name == username) continue;
			rqbf.WriteInt(ui.Name.size()+1);
			rqbf.WriteChar(ui.Name.c_str(), ui.Name.size() + 1);
		}

		SendClient(sk, rqbf.Get(), rqbf.GetPos());
		
		break;
	}
	default:
		Logger.AddLog("[Error] Unknow command.\n ");
		break;
	}
}

void Server::SetAsynch(HWND hwnd)
{
	WSAAsyncSelect(m_SVSocket.Get(), hwnd, WM_SOCKET, FD_ACCEPT | FD_READ | FD_CLOSE);
}

void Server::LoadUser()
{
	std::ifstream fp("Data/data.bin");
	if (!fp)
	{
		std::cout << "Error: Can't load user data.\n";
		return;
	}
	std::string un, pw;
	while (fp >> un >> pw)
	{
		m_Clients.push_back(std::make_unique<Client>(INVALID_SOCKET, un.c_str(), pw.c_str()));
	}

	fp.close();
}

void Server::WriteUser()
{
	std::ofstream fp("Data/data.bin");
	if (!fp)
	{
		std::cout << "Error: Can't open user data.\n";
		return;
	}

	for(size_t i=0; i<m_Clients.size(); i++)
	{
		
		fp << m_Clients[i]->GetUsername() <<" "<< m_Clients[i]->GetPassWord() << std::endl;

	}

	fp.close();
}

bool Server::CheckUser(const char * username)
{
	for (size_t i = 0; i < m_Clients.size(); i++)
	{
		if (m_Clients[i]->GetUsername() == username) return true;
	}

	return false;
}

int Server::FindClient(SOCKET sk)
{
	for (size_t i = 0; i < m_Clients.size(); i++)
	{
		if (m_Clients[i]->GetSocket().Get() == sk) return i;
	}

	return -1;
}

int Server::FindClient(const char * username, const char* password)
{
	for (size_t i = 0; i < m_Clients.size(); i++)
	{
		if (m_Clients[i]->GetUsername() == username)
		{
			if (password)
			{
				if (m_Clients[i]->GetPassWord() == password) return i;
				else return -1;
			}else return i;
		}
	}

	return -1;
}

int Server::FindGroup(const char * name)
{
	for (size_t i = 0; i < m_Groups.size(); i++)
	{
		if (m_Groups[i]->GetName() == name) return i;
	}
	return -1;
}

bool Server::SendClient(Client*cl, const char * buffer, int len)
{
	return SendClient(cl->GetSocket().Get(), buffer,len);
}

bool Server::SendClient(SOCKET sk, const char * buffer, int len)
{
	int iResult = send(sk, buffer, len, 0);

	if (iResult == SOCKET_ERROR) {
		Logger.AddLog("[Error] Send failed with error: %d\n", WSAGetLastError());
		return 0;
	}

	return 1;
}

