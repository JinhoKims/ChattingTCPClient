#include"ChatClient.h"
int main()
{
	boost::asio::io_context io_service;

	auto endpoint = boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), PORT_NUMBER);

	ChatClient Cliet(io_service);
	Cliet.Connect(endpoint);

	boost::thread thread(boost::bind(&boost::asio::io_context::run, &io_service)); // 워커 스레드를 생성 후 run() 호출

	char szInputMessage[MAX_MESSAGE_LEN * 2] = { 0, };

	while (std::cin.getline(szInputMessage, MAX_MESSAGE_LEN)) // 커맨드 줄에 입력 대기
	{
		if (strnlen_s(szInputMessage, MAX_MESSAGE_LEN) == 0)
		{
			break;
		}

		if (Cliet.IsConnecting() == false)
		{
			std::cout << "서버와 연결되지 않았습니다" << std::endl;
			continue;
		}

		if (Cliet.IsLogin() == false) // 로그인이 안되었을 경우
		{
			PKT_REQ_IN SendPkt;
			SendPkt.Init();
			strncpy_s(SendPkt.szName, MAX_NAME_LEN, szInputMessage, MAX_NAME_LEN - 1);

			Cliet.PostSend(false, SendPkt.nSize, (char*)&SendPkt); // 로그인 메시지 전송
		}
		else // 로그인 중일시 
		{
			PKT_REQ_CHAT SendPkt;
			SendPkt.Init();
			strncpy_s(SendPkt.szMessage, MAX_MESSAGE_LEN, szInputMessage, MAX_MESSAGE_LEN - 1);

			Cliet.PostSend(false, SendPkt.nSize, (char*)&SendPkt); // 메시지 전송
		}
	}


	io_service.stop(); // io_service 중지
	Cliet.Close(); // 소켓을 끊고
	thread.join(); // 스레드(io_service)가 종료될 때까지 기다린다.

	std::cout << "클라이언트를 종료해 주세요" << std::endl;

	return 0;
}