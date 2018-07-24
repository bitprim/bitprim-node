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
#include <bitprim/keoken/manager.hpp>

namespace bitprim {
namespace keoken {

using namespace bc;
using bc::wallet::payment_address;

manager::manager(libbitcoin::blockchain::block_chain& chain, size_t keoken_genesis_height) 
    : keoken_genesis_height_(keoken_genesis_height)
    , chain_(chain)
    , interpreter_(chain_, state_)
{}

void manager::initialize_from_blockchain(size_t from_height, size_t to_height) {
    bool witness = false;   //TODO(fernando): what to do with this...

    chain_.for_each_transaction_non_coinbase(from_height, to_height, witness, 
        [this](code const& ec, size_t height, chain::transaction const& tx) {
            if (ec == error::success) {
                interpreter_.process(height, tx);
            }
        }
    );
}

void manager::initialize_from_blockchain() {
    size_t top_height;

    if ( ! chain_.get_last_height(top_height)) {
        return; //TODO(fernando): qué hacemos acá?
    }

    initialize_from_blockchain(keoken_genesis_height_, top_height);
}

// Queries
manager::get_assets_by_address_list manager::get_assets_by_address(payment_address const& addr) const {
    // return get_assets_by_address_list{};


    // 1. consultar assets para 1 address
    /*/
        input: addr
        out  : lista de 
                    - asset id
                    - asset name
                    - addr is owner of the asset
                    - saldo
    */    
}

manager::get_assets_list manager::get_assets() const {
    return get_assets_list{};
}

manager::get_all_asset_addresses_list manager::get_all_asset_addresses() const {
    return get_all_asset_addresses_list{};
}


} // namespace keoken
} // namespace bitprim
