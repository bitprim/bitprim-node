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
#include <bitcoin/node/protocols/protocol_block_out.hpp>

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/full_node.hpp>
#include <bitcoin/bitcoin/math/sip_hash.hpp>

namespace libbitcoin {
namespace node {

#define NAME "block_out"
#define CLASS protocol_block_out

using namespace bc::blockchain;
using namespace bc::message;
using namespace bc::network;
using namespace std::placeholders;

protocol_block_out::protocol_block_out(full_node& node, channel::ptr channel,
    safe_chain& chain)
  : protocol_events(node, channel, NAME),
    node_(node),
    last_locator_top_(null_hash),
    chain_(chain),

    // TODO: move send_compact to a derived class protocol_block_out_70014.
    compact_to_peer_(false),
    compact_high_bandwidth_(true),
    compact_version_(1),

    // TODO: move send_headers to a derived class protocol_block_out_70012.
    headers_to_peer_(false),

    CONSTRUCT_TRACK(protocol_block_out)
{
}

// Start.
//-----------------------------------------------------------------------------

void protocol_block_out::start()
{
    protocol_events::start(BIND1(handle_stop, _1));

    // TODO: Do not enable bip152 protocol level until fully-implemented.
    // TODO: move send_compact to a derived class protocol_block_out_70014.
    if (negotiated_version() >= version::level::bip152)
    {
        // Announce compact vs. header/inventory if compact_to_peer_ is set.
        SUBSCRIBE2(send_compact, handle_receive_send_compact, _1, _2);
    }

    // TODO: move send_headers to a derived class protocol_block_out_70012.
    if (negotiated_version() >= version::level::bip130)
    {
        // Announce headers vs. inventory if headers_to_peer_ is set.
        SUBSCRIBE2(send_headers, handle_receive_send_headers, _1, _2);
    }

    // TODO: move get_headers to a derived class protocol_block_out_31800.
    SUBSCRIBE2(get_headers, handle_receive_get_headers, _1, _2);
    SUBSCRIBE2(get_blocks, handle_receive_get_blocks, _1, _2);
    SUBSCRIBE2(get_data, handle_receive_get_data, _1, _2);

    SUBSCRIBE2(get_block_transactions, handle_receive_get_block_transactions, _1, _2);

    // Subscribe to block acceptance notifications (the block-out heartbeat).
    chain_.subscribe_blockchain(BIND4(handle_reorganized, _1, _2, _3, _4));
}

// Receive send_headers and send_compact.
//-----------------------------------------------------------------------------

// TODO: move send_compact to a derived class protocol_block_out_70014.
bool protocol_block_out::handle_receive_send_compact(const code& ec,
    send_compact_const_ptr message)
{
    if (stopped(ec))
        return false;

    compact_to_peer_ = true;
    compact_high_bandwidth_ = message->high_bandwidth_mode();
    compact_version_ = message->version();
    return false;
}

// TODO: move send_headers to a derived class protocol_block_out_70012.
bool protocol_block_out::handle_receive_send_headers(const code& ec,
    send_headers_const_ptr message)
{
    if (stopped(ec))
        return false;

    // Block annoucements will be headers messages instead of inventory.
    headers_to_peer_ = true;
    return false;
}

// Receive get_headers sequence.
//-----------------------------------------------------------------------------

// TODO: move get_headers to a derived class protocol_block_out_31800.
bool protocol_block_out::handle_receive_get_headers(const code& ec,
    get_headers_const_ptr message)
{
    if (stopped(ec))
        return false;

    const auto size = message->start_hashes().size();

    if (size > max_locator)
    {
        LOG_WARNING(LOG_NODE)
            << "Excessive get_headers locator size ("
            << size << ") from [" << authority() << "]";
        stop(error::channel_stopped);
        return false;
    }

    if (size > locator_limit())
    {
        LOG_DEBUG(LOG_NODE)
            << "Disallowed get_headers locator size ("
            << size << ") from [" << authority() << "]";
        return true;
    }

    const auto threshold = last_locator_top_.load();

    chain_.fetch_locator_block_headers(message, threshold, max_get_headers,
        BIND2(handle_fetch_locator_headers, _1, _2));
    return true;
}

// TODO: move headers to a derived class protocol_block_out_31800.
void protocol_block_out::handle_fetch_locator_headers(const code& ec,
    headers_ptr message)
{
    if (stopped(ec))
        return;

    if (ec)
    {
        LOG_ERROR(LOG_NODE)
            << "Internal failure locating locator block headers for ["
            << authority() << "] " << ec.message();
        stop(ec);
        return;
    }

    if (message->elements().empty())
        return;


    // Allow a peer to sync despite our being stale.
    ////if (chain_.is_stale())
    ////    return;


     /* LOG_INFO(LOG_NODE)
            << "protocol_block_out::handle_fetch_locator_headers "
            << authority() << "] ";*/

    // Respond to get_headers with headers.
    SEND2(*message, handle_send, _1, message->command);



    // Save the locator top to limit an overlapping future request.
    last_locator_top_.store(message->elements().front().hash());
}

bool protocol_block_out::handle_receive_get_block_transactions(const code& ec, get_block_transactions_const_ptr message) {

    if (stopped(ec))
        return false;

    auto block_hash = message->block_hash();

    // LOG_INFO(LOG_NODE)
    //  << "protocol_block_out::handle_receive_get_block_transactions "
    // << " block hash -> " << encode_hash(block_hash)
    //<< " from [" << authority() << "]";

    chain_.fetch_block(block_hash, [this, message](const code& ec, block_const_ptr block, uint64_t) {
            
        if (ec == error::success) {
                    
            auto indexes = message->indexes();

            //LOG_INFO(LOG_NODE)
            //<< "protocol_block_out::handle_receive_get_block_transactions 2 "
            //<< " block hash -> " << encode_hash(block_hash)
            //<< " tx requested count " << indexes.size()
            //<< " from [" << authority() << "]";
            
            //TODO
            /*if (it->second->nHeight < chainActive.Height() - MAX_BLOCKTXN_DEPTH) {
                // If an older block is requested (should never happen in practice,
                // but can happen in tests) send a block response instead of a
                // blocktxn response. Sending a full block response instead of a
                // small blocktxn response is preferable in the case where a peer
                // might maliciously send lots of getblocktxn requests to trigger
                // expensive disk reads, because it will require the peer to
                // actually receive all the data read from disk over the network.
            }*/
       

            uint16_t offset = 0;
            for (size_t j = 0; j < indexes.size(); j++) {
                if (uint64_t(message->indexes()[j]) + uint64_t(offset) > std::numeric_limits<uint16_t>::max()) {
                    
                    //TODO(Mario) ban the peer
                }
                    
                indexes[j] = indexes[j] + offset;
                offset = indexes[j] + 1;
            }
            
            chain::transaction::list txs_list(indexes.size());

            for (size_t i = 0; i < indexes.size(); i++) {
                
                /*if (indexes[i] >= block->transactions().size()) {
                   //TODO(Mario) misbehaving node
                   return;
                }*/

                //LOG_INFO(LOG_NODE)
                //<< "protocol_block_out::handle_receive_get_block_transactions 3 "
                //<< " tx hash -> " << encode_hash(block->transactions()[indexes[i]].hash())
                //<< " from [" << authority() << "]";
            
                txs_list[i] = block->transactions()[indexes[i]];
            }

            block_transactions response(message->block_hash(),txs_list);
            SEND2(response, handle_send, _1, block_transactions::command);


            //LOG_INFO(LOG_NODE)
            //<< "protocol_block_out::handle_receive_get_block_transactions 4 "
            //<< " block hash -> " << encode_hash(block_hash)
            //<< " tx requested count " << indexes.size()
            //<< " from [" << authority() << "]";
            
        } else {
            //todo    
            //  LOG_INFO(LOG_NODE)
            //<< "protocol_block_out::handle_receive_get_block_transactions 5 "
            //<< " block hash -> " << encode_hash(block_hash)
            //<< " from [" << authority() << "]";  
        }
    });
    
    return true;
}


// Receive get_blocks sequence.
//-----------------------------------------------------------------------------

bool protocol_block_out::handle_receive_get_blocks(const code& ec,
    get_blocks_const_ptr message)
{
    if (stopped(ec))
        return false;

    const auto size = message->start_hashes().size();

    if (size > max_locator)
    {
        LOG_WARNING(LOG_NODE)
            << "Excessive get_blocks locator size ("
            << size << ") from [" << authority() << "]";
        stop(error::channel_stopped);
        return false;
    }

    if (size > locator_limit())
    {
        LOG_DEBUG(LOG_NODE)
            << "Disallowed get_blocks locator size ("
            << size << ") from [" << authority() << "]";
        return true;
    }

    // Allow a peer to sync despite our being stale.
    ////if (chain_.is_stale())
    ////    return true;

    const auto threshold = last_locator_top_.load();

    chain_.fetch_locator_block_hashes(message, threshold, max_get_blocks,
        BIND2(handle_fetch_locator_hashes, _1, _2));
    return true;
}

void protocol_block_out::handle_fetch_locator_hashes(const code& ec,
    inventory_ptr message)
{
    if (stopped(ec))
        return;

    if (ec)
    {
        LOG_ERROR(LOG_NODE)
            << "Internal failure locating locator block hashes for ["
            << authority() << "] " << ec.message();
        stop(ec);
        return;
    }

    if (message->inventories().empty())
        return;

    // Allow a peer to sync despite our being stale.
    ////if (chain_.is_stale())
    ////    return;

    // Respond to get_blocks with inventory.
    SEND2(*message, handle_send, _1, message->command);

    // Save the locator top to limit an overlapping future request.
    last_locator_top_.store(message->inventories().front().hash());
}



// Receive get_data sequence.
//-----------------------------------------------------------------------------

// TODO: move compact_block to derived class protocol_block_out_70014.
// TODO: move filtered_block to derived class protocol_block_out_70001.
bool protocol_block_out::handle_receive_get_data(const code& ec,
    get_data_const_ptr message)
{
    if (stopped(ec))
        return false;

    // TODO: consider rejecting the message for duplicated entries.
    if (message->inventories().size() > max_get_data)
    {
        LOG_WARNING(LOG_NODE)
            << "Invalid get_data size (" << message->inventories().size()
            << ") from [" << authority() << "]";
        stop(error::channel_stopped);
        return false;
    }

    // Allow a peer to sync despite our being stale.
    ////if (chain_.is_stale())
    ////    return true;

    // Create a copy because message is const because it is shared.
    const auto& inventories = message->inventories();
    const auto response = std::make_shared<inventory>();

    // TODO: convert all compact_block elements to block unless block is,
    // "recently announced and ... close to the tip of the best chain".
    // Close to the tip could be the tip itself with recent based on timestamp.
    // Peer may request compact only after receipt of a send_compact message.

    // Reverse copy the block elements of the const inventory.
    for (auto it = inventories.rbegin(); it != inventories.rend(); ++it)
        if (it->is_block_type())
            response->inventories().push_back(*it);

    send_next_data(response);
    return true;
}

void protocol_block_out::send_next_data(inventory_ptr inventory)
{
    if (inventory->inventories().empty())
        return;

    // The order is reversed so that we can pop from the back.
    const auto& entry = inventory->inventories().back();

    switch (entry.type())
    {
        case inventory::type_id::block:
        {
            chain_.fetch_block(entry.hash(),
                BIND4(send_block, _1, _2, _3, inventory));
            break;
        }
        case inventory::type_id::filtered_block:
        {
            chain_.fetch_merkle_block(entry.hash(),
                BIND4(send_merkle_block, _1, _2, _3, inventory));
            break;
        }
        case inventory::type_id::compact_block:
        {
            chain_.fetch_compact_block(entry.hash(),
                BIND4(send_compact_block, _1, _2, _3, inventory));
            break;
        }
        default:
        {
            BITCOIN_ASSERT_MSG(false, "improperly-filtered inventory");
        }
    }
}

void protocol_block_out::send_block(const code& ec, block_const_ptr message,
    uint64_t, inventory_ptr inventory)
{
    if (stopped(ec))
        return;

    if (ec == error::not_found)
    {
        LOG_DEBUG(LOG_NODE)
            << "Block requested by [" << authority() << "] not found.";

        // TODO: move not_found to derived class protocol_block_out_70001.
        BITCOIN_ASSERT(!inventory->inventories().empty());
        const not_found reply{ inventory->inventories().back() };
        SEND2(reply, handle_send, _1, reply.command);
        handle_send_next(error::success, inventory);
        return;
    }

    if (ec)
    {
        LOG_ERROR(LOG_NODE)
            << "Internal failure locating block requested by ["
            << authority() << "] " << ec.message();
        stop(ec);
        return;
    }

    SEND2(*message, handle_send_next, _1, inventory);
}

// TODO: move merkle_block to derived class protocol_block_out_70001.
void protocol_block_out::send_merkle_block(const code& ec,
    merkle_block_const_ptr message, uint64_t, inventory_ptr inventory)
{
    if (stopped(ec))
        return;

    if (ec == error::not_found)
    {
        LOG_DEBUG(LOG_NODE)
            << "Merkle block requested by [" << authority() << "] not found.";

        // TODO: move not_found to derived class protocol_block_out_70001.
        BITCOIN_ASSERT(!inventory->inventories().empty());
        const not_found reply{ inventory->inventories().back() };
        SEND2(reply, handle_send, _1, reply.command);
        handle_send_next(error::success, inventory);
        return;
    }

    if (ec)
    {
        LOG_ERROR(LOG_NODE)
            << "Internal failure locating merkle block requested by ["
            << authority() << "] " << ec.message();
        stop(ec);
        return;
    }

    SEND2(*message, handle_send_next, _1, inventory);
}

// TODO: move compact_block to derived class protocol_block_out_70014.
void protocol_block_out::send_compact_block(const code& ec,
    compact_block_const_ptr message, uint64_t, inventory_ptr inventory)
{
    if (stopped(ec))
        return;

    if (ec == error::not_found)
    {
        LOG_DEBUG(LOG_NODE)
            << "Compact block requested by [" << authority() << "] not found.";

        // TODO: move not_found to derived class protocol_block_out_70001.
        BITCOIN_ASSERT(!inventory->inventories().empty());
        const not_found reply{ inventory->inventories().back() };
        SEND2(reply, handle_send, _1, reply.command);
        handle_send_next(error::success, inventory);
        return;
    }

    if (ec)
    {
        LOG_ERROR(LOG_NODE)
            << "Internal failure locating compact block requested by ["
            << authority() << "] " << ec.message();
        stop(ec);
        return;
    }

    SEND2(*message, handle_send_next, _1, inventory);
}

void protocol_block_out::handle_send_next(const code& ec,
    inventory_ptr inventory)
{
    if (stopped(ec))
        return;

    BITCOIN_ASSERT(!inventory->inventories().empty());
    inventory->inventories().pop_back();

    // Break off recursion.
    DISPATCH_CONCURRENT1(send_next_data, inventory);
}

// Subscription.
//-----------------------------------------------------------------------------

// TODO: add consideration for catch-up, where we may not want to announce.
// We never announce or inventory an orphan, only indexed blocks.
bool protocol_block_out::handle_reorganized(code ec, size_t fork_height,
    block_const_ptr_list_const_ptr incoming, block_const_ptr_list_const_ptr)
{
    if (stopped(ec))
        return false;

    if (ec)
    {
        LOG_ERROR(LOG_NODE)
            << "Failure handling reorganization: " << ec.message();
        stop(ec);
        return false;
    }

    // Nothing to do, a channel is stopping but it's not this one.
    if (!incoming || incoming->empty())
        return true;

    // Do not announce blocks to peer if too far behind.
    if (chain_.is_stale())
        return true;

    //LOG_DEBUG(LOG_NODE)
    //<< "protocol_block_out::handle_reorganized ["
    //<< "] to [" << authority() << "].";

    // TODO: consider always sending the last block as compact if enabled.
    if (compact_to_peer_ && compact_high_bandwidth_ && incoming->size() == 1)
    {
        // TODO: move compact_block to a derived class protocol_block_in_70014.
        const auto block = incoming->front();

        if (block->validation.originator != nonce())
        {
            compact_block announce = compact_block::factory_from_block(*block);  
            
            SEND2(announce, handle_send, _1, announce.command);

            //LOG_DEBUG(LOG_NODE)
            //<< "protocol_block_out::handle_reorganized 5 ["
            //<< "] to [" << authority() << "].";
        }

        return true;
    }
    else if (headers_to_peer_)
    {
        // TODO: move headers to a derived class protocol_block_in_70012.
        headers announce;

        for (const auto block: *incoming)
            if (block->validation.originator != nonce())
                announce.elements().push_back(block->header());

        if (!announce.elements().empty())
        {
            
             //LOG_DEBUG(LOG_NODE)
             //<< "protocol_block_out::handle_reorganized 1 [ " << announce.command
             //   << " ] to [" << authority() << "].";
            
            SEND2(announce, handle_send, _1, announce.command);


             //LOG_DEBUG(LOG_NODE)
             //<< "protocol_block_out::handle_reorganized 2 [ " << announce.command
             //   << " ] to [" << authority() << "].";
            

            ////const auto hash = announce.elements().front().hash();
            ////LOG_DEBUG(LOG_NODE)
            ////    << "Announced block header [" << encode_hash(hash)
            ////    << "] to [" << authority() << "].";
        }

        return true;
    }
    else
    {
        inventory announce;

        for (const auto block: *incoming)
            if (block->validation.originator != nonce())
                announce.inventories().push_back(
                    { inventory::type_id::block, block->header().hash() });

        if (!announce.inventories().empty())
        {
            SEND2(announce, handle_send, _1, announce.command);

            //LOG_DEBUG(LOG_NODE)
            // << "protocol_block_out::handle_reorganized 3 [ " << announce.command
            //    << " ] to [" << authority() << "].";
            

            ////const auto hash = announce.inventories().front().hash();
            ////LOG_DEBUG(LOG_NODE)
            ////    << "Announced block inventory [" << encode_hash(hash)
            ////    << "] to [" << authority() << "].";
        }

        return true;
    }
}

void protocol_block_out::handle_stop(const code&)
{
    chain_.unsubscribe();

    LOG_DEBUG(LOG_NETWORK)
        << "Stopped block_out protocol for [" << authority() << "].";
}

// Utility.
//-----------------------------------------------------------------------------

// The locator cannot be longer than allowed by our chain length.
// This is DoS protection, otherwise a peer could tie up our database.
// If we are not synced to near the height of peers then this effectively
// prevents peers from syncing from us. Ideally we should use initial block
// download to get close before enabling the block out protocol in any case.
// Note that always populating a locator from the main chain results in an
// effective limit on reorganization depth. The outer limit is 500 or 2000
// depending on the protocol in use. However the peers starts from the first
// point of agreement, meaning that after the first 10 locator hashes this is
// reduced by the size of the gap. The largest cumulative gap is the sum of
// 2^n for n where n > 1 where the sum is < 500 - 10. So Bitcoin reorganization
// is protocol-limited to depth 256 + 10 = 266, unless nodes grow forks by
// generating fork-relative locators.
size_t protocol_block_out::locator_limit()
{
    const auto height = node_.top_block().height();
    return safe_add(chain::block::locator_size(height), size_t(1));
}

// Threshold:
// The peer threshold prevents a peer from creating an unnecessary backlog
// for itself in the case where it is requesting without having processed
// all of its existing backlog. This also reduces its load on us.
// This could cause a problem during a reorg, where the peer regresses
// and one of its other peers populates the chain back to this level. In
// that case we would not respond but our peer's other peer should.
// Other than a reorg there is no reason for the peer to regress, as
// locators are only useful in walking up the chain.

} // namespace node
} // namespace libbitcoin
