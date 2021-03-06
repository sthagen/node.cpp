/* -------------------------------------------------------------------------------
* Copyright (c) 2019, OLogN Technologies AG
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
*     * Neither the name of the OLogN Technologies AG nor the
*       names of its contributors may be used to endorse or promote products
*       derived from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL OLogN Technologies AG BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
* -------------------------------------------------------------------------------*/


#ifndef CLUSTERING_COMMON_H
#define CLUSTERING_COMMON_H

#include "../../include/nodecpp/common.h"
#include "../../include/nodecpp/net_common.h"

struct ThreadStartupData
{
	size_t assignedThreadID;
	uint16_t commPort;
	nodecpp::log::Log* defaultLog = nullptr;
};

// ad-hoc marchalling between Master and Slave threads
struct ClusteringMsgHeader
{
	enum ClusteringMsgType { ThreadStarted, ServerListening, ConnAccepted, ServerError, ServerCloseRequest, ServerClosedNotification };
	size_t bodySize;
	ClusteringMsgType type;
	size_t assignedThreadID;
	size_t requestID;
	size_t entryIdx;
	void serialize( nodecpp::Buffer& b ) { b.append( this, sizeof( ClusteringMsgHeader ) ); }
	size_t deserialize( const nodecpp::Buffer& b, size_t pos ) { 
		NODECPP_ASSERT( nodecpp::module_id, ::nodecpp::assert::AssertLevel::critical, pos + sizeof( ClusteringMsgHeader ) <= b.size(), "indeed: {} + {} vs. {}", pos, sizeof( ClusteringMsgHeader ), b.size() ); 
		memcpy( this, b.begin() + pos, sizeof( ClusteringMsgHeader ) );
		return pos + sizeof( ClusteringMsgHeader );
	}
	static bool couldBeDeserialized( const nodecpp::Buffer& b, size_t pos = 0 ) { return b.size() >= pos + sizeof( ClusteringMsgHeader ); }
};

#endif // CLUSTERING_COMMON_H
