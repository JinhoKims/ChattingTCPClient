#include "ChatClient.h"

ChatClient::ChatClient(boost::asio::io_context& io_service) :m_IOService(io_service), m_Socket(io_service)
{
	m_bIsLogin = false;
	InitializeCriticalSectionAndSpinCount(&m_lock, 4000); // m_lock의 임계영역 생성 및 초기화(섹션이름, 스핀카운트) 

	/*
		스핀락은 Busy Waiting을 통하여 스레드를 잠재우지 않고 약간의 자원을 소모하는 대신 context switching을 방지하여 gain을 얻을 수 있다.
		Busy Wating : 현재 CPU를 사용하는 스레드가 무한 루프를 돌면서 다른 쓰레드에게 CPU를 양보하지 않는 것이다.
		하지만 임계영역에 있는 스레드가 Lock을 오래 사용한다면 Busy Wating은 그 만큼 CPU 자원을 소모하고 있으니 Sleeping보다 자원을 더 낭비하게 된다.
	*/
}

ChatClient::~ChatClient()
{
	EnterCriticalSection(&m_lock); // 임계영역 진입

	while (m_SendDataQueue.empty() == false)
	{
		delete[] m_SendDataQueue.front(); // 큐가 모두 빌때까지 동적 할당 해제
		m_SendDataQueue.pop_front();
	}

	LeaveCriticalSection(&m_lock); // 임계영역 탈출 
	DeleteCriticalSection(&m_lock); // 및 해제
}

void ChatClient::Connect(boost::asio::ip::tcp::endpoint endpoint)
{
	m_nPacketBufferMark = 0;

	m_Socket.async_connect(endpoint, boost::bind(&ChatClient::handle_connect, this, boost::asio::placeholders::error)); // 비동기로 소켓(endpoint) 접속
}

void ChatClient::Close()
{
	if (m_Socket.is_open())
	{
		m_Socket.close();
	}
}

void ChatClient::PostSend(const bool bImmediately, const int nSize, char* pData) // 데이터 전송 함수 (바로 전송 가능한지, 크기, 내용)
{
	char* pSendData = nullptr;

	EnterCriticalSection(&m_lock); // 임계영역 진입

	if (bImmediately == false) // 데이터를 바로 전송 불가능할 경우
	{
		pSendData = new char[nSize];
		memcpy(pSendData, pData, nSize); // 데이터 복사

		m_SendDataQueue.push_back(pSendData); // 데이터를 큐에 삽입
	}
	else // 바로 전송 가능할 경우
	{
		pSendData = pData; // 데이터 복사
	}

	if (bImmediately || m_SendDataQueue.size() < 2) // 큐가 적을 경우
	{
		boost::asio::async_write(m_Socket, boost::asio::buffer(pSendData, nSize), boost::bind(&ChatClient::handle_write, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
		// 메시지를 서버로 전송
	}

	LeaveCriticalSection(&m_lock); // 임계영역 탈퇴
}

void ChatClient::PostReceive()
{
	memset(&m_ReceiveBuffer, '\0', sizeof(m_ReceiveBuffer)); // 버퍼 초기화
	m_Socket.async_read_some(boost::asio::buffer(m_ReceiveBuffer), boost::bind(&ChatClient::handle_receive, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
	// 비동기로 데이터 수신
}

void ChatClient::handle_connect(const boost::system::error_code& error)
{
	if (!error)
	{
		cout << "서버 접속 성공" << endl << "이름을 입력하세요!! " << endl;
		PostReceive(); // 접속 성공 시
	}
	else
	{
		cout << "서버 접속 실패. error No : " << error.value() << " error Message : " << error.message() << endl;
	}
}

void ChatClient::handle_write(const boost::system::error_code& error, size_t bytes_transferred) // 데이터 전송 후 작업
{
	EnterCriticalSection(&m_lock); // 임계영역 진입

	delete[] m_SendDataQueue.front(); // 동적 할당 해제
	m_SendDataQueue.pop_front(); // front 부분을  pop

	char* pData = nullptr;

	if (m_SendDataQueue.empty() == false)
	{
		pData = m_SendDataQueue.front(); // pData는 최근 큐
	}

	LeaveCriticalSection(&m_lock); // 임계영역 탈퇴

	if (pData != nullptr)
	{
		PACKET_HEADER* pHeader = (PACKET_HEADER*)pData;
		PostSend(true, pHeader->nSize, pData); // 메시지 전송
	}
}

void ChatClient::handle_receive(const boost::system::error_code& error, size_t bytes_transferred)
{
	if (error) // 연결 실패 시
	{
		if (error == boost::asio::error::eof)
		{
			cout << "클라이언트와 연결이 끊어졌습니다." << endl;
		}
		else
		{
			cout << "error No : " << error.value() << " error Message : " << error.message() << endl;
		}
		Close();
	}
	else // 연결 성공 시
	{
		memcpy(&m_PacketBuffer[m_nPacketBufferMark], m_ReceiveBuffer.data(), bytes_transferred);
		int nPacketData = m_nPacketBufferMark + bytes_transferred;
		int nReadData = 0;

		while (nPacketData > 0) // 패킷 처리
		{
			if (nPacketData < sizeof(PACKET_HEADER))
			{
				break;
			}

			PACKET_HEADER* pHeader = (PACKET_HEADER*)&m_PacketBuffer[nReadData];

			if (pHeader->nSize <= nPacketData)
			{
				ProcessPacket(&m_PacketBuffer[nReadData]); // 패킷 처리 함수 호출

				nPacketData -= pHeader->nSize;
				nReadData += pHeader->nSize;
			}
			else
			{
				break;
			}
		}

		if (nPacketData > 0) // 남은 데이터 패킷 처리
		{
			char TempBuffer[MAX_RECEIVE_BUFFER_LEN] = { 0, };
			memcpy(&TempBuffer[0], &m_PacketBuffer[nReadData], nPacketData);
			memcpy(&m_PacketBuffer[0], &TempBuffer[0], nPacketData);
		}

		m_nPacketBufferMark = nPacketData;

		// 데이터 수신 요청
		PostReceive();
	}
	
}

void ChatClient::ProcessPacket(const char* pData) // 패킷 처리
{
	PACKET_HEADER* pheader = (PACKET_HEADER*)pData;

	switch (pheader->nID)
	{
	case RES_IN: // 로그인 성공 출력
	{
		PKT_RES_IN* pPacket = (PKT_RES_IN*)pData;

		LoginOK();

		std::cout << "클라이언트 로그인 성공 ?: " << pPacket->bIsSuccess << std::endl;
	}
	break;
	case NOTICE_CHAT: // 메시지 출력
	{
		PKT_NOTICE_CHAT* pPacket = (PKT_NOTICE_CHAT*)pData;

		std::cout << pPacket->szName << ": " << pPacket->szMessage << std::endl;
	}
	break;
	}
}
