#include"ChatClient.h"
int main()
{
	boost::asio::io_context io_service;

	auto endpoint = boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), PORT_NUMBER);

	ChatClient Cliet(io_service);
	Cliet.Connect(endpoint);

	boost::thread thread(boost::bind(&boost::asio::io_context::run, &io_service)); // ��Ŀ �����带 ���� �� run() ȣ��

	char szInputMessage[MAX_MESSAGE_LEN * 2] = { 0, };

	while (std::cin.getline(szInputMessage, MAX_MESSAGE_LEN)) // Ŀ�ǵ� �ٿ� �Է� ���
	{
		if (strnlen_s(szInputMessage, MAX_MESSAGE_LEN) == 0)
		{
			break;
		}

		if (Cliet.IsConnecting() == false)
		{
			std::cout << "������ ������� �ʾҽ��ϴ�" << std::endl;
			continue;
		}

		if (Cliet.IsLogin() == false) // �α����� �ȵǾ��� ���
		{
			PKT_REQ_IN SendPkt;
			SendPkt.Init();
			strncpy_s(SendPkt.szName, MAX_NAME_LEN, szInputMessage, MAX_NAME_LEN - 1);

			Cliet.PostSend(false, SendPkt.nSize, (char*)&SendPkt); // �α��� �޽��� ����
		}
		else // �α��� ���Ͻ� 
		{
			PKT_REQ_CHAT SendPkt;
			SendPkt.Init();
			strncpy_s(SendPkt.szMessage, MAX_MESSAGE_LEN, szInputMessage, MAX_MESSAGE_LEN - 1);

			Cliet.PostSend(false, SendPkt.nSize, (char*)&SendPkt); // �޽��� ����
		}
	}


	io_service.stop(); // io_service ����
	Cliet.Close(); // ������ ����
	thread.join(); // ������(io_service)�� ����� ������ ��ٸ���.

	std::cout << "Ŭ���̾�Ʈ�� ������ �ּ���" << std::endl;

	return 0;
}