/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SHARED_NETWORK_H
#define ENGINE_SHARED_NETWORK_H

#include "ringbuffer.h"
#include "huffman.h"

/*

CURRENT:
	packet header: 7 bytes (9 bytes for connless)
		unsigned char flags_ack;    // 6bit flags, 2bit ack
		unsigned char ack;          // 8bit ack
		unsigned char numchunks;    // 8bit chunks
		unsigned char token[4];     // 32bit token
		// ffffffaa
		// aaaaaaaa
		// NNNNNNNN
		// TTTTTTTT
		// TTTTTTTT
		// TTTTTTTT
		// TTTTTTTT

	packet header (CONNLESS):
		unsigned char flag_version;				// 6bit flags, 2bits version
		unsigned char token[4];					// 32bit token
		unsigned char responsetoken[4];			// 32bit response token

		// ffffffvv
		// TTTTTTTT
		// TTTTTTTT
		// TTTTTTTT
		// TTTTTTTT
		// RRRRRRRR
		// RRRRRRRR
		// RRRRRRRR
		// RRRRRRRR

	if the token isn't explicitely set by any means, it must be set to
	0xffffffff

	chunk header: 2-3 bytes
		unsigned char flags_size; // 2bit flags, 6 bit size
		unsigned char size_seq; // 6bit size, 2bit seq
		(unsigned char seq;) // 8bit seq, if vital flag is set
*/

enum
{
	NETFLAG_ALLOWSTATELESS=1,
	NETSENDFLAG_VITAL=1,
	NETSENDFLAG_CONNLESS=2,
	NETSENDFLAG_FLUSH=4,

	NETSTATE_OFFLINE=0,
	NETSTATE_CONNECTING,
	NETSTATE_ONLINE,

	NETBANTYPE_SOFT=1,
	NETBANTYPE_DROP=2,

	NETCREATE_FLAG_RANDOMPORT=1,
};


enum
{
	NET_MAX_CHUNKHEADERSIZE = 3,
	
	// packets
	NET_PACKETHEADERSIZE = 7,
	NET_PACKETHEADERSIZE_CONNLESS = NET_PACKETHEADERSIZE + 2,
	NET_MAX_PACKETHEADERSIZE = NET_PACKETHEADERSIZE_CONNLESS,

	NET_MAX_PACKETSIZE = 1400,
	NET_MAX_PAYLOAD = NET_MAX_PACKETSIZE-NET_MAX_PACKETHEADERSIZE,

	NET_PACKETVERSION=1,

	NET_PACKETFLAG_CONTROL=1,
	NET_PACKETFLAG_RESEND=2,
	NET_PACKETFLAG_COMPRESSION=4,
	NET_PACKETFLAG_CONNLESS=8,

	NET_MAX_PACKET_CHUNKS=256,

	// token
	NET_SEEDTIME = 16,

	NET_TOKENCACHE_SIZE = 64,
	NET_TOKENCACHE_ADDRESSEXPIRY = NET_SEEDTIME,
	NET_TOKENCACHE_PACKETEXPIRY = 5,
};
enum
{
	NET_TOKEN_MAX = 0xffffffff,
	NET_TOKEN_NONE = NET_TOKEN_MAX,
	NET_TOKEN_MASK = NET_TOKEN_MAX,
};
enum
{
	NET_TOKENFLAG_ALLOWBROADCAST = 1,
	NET_TOKENFLAG_RESPONSEONLY = 2,

	NET_TOKENREQUEST_DATASIZE = 512,

	//
	NET_MAX_CLIENTS = 128,
	NET_MAX_CONSOLE_CLIENTS = 4,
	
	NET_MAX_SEQUENCE = 1<<10,
	NET_SEQUENCE_MASK = NET_MAX_SEQUENCE-1,

	NET_CONNSTATE_OFFLINE=0,
	NET_CONNSTATE_TOKEN=1,
	NET_CONNSTATE_CONNECT=2,
	NET_CONNSTATE_PENDING=3,
	NET_CONNSTATE_ONLINE=4,
	NET_CONNSTATE_ERROR=5,

	// F-DDrace
	NET_CONNSTATE_DUMMY = 6,

	NET_CHUNKFLAG_VITAL=1,
	NET_CHUNKFLAG_RESEND=2,

	NET_CTRLMSG_KEEPALIVE=0,
	NET_CTRLMSG_CONNECT=1,
	NET_CTRLMSG_ACCEPT=2,
	NET_CTRLMSG_CLOSE=4,
	NET_CTRLMSG_TOKEN=5,

	// F-DDrace
	NET_CONNLIMIT_IPS=16,

	NET_CONN_BUFFERSIZE=1024*128, // F-DDrace changed this, default is 1024*32

	NET_ENUM_TERMINATOR
};

typedef int SECURITY_TOKEN;

static const unsigned char SECURITY_TOKEN_MAGIC[] = {'T', 'K', 'E', 'N'};

enum
{
	NET_SECURITY_TOKEN_UNKNOWN = -1,
	NET_SECURITY_TOKEN_UNSUPPORTED = 0,
};

enum
{
	SOCKET_MAIN,
	SOCKET_TWO,
	NUM_SOCKETS
};

typedef int (*NETFUNC_DELCLIENT)(int ClientID, const char* pReason, void *pUser);
typedef int (*NETFUNC_NEWCLIENT)(int ClientID, bool Sevendown, int Socket, void *pUser);
typedef int (*NETFUNC_CLIENTREJOIN)(int ClientID, bool Sevendown, int Socket, void *pUser);
typedef bool (*NETFUNC_CLIENTCANCLOSE)(int ClientID, void *pUser);

typedef unsigned int TOKEN;

struct CNetChunk
{
	// -1 means that it's a connless packet
	// 0 on the client means the server
	int m_ClientID;
	NETADDR m_Address; // only used when cid == -1
	int m_Flags;
	int m_DataSize;
	const void *m_pData;
	unsigned char m_aExtraData[4];
};

class CNetChunkHeader
{
public:
	int m_Flags;
	int m_Size;
	int m_Sequence;

	unsigned char *Pack(unsigned char *pData, int Split = 6);
	unsigned char *Unpack(unsigned char *pData, int Split = 6);
};

class CNetChunkResend
{
public:
	int m_Flags;
	int m_DataSize;
	unsigned char *m_pData;

	int m_Sequence;
	int64 m_LastSendTime;
	int64 m_FirstSendTime;
};

class CNetPacketConstruct
{
public:
	TOKEN m_Token;
	TOKEN m_ResponseToken; // only used in connless packets
	int m_Flags;
	int m_Ack;
	int m_NumChunks;
	int m_DataSize;
	unsigned char m_aChunkData[NET_MAX_PAYLOAD];
	unsigned char m_aExtraData[4];
};


class CNetBase
{
	class CNetInitializer
	{
	public:
		CNetInitializer()
		{
			// init the network
			net_init();
		}
	};
	static CNetInitializer m_NetInitializer;

	class CConfig *m_pConfig;
	class IEngine *m_pEngine;
	NETSOCKET m_aSocket[NUM_SOCKETS];
	IOHANDLE m_DataLogSent;
	IOHANDLE m_DataLogRecv;
	CHuffman m_Huffman;
	unsigned char m_aRequestTokenBuf[NET_TOKENREQUEST_DATASIZE];

public:
	CNetBase();
	CConfig *Config() { return m_pConfig; }
	class IEngine *Engine() { return m_pEngine; }
	int NetType(int Socket) { return net_socket_type(m_aSocket[Socket]); }
	
	void Init(NETSOCKET Socket, NETSOCKET SocketTwo, class CConfig *pConfig, class IConsole *pConsole, class IEngine *pEngine);
	void Shutdown();
	void UpdateLogHandles();
	void Wait(int Time);

	void SendControlMsg(const NETADDR *pAddr, TOKEN Token, int Ack, int ControlMsg, const void *pExtra, int ExtraSize, bool Sevendown, int Socket, SECURITY_TOKEN SecurityToken = NET_SECURITY_TOKEN_UNSUPPORTED);
	void SendControlMsgWithToken(const NETADDR *pAddr, TOKEN Token, int Ack, int ControlMsg, TOKEN MyToken, bool Extended, int Socket);
	void SendPacketConnless(const NETADDR *pAddr, TOKEN Token, TOKEN ResponseToken, const void *pData, int DataSize, bool Sevendown, int Socket);
	void SendPacket(const NETADDR *pAddr, CNetPacketConstruct *pPacket, bool Sevendown, int Socket, SECURITY_TOKEN SecurityToken = NET_SECURITY_TOKEN_UNSUPPORTED);
	int UnpackPacket(NETADDR *pAddr, unsigned char *pBuffer, CNetPacketConstruct *pPacket, bool *pSevendown, int Socket, class CNetServer *pNetServer = 0);
};

class CNetTokenManager
{
public:
	void Init(CNetBase *pNetBase, int SeedTime = NET_SEEDTIME);
	void Update();

	void GenerateSeed();

	int ProcessMessage(const NETADDR *pAddr, const CNetPacketConstruct *pPacket, int Socket);

	bool CheckToken(const NETADDR *pAddr, TOKEN Token, TOKEN ResponseToken, bool *BroadcastResponse);
	TOKEN GenerateToken(const NETADDR *pAddr) const;
	static TOKEN GenerateToken(const NETADDR *pAddr, int64 Seed);

private:
	CNetBase *m_pNetBase;

	int64 m_Seed;
	int64 m_PrevSeed;

	TOKEN m_GlobalToken;
	TOKEN m_PrevGlobalToken;

	int m_SeedTime;
	int64 m_NextSeedTime;
};

typedef void(*FSendCallback)(int TrackID, void *pUser);
struct CSendCBData
{
	FSendCallback m_pfnCallback;
	void *m_pCallbackUser;
	int m_TrackID;
};

class CNetTokenCache
{
public:
	CNetTokenCache();
	~CNetTokenCache();
	void Init(CNetBase *pNetBase, const CNetTokenManager *pTokenManager);
	void SendPacketConnless(const NETADDR *pAddr, const void *pData, int DataSize, int Socket, CSendCBData *pCallbackData = 0);
	void PurgeStoredPacket(int TrackID);
	void FetchToken(const NETADDR *pAddr, int Socket);
	void AddToken(const NETADDR *pAddr, TOKEN PeerToken, int TokenFlag, int Socket);
	TOKEN GetToken(const NETADDR *pAddr);
	void Update(int Socket);

private:
	class CConnlessPacketInfo
	{
	private:
		static int m_UniqueID;

	public:
		CConnlessPacketInfo() : m_TrackID(CConnlessPacketInfo::m_UniqueID++) {}

		NETADDR m_Addr;
		int m_DataSize;
		char m_aData[NET_MAX_PAYLOAD];
		int64 m_Expiry;
		int64 m_LastTokenRequest;
		const int m_TrackID;
		FSendCallback m_pfnCallback;
		void *m_pCallbackUser;
		CConnlessPacketInfo *m_pNext;
	};

	struct CAddressInfo
	{
		NETADDR m_Addr;
		TOKEN m_Token;
		int64 m_Expiry;
	};

	TStaticRingBuffer<CAddressInfo,
	                  NET_TOKENCACHE_SIZE * sizeof(CAddressInfo),
	                  CRingBufferBase::FLAG_RECYCLE> m_TokenCache;

	CConnlessPacketInfo *m_pConnlessPacketList; // TODO: enhance this, dynamic linked lists
	                                            // are bad for performance
	CNetBase *m_pNetBase;
	const CNetTokenManager *m_pTokenManager;
};


class CNetConnection
{
	// TODO: is this needed because this needs to be aware of
	// the ack sequencing number and is also responible for updating
	// that. this should be fixed.
	friend class CNetRecvUnpacker;
private:
	unsigned short m_Sequence;
	unsigned short m_Ack;
	unsigned short m_PeerAck;
	unsigned m_State;

	int m_RemoteClosed;
	bool m_BlockCloseMsg;

	TStaticRingBuffer<CNetChunkResend, NET_CONN_BUFFERSIZE> m_Buffer;

	int64 m_LastUpdateTime;
	int64 m_LastRecvTime;
	int64 m_LastSendTime;

	char m_ErrorString[256];

	CNetPacketConstruct m_Construct;

	TOKEN m_Token;
	TOKEN m_PeerToken;
	NETADDR m_PeerAddr;

	NETSTATS m_Stats;
	CNetBase *m_pNetBase;

	//
	void ResetStats();
	void SetError(const char *pString);
	void AckChunks(int Ack);

	int QueueChunkEx(int Flags, int DataSize, const void *pData, int Sequence);
	void SendControl(int ControlMsg, const void *pExtra, int ExtraSize);
	void SendControlWithToken(int ControlMsg);
	void ResendChunk(CNetChunkResend *pResend);
	void Resend();

	static TOKEN GenerateToken(const NETADDR *pPeerAddr);

public:
	void Reset();
	void Init(CNetBase *pNetBase, bool BlockCloseMsg);
	int Connect(NETADDR *pAddr);
	void Disconnect(const char *pReason, bool Shutdown = false);

	void SetToken(TOKEN Token);

	TOKEN Token() const { return m_Token; }
	TOKEN PeerToken() const { return m_PeerToken; }
	class CConfig *Config() { return m_pNetBase->Config(); }

	int Update();
	int Flush();

	int Feed(CNetPacketConstruct *pPacket, NETADDR *pAddr, bool Sevendown, int Socket, bool ClientCanClose = true);
	int QueueChunk(int Flags, int DataSize, const void *pData);
	void SendPacketConnless(const char *pData, int DataSize);

	const char *ErrorString();
	void SignalResend();
	int State() const { return m_State; }
	const NETADDR *PeerAddress() const { return &m_PeerAddr; }

	void ResetErrorString() { m_ErrorString[0] = 0; }
	const char *ErrorString() const { return m_ErrorString; }

	// Needed for GotProblems in NetClient
	int64 LastRecvTime() const { return m_LastRecvTime; }
	int64 ConnectTime() const { return m_LastUpdateTime; }

	int AckSequence() const { return m_Ack; }
	// The backroom is ack-NET_MAX_SEQUENCE/2. Used for knowing if we acked a packet or not
	static int IsSeqInBackroom(int Seq, int Ack);

	// F-DDrace
	void DummyConnect();
	void DummyDrop();

	SECURITY_TOKEN m_SecurityToken;
	bool m_Sevendown;
	int m_Socket;
	void DirectInit(const NETADDR *pAddr, const CNetPacketConstruct *pPacket, SECURITY_TOKEN SecurityToken, bool Sevendown, int Socket);

	int SeqSequence() const { return m_Sequence; }
	TStaticRingBuffer<CNetChunkResend, NET_CONN_BUFFERSIZE> *ResendBuffer() { return &m_Buffer; };
	bool m_TimeoutProtected;
	bool m_TimeoutSituation;
	void SetTimedOut(const NETADDR *pAddr, int Sequence, int Ack, TOKEN Token, TStaticRingBuffer<CNetChunkResend, NET_CONN_BUFFERSIZE> *pResendBuffer, TOKEN PeerToken, bool Sevendown, SECURITY_TOKEN SecurityToken);
};

class CConsoleNetConnection
{
private:
	int m_State;

	NETADDR m_PeerAddr;
	NETSOCKET m_Socket;

	char m_aBuffer[NET_MAX_PACKETSIZE];
	int m_BufferOffset;

	char m_aErrorString[256];

	bool m_LineEndingDetected;
	char m_aLineEnding[3];

public:
	void Init(NETSOCKET Socket, const NETADDR *pAddr);
	void Disconnect(const char *pReason);

	int State() const { return m_State; }
	const NETADDR *PeerAddress() const { return &m_PeerAddr; }
	const char *ErrorString() const { return m_aErrorString; }

	void Reset();
	int Update();
	int Send(const char *pLine);
	int Recv(char *pLine, int MaxLength);
};

class CNetRecvUnpacker
{
	bool m_Valid;

public:
	NETADDR m_Addr;
	CNetConnection *m_pConnection;
	int m_CurrentChunk;
	int m_ClientID;
	CNetPacketConstruct m_Data;
	unsigned char m_aBuffer[NET_MAX_PACKETSIZE];

	CNetRecvUnpacker() { Clear(); }
	bool IsActive() { return m_Valid; }
	void Clear();
	void Start(const NETADDR *pAddr, CNetConnection *pConnection, int ClientID);
	int FetchChunk(CNetChunk *pChunk);
};

// server side
class CNetServer : public CNetBase
{
	struct CSlot
	{
	public:
		CNetConnection m_Connection;
	};

	class CNetBan *m_pNetBan;
	CSlot m_aSlots[NET_MAX_CLIENTS];
	int m_NumClients;
	int m_MaxClients;
	int m_MaxClientsPerIP;

	NETFUNC_NEWCLIENT m_pfnNewClient;
	NETFUNC_DELCLIENT m_pfnDelClient;
	NETFUNC_CLIENTREJOIN m_pfnClientRejoin;
	void *m_UserPtr;

	// F-DDrace
	NETFUNC_CLIENTCANCLOSE m_pfnClientCanClose;

	struct CSpamConn
	{
		NETADDR m_Addr;
		int64 m_Time;
		int m_Conns;
	};

	CSpamConn m_aSpamConns[NET_CONNLIMIT_IPS];
	unsigned char m_SecurityTokenSeed[16];

	CNetRecvUnpacker m_RecvUnpacker;

	CNetTokenManager m_TokenManager;
	CNetTokenCache m_TokenCache;

public:
	//
	bool Open(NETADDR BindAddr, class CConfig *pConfig, class IConsole *pConsole, class IEngine *pEngine, class CNetBan *pNetBan,
		int MaxClients, int MaxClientsPerIP, NETFUNC_NEWCLIENT pfnNewClient, NETFUNC_DELCLIENT pfnDelClient,
		NETFUNC_CLIENTREJOIN pfnClientRejoin, NETFUNC_CLIENTCANCLOSE pfnClientCanClose, void *pUser);
	void Close();

	// the token parameter is only used for connless packets
	int Recv(CNetChunk *pChunk, TOKEN *pResponseToken, bool *pSevendown, int Socket);
	int Send(CNetChunk *pChunk, TOKEN Token = NET_TOKEN_NONE, bool Sevendown = false, int Socket = SOCKET_MAIN); // Sevendown and Socket is only for connless packets
	int Update();
	void AddToken(const NETADDR *pAddr, TOKEN Token, int Socket) { m_TokenCache.AddToken(pAddr, Token, 0, Socket); };

	//
	void Drop(int ClientID, const char *pReason, bool Banned = false, bool Shutdown = false);

	// status requests
	const NETADDR *ClientAddr(int ClientID) const { return m_aSlots[ClientID].m_Connection.PeerAddress(); }
	class CNetBan *NetBan() const { return m_pNetBan; }

	void SetMaxClients(int MaxClients);
	void SetMaxClientsPerIP(int MaxClientsPerIP);

	// F-DDrace

	void DummyInit(int DummyID);
	void DummyDelete(int DummyID);

	bool Connlimit(NETADDR Addr);

	char m_ShutdownMessage[128];

	int NumClients() { return m_NumClients; }
	SECURITY_TOKEN GetGlobalToken();
	SECURITY_TOKEN GetSecurityToken(const NETADDR& Addr);
	bool GetSevendown(const NETADDR *pAddr, CNetPacketConstruct *pPacket, unsigned char *pBuffer);

	int ResetErrorString(int ClientID);
	const char *ErrorString(int ClientID);
	bool SetTimedOut(int ClientID, int OrigID);
	void SetTimeoutProtected(int ClientID);
};

class CNetConsole
{
	struct CSlot
	{
		CConsoleNetConnection m_Connection;
	};

	NETSOCKET m_Socket;
	class CNetBan *m_pNetBan;
	CSlot m_aSlots[NET_MAX_CONSOLE_CLIENTS];

	NETFUNC_NEWCLIENT m_pfnNewClient;
	NETFUNC_DELCLIENT m_pfnDelClient;
	void *m_UserPtr;

	CNetRecvUnpacker m_RecvUnpacker;

public:
	//
	bool Open(NETADDR BindAddr, class CNetBan *pNetBan, NETFUNC_NEWCLIENT pfnNewClient, NETFUNC_DELCLIENT pfnDelClient, void *pUser);
	void Close();

	//
	int Recv(char *pLine, int MaxLength, int *pClientID = 0);
	int Send(int ClientID, const char *pLine);
	int Update();
	void SetLingerState(int State);

	//
	int AcceptClient(NETSOCKET Socket, const NETADDR *pAddr);
	void Drop(int ClientID, const char *pReason);

	// status requests
	const NETADDR *ClientAddr(int ClientID) const { return m_aSlots[ClientID].m_Connection.PeerAddress(); }
	class CNetBan *NetBan() const { return m_pNetBan; }
};



// client side
class CNetClient : public CNetBase
{
	CNetConnection m_Connection;
	CNetRecvUnpacker m_RecvUnpacker;

	CNetTokenCache m_TokenCache;
	CNetTokenManager m_TokenManager;

	int m_Flags;

public:
	// openness
	bool Open(NETADDR BindAddr, class CConfig *pConfig, class IConsole *pConsole, class IEngine *pEngine, int Flags);
	void Close();

	// connection state
	int Disconnect(const char *Reason);
	int Connect(NETADDR *Addr);

	// communication
	int Recv(CNetChunk *pChunk, TOKEN *pResponseToken = 0);
	int Send(CNetChunk *pChunk, TOKEN Token = NET_TOKEN_NONE, CSendCBData *pCallbackData = 0);
	void PurgeStoredPacket(int TrackID);

	// pumping
	int Update();
	int Flush();

	int ResetErrorString();

	// error and state
	int State() const;
	bool GotProblems() const;
	const char *ErrorString() const;
};

#endif
