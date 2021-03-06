#pragma once



class Client: public Module
{
public:
	enum Status
	{
		NONE,
		LOGIN_FAIL,
		LOGIN_SUCCESS,
		REGISTER_FAIL,
		REGISTER_SUCCESS,
		REGISTER_GR_FAIL,
		REGISTER_GR_SUCCESS

	};
public:
	Client()=default;
	~Client();
	
	
	void Update(float dt);
	Client::Status GetStatus();
	void SetStatus(Client::Status);
	
	std::string& GetUsername() {
		return username;	}
	std::string& GetPassword() {
		return password;
	}
private:
	std::string username;
	std::string password;
	Status m_Status;
};