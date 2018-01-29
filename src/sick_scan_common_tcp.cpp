/*
* Copyright (C) 2013, Osnabrück University
* Copyright (C) 2017, Ing.-Buero Dr. Michael Lehning, Hildesheim
* Copyright (C) 2017, SICK AG, Waldkirch
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
*     * Neither the name of Osnabrück University nor the names of its
*       contributors may be used to endorse or promote products derived from
*       this software without specific prior written permission.
*     * Neither the name of SICK AG nor the names of its
*       contributors may be used to endorse or promote products derived from
*       this software without specific prior written permission
*     * Neither the name of Ing.-Buero Dr. Michael Lehning nor the names of its
*       contributors may be used to endorse or promote products derived from
*       this software without specific prior written permission
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*
*  Last modified: 12th Dec 2017
*
*      Authors:
*              Michael Lehning <michael.lehning@lehning.de>
*         Jochen Sprickerhof <jochen@sprickerhof.de>
*         Martin Günther <mguenthe@uos.de>
*
* Based on the TiM communication example by SICK AG.
*
*/

#ifdef _MSC_VER
#pragma warning(disable: 4996)
#pragma warning(disable: 4267)
#pragma warning(disable: 4101)   // C4101: "e" : Unreferenzierte lokale Variable
#define _WIN32_WINNT 0x0501

#endif

#include <sick_scan/sick_scan_common_tcp.h>
#include <sick_scan/tcp/colaa.hpp>
#include <sick_scan/tcp/colab.hpp>

#include <boost/asio.hpp>
#include <boost/lambda/lambda.hpp>
#include <algorithm>
#include <iterator>
#include <boost/lexical_cast.hpp>
#include <vector>
#ifdef _MSC_VER
#include "sick_scan/rosconsole_simu.hpp"
#endif

std::vector<unsigned char> exampleData(65536);
std::vector<unsigned char> receivedData(65536);
static long receivedDataLen = 0;
static int getDiagnosticErrorCode()
{
#ifdef _MSC_VER
#undef ERROR
	return(2);
#else
	return(diagnostic_msgs::DiagnosticStatus::ERROR);
#endif
}
namespace sick_scan
{

	SickScanCommonTcp::SickScanCommonTcp(const std::string &hostname, const std::string &port, int &timelimit, SickGenericParser* parser)
		:
		SickScanCommon(parser),
		socket_(io_service_),
		deadline_(io_service_),
		hostname_(hostname),
		port_(port),
		timelimit_(timelimit)
	{
		m_alreadyReceivedBytes = 0;
		this->setReplyMode(0);
		// io_service_.setReadCallbackFunction(boost::bind(&SopasDevice::readCallbackFunction, this, _1, _2));

		// Set up the deadline actor to implement timeouts.
		// Based on blocking TCP example on:
		// http://www.boost.org/doc/libs/1_46_0/doc/html/boost_asio/example/timeouts/blocking_tcp_client.cpp
		deadline_.expires_at(boost::posix_time::pos_infin);
		checkDeadline();

	}

	SickScanCommonTcp::~SickScanCommonTcp()
	{
		stop_scanner();
		close_device();
	}

	using boost::asio::ip::tcp;
	using boost::lambda::var;
	using boost::lambda::_1;


	void SickScanCommonTcp::disconnectFunction()
	{

	}

	void SickScanCommonTcp::disconnectFunctionS(void *obj)
	{
		if (obj != NULL)
		{
			((SickScanCommonTcp *)(obj))->disconnectFunction();
		}
	}

	void SickScanCommonTcp::readCallbackFunctionS(void* obj, UINT8* buffer, UINT32& numOfBytes)
	{
		((SickScanCommonTcp*)obj)->readCallbackFunction(buffer, numOfBytes);
	}


	void SickScanCommonTcp::setReplyMode(int _mode)
	{
		m_replyMode = _mode;
	}
	int SickScanCommonTcp::getReplyMode()
	{
		return(m_replyMode);
	}

	/**
 * Read callback. Diese Funktion wird aufgerufen, sobald Daten auf der Schnittstelle
 * hereingekommen sind.
 */
	void SickScanCommonTcp::readCallbackFunction(UINT8* buffer, UINT32& numOfBytes)
	{
		// should be member variable in the future


#if 0

		this->recvQueue.push(std::vector<unsigned char>(buffer, buffer + numOfBytes));

		if (this->getReplyMode() == 0)
		{
			this->recvQueue.push(std::vector<unsigned char>(buffer, buffer + numOfBytes));
			return;
		}
#endif

		// starting with 0x02 - but no magic word -> ASCII-Command-Reply
		if ((numOfBytes < 2) && (m_alreadyReceivedBytes == 0))
		{
			return;  // ultra short message (only 1 byte) must be nonsense 
		}
		if ((buffer[0] == 0x02) && (buffer[1] != 0x02)) // no magic word, but received initial 0x02 -> guess Ascii reply
		{
			if (numOfBytes > 0)
			{
				// check last character of message - must be 0x03 
				char lastChar = buffer[numOfBytes - 1];  // check last for 0x03
				if (lastChar == 0x03)  
				{
					memcpy(m_packetBuffer, buffer, numOfBytes);
					m_alreadyReceivedBytes = numOfBytes;
					recvQueue.push(std::vector<unsigned char>(m_packetBuffer, m_packetBuffer + numOfBytes));
					m_alreadyReceivedBytes = 0;
				}
				else
				{

					ROS_WARN("Dropping packages???\n");
					FILE *fout = fopen("/tmp/package.bin", "wb");
					if (fout != NULL)
					{
						fwrite(m_packetBuffer, 1, numOfBytes, fout);
						fclose(fout);
					}
				}
			}
		}

		if ((numOfBytes < 9) && (m_alreadyReceivedBytes == 0))
		{
			return;
		}


		// check magic word for cola B
		if ((m_alreadyReceivedBytes > 0) || (buffer[0] == 0x02 && buffer[1] == 0x02 && buffer[2] == 0x02 && buffer[3] == 0x02))
		{
			std::string command;
			UINT16 nextData = 4;
			UINT32 numberBytes = numOfBytes;
			if (m_alreadyReceivedBytes == 0)
			{
				const char *packetKeyWord = "sSN LMDscandata";
				m_lastPacketSize = colab::getIntegerFromBuffer<UINT32>(buffer, nextData);
				//
				m_lastPacketSize += 9; // Magic number + CRC

				// Check for "normal" command reply
				if (strncmp((char *)(buffer + 8), packetKeyWord, strlen(packetKeyWord)) != 0)
				{
					// normal command reply
					this->recvQueue.push(std::vector<unsigned char>(buffer, buffer + numOfBytes));
					return;
				}

				// probably a scan
				if (m_lastPacketSize > 4000)
				{
					INT16 topmostLayerAngle = 1350 * 2 - 62; // for identification of first layer of a scan
					UINT16 layerPos = 24 + 26;
					INT16 layerAngle = colab::getIntegerFromBuffer<INT16>(buffer, layerPos);

					if (layerAngle == topmostLayerAngle)
					{
						// wait for all 24 layers
						// m_lastPacketSize = m_lastPacketSize * 24;

						// traceDebug(MRS6xxxB_VERSION) << "Received new scan" << std::endl;
					}
				}
			}

			// copy
			memcpy(m_packetBuffer + m_alreadyReceivedBytes, buffer, numOfBytes);
			m_alreadyReceivedBytes += numberBytes;

			if (m_alreadyReceivedBytes < m_lastPacketSize)
			{
				// wait for completeness of packet
				return;
			}

			m_alreadyReceivedBytes = 0;
			recvQueue.push(std::vector<unsigned char>(m_packetBuffer, m_packetBuffer + m_lastPacketSize));
		}
	}


	int SickScanCommonTcp::init_device()
	{
		int portInt;
		sscanf(port_.c_str(), "%d", &portInt);
		m_nw.init(hostname_, portInt, disconnectFunctionS, (void*)this);
		m_nw.setReadCallbackFunction(readCallbackFunctionS, (void*)this);
		m_nw.connect();
		return ExitSuccess;
	}

	int SickScanCommonTcp::close_device()
	{
		if (socket_.is_open())
		{
			try
			{
				socket_.close();
			}
			catch (boost::system::system_error &e)
			{
				ROS_ERROR("An error occured during closing of the connection: %d:%s", e.code().value(), e.code().message().c_str());
			}
		}
		return 0;
	}

	void SickScanCommonTcp::handleRead(boost::system::error_code error, size_t bytes_transfered)
	{
		ec_ = error;
		bytes_transfered_ += bytes_transfered;
	}


	void SickScanCommonTcp::checkDeadline()
	{
		if (deadline_.expires_at() <= boost::asio::deadline_timer::traits_type::now())
		{
			// The reason the function is called is that the deadline expired. Close
			// the socket to return all IO operations and reset the deadline
			socket_.close();
			deadline_.expires_at(boost::posix_time::pos_infin);
		}

		// Nothing bad happened, go back to sleep
		deadline_.async_wait(boost::bind(&SickScanCommonTcp::checkDeadline, this));
	}

	int SickScanCommonTcp::readWithTimeout(size_t timeout_ms, char *buffer, int buffer_size, int *bytes_read, bool *exception_occured, bool isBinary)
	{
		// Set up the deadline to the proper timeout, error and delimiters
		deadline_.expires_from_now(boost::posix_time::milliseconds(timeout_ms));
		const char end_delim = static_cast<char>(0x03);
		int dataLen = 0;
		ec_ = boost::asio::error::would_block;
		bytes_transfered_ = 0;

		size_t to_read;

		int numBytes = 0;
		std::vector<unsigned char> recvData = this->recvQueue.pop();
		*bytes_read = recvData.size();
		memcpy(buffer, &(recvData[0]), recvData.size());
		return(ExitSuccess);
	}

	/**
	 * Send a SOPAS command to the device and print out the response to the console.
	 */
	int SickScanCommonTcp::sendSOPASCommand(const char* request, std::vector<unsigned char> * reply, int cmdLen)
	{
#if 0
		if (!socket_.is_open()) {
			ROS_ERROR("sendSOPASCommand: socket not open");
			diagnostics_.broadcast(getDiagnosticErrorCode(), "sendSOPASCommand: socket not open.");
			return ExitError;
		}
#endif
		int sLen = 0;
		int preambelCnt = 0;
		bool cmdIsBinary = false;

		if (request != NULL)
		{
			sLen = cmdLen;
			preambelCnt = 0; // count 0x02 bytes to decide between ascii and binary command
			if (sLen >= 4)
			{
				for (int i = 0; i < 4; i++) {
					if (request[i] == 0x02)
					{
						preambelCnt++;
					}
				}
			}

			if (preambelCnt < 4) {
				cmdIsBinary = false;
			}
			else
			{
				cmdIsBinary = true;
			}
			int msgLen = 0;
			if (cmdIsBinary == false)
			{
				msgLen = strlen(request);
			}
			else
			{
				int dataLen = 0;
				for (int i = 4; i < 8; i++)
				{
					dataLen |= ((unsigned char)request[i] << (7 - i) * 8);
				}
				msgLen = 8 + dataLen + 1; // 8 Msg. Header + Packet +
			}
#if 1
			m_nw.sendCommandBuffer((UINT8*)request, msgLen);
#else

			/*
			 * Write a SOPAS variable read request to the device.
			 */
			try
			{
				boost::asio::write(socket_, boost::asio::buffer(request, msgLen));
			}
			catch (boost::system::system_error &e)
			{
				ROS_ERROR("write error for command: %s", request);
				diagnostics_.broadcast(getDiagnosticErrorCode(), "Write error for sendSOPASCommand.");
				return ExitError;
			}
#endif
		}

		// Set timeout in 5 seconds
		const int BUF_SIZE = 1000;
		char buffer[BUF_SIZE];
		int bytes_read;
		if (readWithTimeout(20000, buffer, BUF_SIZE, &bytes_read, 0, cmdIsBinary) == ExitError)
		{
			ROS_ERROR_THROTTLE(1.0, "sendSOPASCommand: no full reply available for read after 1s");
			diagnostics_.broadcast(getDiagnosticErrorCode(), "sendSOPASCommand: no full reply available for read after 5 s.");
			return ExitError;
		}

		if (reply)
		{
			reply->resize(bytes_read);
			std::copy(buffer, buffer + bytes_read, &(*reply)[0]);
		}

		return ExitSuccess;
	}

	int SickScanCommonTcp::get_datagram(unsigned char* receiveBuffer, int bufferSize, int* actual_length, bool isBinaryProtocol)
	{
		this->setReplyMode(1);
		std::vector<unsigned char> dataBuffer = this->recvQueue.pop();
		long size = dataBuffer.size();
		memcpy(receiveBuffer, &(dataBuffer[0]), size);
		*actual_length = size;

#if 0
		static int cnt = 0;
		char szFileName[255];
		sprintf(szFileName, "/tmp/dg%06d.bin", cnt++);

		FILE *fout;

		fout = fopen(szFileName, "wb");
		if (fout != NULL)
		{
			fwrite(receiveBuffer, size, 1, fout);
			fclose(fout);
		}
#endif
		return ExitSuccess;

		if (!socket_.is_open()) {
			ROS_ERROR("get_datagram: socket not open");
			diagnostics_.broadcast(getDiagnosticErrorCode(), "get_datagram: socket not open.");
			return ExitError;
		}

		/*
		 * Write a SOPAS variable read request to the device.
		 */
		std::vector<unsigned char> reply;

		// Wait at most 5000ms for a new scan
		size_t timeout = 30000;
		bool exception_occured = false;

		char *buffer = reinterpret_cast<char *>(receiveBuffer);

		if (readWithTimeout(timeout, buffer, bufferSize, actual_length, &exception_occured, isBinaryProtocol) != ExitSuccess)
		{
			ROS_ERROR_THROTTLE(1.0, "get_datagram: no data available for read after %zu ms", timeout);
			diagnostics_.broadcast(getDiagnosticErrorCode(), "get_datagram: no data available for read after timeout.");

			// Attempt to reconnect when the connection was terminated
			if (!socket_.is_open())
			{
#ifdef _MSC_VER
				Sleep(1000);
#else
				sleep(1);
#endif
				ROS_INFO("Failure - attempting to reconnect");
				return init();
			}

			return exception_occured ? ExitError : ExitSuccess;    // keep on trying
		}

		return ExitSuccess;
	}

} /* namespace sick_scan */
