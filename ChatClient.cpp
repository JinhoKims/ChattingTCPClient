#include "ChatClient.h"

ChatClient::ChatClient(boost::asio::io_context& io_service) :m_IOService(io_service), m_Socket(io_service)
{
	m_bIsLogin = false;
	InitializeCriticalSectionAndSpinCount(&m_lock, 4000); // m_lock�� �Ӱ迵�� ���� �� �ʱ�ȭ(�����̸�, ����ī��Ʈ) 

	/*
		���ɶ��� Busy Waiting�� ���Ͽ� �����带 ������� �ʰ� �ణ�� �ڿ��� �Ҹ��ϴ� ��� context switching�� �����Ͽ� gain�� ���� �� �ִ�.
		Busy Wating : ���� CPU�� ����ϴ� �����尡 ���� ������ ���鼭 �ٸ� �����忡�� CPU�� �纸���� �ʴ� ���̴�.
		������ �Ӱ迵���� �ִ� �����尡 Lock�� ���� ����Ѵٸ� Busy Wating�� �� ��ŭ CPU �ڿ��� �Ҹ��ϰ� ������ Sleeping���� �ڿ��� �� �����ϰ� �ȴ�.
	*/
}

ChatClient::~ChatClient()
{
	EnterCriticalSection(&m_lock); // �Ӱ迵�� ����

	while (m_SendDataQueue.empty() == false)
	{
		delete[] m_SendDataQueue.front(); // ť�� ��� �������� ���� �Ҵ� ����
		m_SendDataQueue.pop_front();
	}

	LeaveCriticalSection(&m_lock); // �Ӱ迵�� Ż�� 
	DeleteCriticalSection(&m_lock); // �� ����
}

void ChatClient::Connect(boost::asio::ip::tcp::endpoint endpoint)
{
	m_nPacketBufferMark = 0;

	m_Socket.async_connect(endpoint, boost::bind(&ChatClient::handle_connect, this, boost::asio::placeholders::error)); // �񵿱�� ����(endpoint) ����
}

void ChatClient::Close()
{
	if (m_Socket.is_open())
	{
		m_Socket.close();
	}
}

void ChatClient::PostSend(const bool bImmediately, const int nSize, char* pData) // ������ ���� �Լ� (�ٷ� ���� ��������, ũ��, ����)
{
	char* pSendData = nullptr;

	EnterCriticalSection(&m_lock); // �Ӱ迵�� ����

	if (bImmediately == false) // �����͸� �ٷ� ���� �Ұ����� ���
	{
		pSendData = new char[nSize];
		memcpy(pSendData, pData, nSize); // ������ ����

		m_SendDataQueue.push_back(pSendData); // �����͸� ť�� ����
	}
	else // �ٷ� ���� ������ ���
	{
		pSendData = pData; // ������ ����
	}

	if (bImmediately || m_SendDataQueue.size() < 2) // ť�� ���� ���
	{
		boost::asio::async_write(m_Socket, boost::asio::buffer(pSendData, nSize), boost::bind(&ChatClient::handle_write, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
		// �޽����� ������ ����
	}

	LeaveCriticalSection(&m_lock); // �Ӱ迵�� Ż��
}

void ChatClient::PostReceive()
{
	memset(&m_ReceiveBuffer, '\0', sizeof(m_ReceiveBuffer)); // ���� �ʱ�ȭ
	m_Socket.async_read_some(boost::asio::buffer(m_ReceiveBuffer), boost::bind(&ChatClient::handle_receive, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
	// �񵿱�� ������ ����
}

void ChatClient::handle_connect(const boost::system::error_code& error)
{
	if (!error)
	{
		cout << "���� ���� ����" << endl << "�̸��� �Է��ϼ���!! " << endl;
		PostReceive(); // ���� ���� ��
	}
	else
	{
		cout << "���� ���� ����. error No : " << error.value() << " error Message : " << error.message() << endl;
	}
}

void ChatClient::handle_write(const boost::system::error_code& error, size_t bytes_transferred) // ������ ���� �� �۾�
{
	EnterCriticalSection(&m_lock); // �Ӱ迵�� ����

	delete[] m_SendDataQueue.front(); // ���� �Ҵ� ����
	m_SendDataQueue.pop_front(); // front �κ���  pop

	char* pData = nullptr;

	if (m_SendDataQueue.empty() == false)
	{
		pData = m_SendDataQueue.front(); // pData�� �ֱ� ť
	}

	LeaveCriticalSection(&m_lock); // �Ӱ迵�� Ż��

	if (pData != nullptr)
	{
		PACKET_HEADER* pHeader = (PACKET_HEADER*)pData;
		PostSend(true, pHeader->nSize, pData); // �޽��� ����
	}
}

void ChatClient::handle_receive(const boost::system::error_code& error, size_t bytes_transferred)
{
	if (error) // ���� ���� ��
	{
		if (error == boost::asio::error::eof)
		{
			cout << "Ŭ���̾�Ʈ�� ������ ���������ϴ�." << endl;
		}
		else
		{
			cout << "error No : " << error.value() << " error Message : " << error.message() << endl;
		}
		Close();
	}
	else // ���� ���� ��
	{
		memcpy(&m_PacketBuffer[m_nPacketBufferMark], m_ReceiveBuffer.data(), bytes_transferred);
		int nPacketData = m_nPacketBufferMark + bytes_transferred;
		int nReadData = 0;

		while (nPacketData > 0) // ��Ŷ ó��
		{
			if (nPacketData < sizeof(PACKET_HEADER))
			{
				break;
			}

			PACKET_HEADER* pHeader = (PACKET_HEADER*)&m_PacketBuffer[nReadData];

			if (pHeader->nSize <= nPacketData)
			{
				ProcessPacket(&m_PacketBuffer[nReadData]); // ��Ŷ ó�� �Լ� ȣ��

				nPacketData -= pHeader->nSize;
				nReadData += pHeader->nSize;
			}
			else
			{
				break;
			}
		}

		if (nPacketData > 0) // ���� ������ ��Ŷ ó��
		{
			char TempBuffer[MAX_RECEIVE_BUFFER_LEN] = { 0, };
			memcpy(&TempBuffer[0], &m_PacketBuffer[nReadData], nPacketData);
			memcpy(&m_PacketBuffer[0], &TempBuffer[0], nPacketData);
		}

		m_nPacketBufferMark = nPacketData;

		// ������ ���� ��û
		PostReceive();
	}
	
}

void ChatClient::ProcessPacket(const char* pData) // ��Ŷ ó��
{
	PACKET_HEADER* pheader = (PACKET_HEADER*)pData;

	switch (pheader->nID)
	{
	case RES_IN: // �α��� ���� ���
	{
		PKT_RES_IN* pPacket = (PKT_RES_IN*)pData;

		LoginOK();

		std::cout << "Ŭ���̾�Ʈ �α��� ���� ?: " << pPacket->bIsSuccess << std::endl;
	}
	break;
	case NOTICE_CHAT: // �޽��� ���
	{
		PKT_NOTICE_CHAT* pPacket = (PKT_NOTICE_CHAT*)pData;

		std::cout << pPacket->szName << ": " << pPacket->szMessage << std::endl;
	}
	break;
	}
}
