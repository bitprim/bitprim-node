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
#ifndef BITPRIM_NODE_KEOKEN_MANAGER_HPP_
#define BITPRIM_NODE_KEOKEN_MANAGER_HPP_

#include <functional>

#include <bitcoin/bitcoin/message/messages.hpp>
#include <bitcoin/bitcoin/wallet/payment_address.hpp>
#include <bitcoin/blockchain/interface/block_chain.hpp>
#include <bitcoin/node/define.hpp>

#include <bitprim/keoken/interpreter.hpp>

// #include <bitprim/keoken/state.hpp>
#include <bitprim/keoken/state_delegated.hpp>

namespace bitprim {
namespace keoken {

// using keoken_state_create_asset_t = void (*)
//                 (char const* /*asset_name*/, 
//                 keoken_amount_t /*asset_amount*/, 
//                 payment_address_t /*owner*/, 
//                 uint64_t /*size_t*/ /*block_height*/, 
//                 hast_t /*txid*/);

// using keoken_state_create_balance_entry_t = void (*)
//                 (keoken_asset_id_t /*asset_id*/, 
//                 keoken_amount_t /*asset_amount*/, 
//                 payment_address_t /*source*/, 
//                 payment_address_t /*target*/,  
//                 uint64_t /*size_t*/ /*block_height*/, 
//                 hast_t /*txid*/);

// using keoken_state_asset_id_exists_t = void (*)
//                 (keoken_asset_id_t /*id*/);

// using keoken_state_get_balance_t = void (*)
//                 (keoken_asset_id_t /*id*/, 
//                 payment_address_t /*addr*/);

// using keoken_state_get_assets_by_address_t = void (*)
//                 (payment_address_t /*addr*/);

// using keoken_state_get_assets_t = void (*)(void);

// using keoken_state_get_all_asset_addresses_t = void (*)(void);



class BCN_API manager {
public:
    using state_t = state_delegated;

    using get_assets_by_address_list = state_t::get_assets_by_address_list;
    using get_assets_list = state_t::get_assets_list;
    using get_all_asset_addresses_list = state_t::get_all_asset_addresses_list;

    explicit
    manager(bc::blockchain::block_chain& chain, size_t keoken_genesis_height);

    // non-copyable class
    manager(manager const&) = delete;
    manager& operator=(manager const&) = delete;

    // Commands
    // void configure(create_asset_func create_asset, create_balance_entry_func create_balance_entry
    //                 , asset_id_exists_func asset_id_exists, get_balance_func get_balance
    //                 , get_assets_by_address_func get_assets_by_address, get_assets_func get_assets
    //                 , get_all_asset_addresses_func get_all_asset_addresses);

    void configure_state(state_t::set_initial_asset_id_func set_initial_asset_id, 
                         state_t::asset_id_exists_func asset_id_exists);

    void initialize_from_blockchain();

    // Queries
    bool initialized() const;
    get_assets_by_address_list get_assets_by_address(bc::wallet::payment_address const& addr) const;
    get_assets_list get_assets() const;
    get_all_asset_addresses_list get_all_asset_addresses() const;

private:
    void initialize_from_blockchain(size_t from_height, size_t to_height);
    // void initialize_from_blockchain(size_t from_height);
    void for_each_transaction_non_coinbase(size_t height, bc::chain::block const& block);
    bool handle_reorganized(bc::code ec, size_t fork_height, bc::block_const_ptr_list_const_ptr const& incoming, bc::block_const_ptr_list_const_ptr const& outgoing);

    size_t keoken_genesis_height_;
    size_t processed_height_;
    bool initialized_;
    state_t state_;
    bc::blockchain::block_chain& chain_;
    interpreter<state_t, libbitcoin::blockchain::block_chain> interpreter_;
};

} // namespace keoken
} // namespace bitprim

#endif //BITPRIM_NODE_KEOKEN_MANAGER_HPP_
