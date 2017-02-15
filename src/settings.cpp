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
#include <bitcoin/node/settings.hpp>

namespace libbitcoin {
namespace node {

settings::settings()
  : sync_peers(0),
    sync_timeout_seconds(5),
    block_poll_seconds(1),
    refresh_transactions(true)
{
}

// There are no current distinctions spanning chain contexts.
settings::settings(config::settings context)
  : settings()
{
}

} // namespace node
} // namespace libbitcoin
