/*
	------------------------------------------------------------------------------------
	LICENSE:
	------------------------------------------------------------------------------------
	This file is part of EVEmu: EVE Online Server Emulator
	Copyright 2006 - 2008 The EVEmu Team
	For the latest information visit http://evemu.mmoforge.org
	------------------------------------------------------------------------------------
	This program is free software; you can redistribute it and/or modify it under
	the terms of the GNU Lesser General Public License as published by the Free Software
	Foundation; either version 2 of the License, or (at your option) any later
	version.

	This program is distributed in the hope that it will be useful, but WITHOUT
	ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
	FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public License along with
	this program; if not, write to the Free Software Foundation, Inc., 59 Temple
	Place - Suite 330, Boston, MA 02111-1307, USA, or go to
	http://www.gnu.org/copyleft/lesser.txt.
	------------------------------------------------------------------------------------
	Author:		Zhur, Bloody.Rabbit
*/

#ifndef TCP_CONNECTION_H
#define TCP_CONNECTION_H

#include "network/Socket.h"
#include "threading/Mutex.h"

/** Size of error buffer TCPConnection uses. */
static const uint32 TCPCONN_ERRBUF_SIZE = 1024;
/** Size limit for receive buffer of TCPConnection. */
extern const uint32 TCPCONN_RECVBUF_LIMIT;
/** Time (in milliseconds) between periodical process for incoming/outgoing data. */
extern const uint32 TCPCONN_LOOP_GRANULARITY;

/**
 * @brief Generic class for TCP connections.
 *
 * @author Zhur, Bloody.Rabbit
 */
class TCPConnection
{
public:
    /** Describes all states this object may be in. */
	enum state_t
    {
		STATE_DISCONNECTED, /**< No connection. */
		STATE_CONNECTING,   /**< Connection pending (asynchronous connection). */
		STATE_CONNECTED,    /**< Connection established, transferring data. */
		STATE_DISCONNECTING /**< Disconnect pending, waiting for all data to be sent. */
    };

    /**
     * @brief Creates new connection in STATE_DISCONNECTED.
     */
	TCPConnection();
    /**
     * @brief Cleans connection up.
     */
	virtual ~TCPConnection();

    /** @return Remote IP. */
	uint32          GetrIP() const	 { return mrIP; }
    /** @return Remote port. */
	uint16          GetrPort() const { return mrPort; }
	/**
	 * @return String in format "<remote_address>:<remote_port>".
     *
     * @note It's kinda slow this way.
	 */
	std::string		GetAddress();
    /** @return Current state of connection. */
	state_t         GetState() const;

    /**
     * @brief Connects to specified address.
     *
     * This function does synchronous connect, ie. blocks calling thread
     * until connect either succeeds or fails. Benefit is that outcome
     * of connect is known immediately.
     *
     * @param[in]  rIP    Target remote IP address.
     * @param[in]  rPort  Target remote TCP port.
     * @param[out] errbuf String buffer which receives error desription.
     *
     * @return True if connection succeeds, false if not.
     */
	bool            Connect( uint32 rIP, uint16 rPort, char* errbuf = 0 );
    /**
     * @brief Schedules asynchronous connect to specified address.
     *
     * This function does asynchronous connect, ie. does not block
     * calling thread at all. However, result of connect is not
     * known immediately.
     *
     * @param[in] rIP   Target remote IP address.
     * @param[in] rPort Target remote TCP port.
     */
	void            AsyncConnect( uint32 rIP, uint16 rPort );
    /**
     * @brief Schedules disconnect of current connection.
     *
     * Connection will be closed as soon as possible. Note that
     * this may take some time since we wait for emptying send
     * queue before actually disconnecting.
     */
	void            Disconnect();

    /**
     * @brief Enqueues data to be sent.
     *
     * @param[in] data Pointer to data.
     * @param[in] size Size of data.
     *
     * @return True if data has been accepted, false if not.
     */
	bool			Send( const uint8* data, uint32 size );
	
protected:
    /**
     * @brief Creates connection from an existing socket.
     *
     * @param[in] sock  Socket to be used for connection.
     * @param[in] rIP   Remote IP socket is connected to.
     * @param[in] rPort Remote TCP port socket is connected to.
     */
	TCPConnection( Socket* sock, uint32 rIP, uint16 rPort );

    /**
     * @brief Sets current state to the given one.
     *
     * @param[in] state State to be switched to.
     */
	void			SetState( state_t state );

    /**
     * @brief Starts working thread.
     *
     * This function just starts a thread, does not check
     * whether there is already one running!
     */
    void            StartLoop();
    /**
     * @brief Blocks calling thread until working thread terminates.
     */
    void            WaitLoop();

    /**
     * @brief Does all stuff that needs to be periodically done to keep connection alive.
     *
     * @return True if connection should be further processed, false if not (eg. error, disconnected).
     */
	virtual bool    Process();
    /**
     * @brief Processes received data.
     *
     * This function must be overloaded by children to process received data.
     * Called every time a chunk of new data is received.
     *
     * @return True if processing ran fine, false if not.
     */
	virtual bool    ProcessReceivedData( char* errbuf = 0 ) = 0;

    /**
     * @brief Sends data in send queue.
     *
     * @param[out] send_something This variable is set to true if anything has been sent, false otherwise.
     * @param[out] errbuf         Buffer which receives desription of error.
     *
     * @return True if send was OK, false if not.
     */
	virtual bool    SendData( bool& sent_something, char* errbuf = 0 );
    /**
     * @brief Receives data and puts them into receive queue.
     *
     * @param[out] errbuf Buffer which receives description of error.
     *
     * @return True if receive was OK, false if not.
     */
	virtual bool    RecvData( char* errbuf = 0 );
    /**
     * @brief Disconnects socket.
     */
	void            DoDisconnect();

    /**
     * @brief Clears send and receive buffers.
     */
	virtual void    ClearBuffers();

    /**
     * @brief Loop for worker threads.
     *
     * This function just casts given arg into TCPConnection and calls
     * member TCPConnectionLoop.
     *
     * @param[in] arg Pointer to TCPConnection.
     */
	static ThreadReturnType TCPConnectionLoop( void* arg );
    /**
     * @brief Loop for worker threads.
     */
    ThreadReturnType        TCPConnectionLoop();

    /** Protection of socket and associated variables. */
    mutable Mutex   mMSock;
    /** Socket for connection. */
    Socket*         mSock;
    /** State the socket is in. */
	state_t         mSockState;
    /** Remote IP the socket is connected to. */
	uint32          mrIP;
    /** Remote TCP port the socket is connected to; is in host byte order. */
	uint16          mrPort;

    /** When a thread is running TCPConnectionLoop, it acquires this mutex first; used for synchronization. */
    mutable Mutex   mMLoopRunning;

	void	        ServerSendQueuePushFront( const uint8* data, uint32 size );

    void	        ServerSendQueuePushEnd( const uint8* head_data, uint32 head_size, const uint8* data, uint32 size );
	void	        ServerSendQueuePushEnd( const uint8* head_data, uint32 head_size, uint8** data, uint32 size );
	void	        ServerSendQueuePushEnd( const uint8* data, uint32 size );
	void	        ServerSendQueuePushEnd( uint8** data, uint32 size );

	bool	        ServerSendQueuePop( uint8** data, uint32* size );
	bool	        ServerSendQueuePopForce( uint8** data, uint32* size );		//does a lock() instead of a trylock()

    /** Mutex protecting send queue. */
	mutable Mutex   mMSendQueue;
    /** Send queue. */
	uint8*          mSendBuf;
    /** Send queue allocated size. */
	uint32          mSendBufSize;
    /** Send queue used size. */
	uint32          mSendBufUsed;

    /** Receive queue. */
	uint8*          mRecvBuf;
    /** Receive queue allocated size. */
	uint32          mRecvBufSize;
    /** Receive queue used size. */
	uint32          mRecvBufUsed;
};


#endif



