#include<sdkddkver.h>
#include<boost/asio.hpp>
#include<boost/bind.hpp>
#include<boost/thread.hpp>
#include<iostream>
#include<deque>
#include "..\ChattingTCPServer\Protocol.h"
using namespace std;

class ChatClient
{
public:
	ChatClient(boost::asio::io_context&);
	~ChatClient();

	bool IsConnecting() { return m_Socket.is_open(); }
	void LoginOK() { m_bIsLogin = true; }
	bool IsLogin() { return m_bIsLogin; }

	void Connect(boost::asio::ip::tcp::endpoint);
	void Close();
	void PostSend(const bool, const int, char*);

private:
	boost::asio::io_context& m_IOService;
	boost::asio::ip::tcp::socket m_Socket;

	std::array<char, 512> m_ReceiveBuffer;
	
	int m_nPacketBufferMark;
	char m_PacketBuffer[MAX_RECEIVE_BUFFER_LEN * 2];

	CRITICAL_SECTION m_lock;
	std::deque< char* > m_SendDataQueue;
	bool m_bIsLogin;

	void PostReceive();
	void handle_connect(const boost::system::error_code&);
	void handle_write(const boost::system::error_code&, size_t);
	void handle_receive(const boost::system::error_code&, size_t);
	void ProcessPacket(const char*);
};