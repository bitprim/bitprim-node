/**
 * Copyright (c) 2016-2018 Bitprim Inc.
 *
 * This file is part of Bitprim.
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
#include <bitprim/keoken/manager.hpp>

namespace bitprim {
namespace keoken {

using namespace bc;
using bc::wallet::payment_address;
using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;
using std::placeholders::_4;

manager::manager(libbitcoin::blockchain::block_chain& chain, size_t keoken_genesis_height) 
    : keoken_genesis_height_(keoken_genesis_height)
    , chain_(chain)
    , interpreter_(chain_, state_)
{}

//TODO(fernando): change the name
void manager::initialize_from_blockchain(size_t from_height, size_t to_height) {
    bool witness = false;   //TODO(fernando): what to do with this...

    boost::unique_lock<boost::shared_mutex> lock(mutex_);

    chain_.for_each_transaction_non_coinbase(from_height, to_height, witness, 
        [this](code const& ec, size_t height, chain::transaction const& tx) {
            if (ec == error::success) {
                interpreter_.process(height, tx);
            }
        }
    );
}

//TODO(fernando): move to other site
void manager::for_each_transaction_non_coinbase(size_t height, chain::block const& block) {
    boost::unique_lock<boost::shared_mutex> lock(mutex_);
    std::for_each(std::next(block.transactions().begin()), block.transactions().end(), [this, height](chain::transaction const& tx) {
        interpreter_.process(height, tx);
    });
}

void manager::initialize_from_blockchain() {
    size_t top_height;

    if ( ! chain_.get_last_height(top_height)) {
        return; //TODO(fernando): qué hacemos acá?
    }

    initialize_from_blockchain(keoken_genesis_height_, top_height);

    // chain_.subscribe_blockchain(std::bind(&manager::handle_reorganized, this, _1, _2, _3, _4));
}

// // A typical reorganization consists of one incoming and zero outgoing blocks.
// bool manager::handle_reorganized(code ec, size_t fork_height, block_const_ptr_list_const_ptr incoming, block_const_ptr_list_const_ptr outgoing) {
//     if (ec == error::service_stopped) { //stopped() || 
//         return false;
//     }

//     if (ec) {
//         return false;
//     }

//     // Nothing to do here.
//     if ( ! incoming || incoming->empty()) {
//         return true;
//     }

//     for (auto const& block_ptr: *incoming) {
//         for_each_transaction_non_coinbase(fork_height++, *block_ptr);
//     }

//     // for (const auto block: *outgoing)
//     //     LOG_DEBUG(LOG_NODE)
//     //         << "Reorganization moved block to orphan pool ["
//     //         << encode_hash(block->header().hash()) << "]";
//     // const auto height = safe_add(fork_height, incoming->size());
//     // set_top_block({ incoming->back()->hash(), height });


//     return true;
// }


// Queries
manager::get_assets_by_address_list manager::get_assets_by_address(payment_address const& addr) const {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    return state_.get_assets_by_address(addr);
}

manager::get_assets_list manager::get_assets() const {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    return state_.get_assets();
}

manager::get_all_asset_addresses_list manager::get_all_asset_addresses() const {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    return state_.get_all_asset_addresses();
}

} // namespace keoken
} // namespace bitprim
