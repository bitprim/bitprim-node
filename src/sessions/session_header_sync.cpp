/**
 * Copyright (c) 2011-2017 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <bitcoin/node/sessions/session_header_sync.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/protocols/protocol_header_sync.hpp>
#include <bitcoin/node/full_node.hpp>
#include <bitcoin/node/settings.hpp>
#include <bitcoin/node/utility/check_list.hpp>

namespace libbitcoin {
namespace node {

#define CLASS session_header_sync
#define NAME "session_header_sync"

using namespace bc::blockchain;
using namespace bc::config;
using namespace bc::database;
using namespace bc::message;
using namespace bc::network;
using namespace std::placeholders;

// The minimum rate back off factor, must be < 1.0.
static constexpr float back_off_factor = 0.75f;

// The starting minimum header download rate, exponentially backs off.
static constexpr uint32_t headers_per_second = 10000;

// Sort is required here but not in configuration settings.
session_header_sync::session_header_sync(full_node& network,
    check_list& hashes, fast_chain& blockchain,
    const checkpoint::list& checkpoints)
  : session<network::session_outbound>(network, false),
    hashes_(hashes),
    minimum_rate_(headers_per_second),
    chain_(blockchain),
    checkpoints_(checkpoint::sort(checkpoints)),
    CONSTRUCT_TRACK(session_header_sync)
{
    static_assert(back_off_factor < 1.0, "invalid back-off factor");
}

// Start sequence.
// ----------------------------------------------------------------------------

void session_header_sync::start(result_handler handler)
{
    session::start(CONCURRENT_DELEGATE2(handle_started, _1, handler));
}

void session_header_sync::handle_started(const code& ec,
    result_handler handler)
{
    if (ec)
    {
        handler(ec);
        return;
    }

    // TODO: expose header count and emit here.
    LOG_INFO(LOG_NODE)
        << "Getting headers.";

    if (!initialize())
    {
        handler(error::operation_failed);
        return;
    }

    const auto complete = synchronize(handler, headers_.size(), NAME);

    // This is the end of the start sequence.
    for (const auto row: headers_)
        new_connection(row, complete);
}

// Header sync sequence.
// ----------------------------------------------------------------------------

void session_header_sync::new_connection(header_list::ptr row,
    result_handler handler)
{
    if (stopped())
    {
        LOG_DEBUG(LOG_NODE)
            << "Suspending header slot (" << row->slot() << ").";
        return;
    }

    LOG_DEBUG(LOG_NODE)
        << "Starting header slot (" << row->slot() << ").";

    // HEADER SYNC CONNECT
    session_batch::connect(BIND4(handle_connect, _1, _2, row, handler));
}

void session_header_sync::handle_connect(const code& ec, channel::ptr channel,
    header_list::ptr row, result_handler handler)
{
    if (ec)
    {
        LOG_DEBUG(LOG_NODE)
            << "Failure connecting header slot (" << row->slot() << ") "
            << ec.message();
        new_connection(row ,handler);
        return;
    }

    LOG_DEBUG(LOG_NODE)
        << "Connected header slot (" << row->slot() << ") ["
        << channel->authority() << "]";

    register_channel(channel,
        BIND4(handle_channel_start, _1, channel, row, handler),
        BIND2(handle_channel_stop, _1, row));
}

void session_header_sync::attach_handshake_protocols(channel::ptr channel,
    result_handler handle_started)
{
    // Don't use configured services, relay or min version for header sync.
    const auto relay = false;
    const auto own_version = settings_.protocol_maximum;
    const auto own_services = version::service::none;
    const auto invalid_services = settings_.invalid_services;
    const auto minimum_version = version::level::headers;
    const auto minimum_services = version::service::node_network;

    // The negotiated_version is initialized to the configured maximum.
    if (channel->negotiated_version() >= version::level::bip61)
        attach<protocol_version_70002>(channel, own_version, own_services,
            invalid_services, minimum_version, minimum_services, relay)
            ->start(handle_started);
    else
        attach<protocol_version_31402>(channel, own_version, own_services,
            invalid_services, minimum_version, minimum_services)
            ->start(handle_started);
}

void session_header_sync::handle_channel_start(const code& ec,
    channel::ptr channel, header_list::ptr row, result_handler handler)
{
    // Treat a start failure just like a completion failure.
    if (ec)
    {
        handle_complete(ec, row, handler);
        return;
    }

    attach_protocols(channel, row, handler);
}

void session_header_sync::attach_protocols(channel::ptr channel,
    header_list::ptr row, result_handler handler)
{
    BITCOIN_ASSERT(channel->negotiated_version() >= version::level::headers);

    if (channel->negotiated_version() >= version::level::bip31)
        attach<protocol_ping_60001>(channel)->start();
    else
        attach<protocol_ping_31402>(channel)->start();

    attach<protocol_address_31402>(channel)->start();
    attach<protocol_header_sync>(channel, row, minimum_rate_)->start(
        BIND3(handle_complete, _1, row, handler));
}

void session_header_sync::handle_complete(const code& ec,
    header_list::ptr row, result_handler handler)
{
    if (ec)
    {
        // Reduce the rate minimum so that we don't get hung up.
        minimum_rate_ = static_cast<uint32_t>(minimum_rate_ * back_off_factor);

        // There is no failure scenario, we ignore the result code here.
        new_connection(row, handler);
        return;
    }

    //*************************************************************************
    // TODO: as each header sink slot completes store headers to database and
    // assign its checkpoints to hashes_, terminating the slot.
    //*************************************************************************

    auto height = row->first_height();
    const auto& headers = row->headers();

    // Store the hash if there is a gap reservation.
    for (const auto& header: headers)
        hashes_.enqueue(header.hash(), height++);

    LOG_DEBUG(LOG_NODE)
        << "Completed header slot (" << row->slot() << ")";

    // This is the end of the header sync sequence.
    handler(error::success);
}

void session_header_sync::handle_channel_stop(const code& ec,
    header_list::ptr row)
{
    LOG_DEBUG(LOG_NODE)
        << "Channel stopped on header slot (" << row->slot() << ") "
        << ec.message();
}

// Utility.
// ----------------------------------------------------------------------------

bool session_header_sync::initialize()
{
    if (!hashes_.empty()) {
        LOG_ERROR(LOG_NODE)
            << "Block hash list must not be initialized.";
        return false;
    }

    //LOG_INFO(LOG_NODE) << "asm int $3 - 14";
    //asm("int $3");  //TODO(fernando): remover
#ifdef BITPRIM_DB_LEGACY     
    block_database::heights gaps;
    // Populate hash buckets from full database empty height scan.
    if ( ! chain_.get_gaps(gaps)) {
        return false;
    }
    // TODO: consider populating this directly in the database.
    hashes_.reserve(gaps);
//#else
   //return false;
#endif // BITPRIM_DB_LEGACY     

    //*************************************************************************
    // TODO: get top and pair up checkpoints into slots.
    const auto& front = checkpoints_.front();
    const auto& back = checkpoints_.back();
    headers_.push_back(std::make_shared<header_list>(0, front, back));
    //*************************************************************************

    return true;
}

} // namespace node
} // namespace libbitcoin
