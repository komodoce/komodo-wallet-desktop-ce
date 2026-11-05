/******************************************************************************
 * Copyright © 2013-2024 The Komodo Platform Developers.                      *
 *                                                                            *
 * See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
 * the top-level directory of this distribution for the individual copyright  *
 * holder information and the developer policies on copyright and licensing.  *
 *                                                                            *
 * Unless otherwise agreed in a custom licensing agreement, no part of the    *
 * Komodo Platform software, including this file may be copied, modified,     *
 * propagated or distributed except according to the terms contained in the   *
 * LICENSE file                                                               *
 *                                                                            *
 * Removal or modification of this copyright notice is prohibited.            *
 *                                                                            *
 ******************************************************************************/

#pragma once

#include <nlohmann/json.hpp>
#include "atomicdex/utilities/global.utilities.hpp"
#include "atomicdex/version/version.hpp"
#include "atomicdex/constants/dex.constants.hpp"

namespace atomic_dex
{
    using nlohmann::json;
    using namespace std::string_literals;

    struct kdf_config
    {
        std::string              gui{std::string(DEX_NAME) + " "s + atomic_dex::get_version()};
        int64_t                  netid{6133};
        int64_t                  rpcport{atomic_dex::g_dex_rpcport};
        std::vector<std::string> seednodes{"seed02.cipig.net", "kdfseed1.decker.im", "staking1.gleec.com", "staking2.gleec.com", "seed01.swapscope.net", "seed03.kmdefi.net"};
#ifdef _WIN32
        std::string              userhome{utils::u8string(std::filesystem::path(_wgetenv(L"HOMEPATH")))};
#else
        std::string              userhome{std::getenv("HOME")};
#endif
        std::string              passphrase;
        std::string              dbdir{utils::u8string((utils::get_atomic_dex_data_folder() / "kdf" / "DB"))};
        std::string              rpc_password{"atomic_dex_kdf_passphrase"};
        int64_t                  metrics_interval{43200};
    };

    void from_json(const json& j, kdf_config& cfg);
    void to_json(json& j, const kdf_config& cfg);

    inline void
    from_json(const json& j, kdf_config& cfg)
    {
        cfg.gui              = j.at("gui").get<std::string>();
        cfg.netid            = j.at("netid").get<int64_t>();
        cfg.rpcport          = j.at("rpcport").get<int64_t>();
        cfg.userhome         = j.at("userhome").get<std::string>();
        cfg.passphrase       = j.at("passphrase").get<std::string>();
        cfg.rpc_password     = j.at("rpc_password").get<std::string>();
        cfg.dbdir            = j.at("dbdir").get<std::string>();
        cfg.metrics_interval = j.at("metrics_interval").get<int64_t>();
    }

    inline void
    to_json(json& j, const kdf_config& cfg)
    {
        j                     = json::object();
        j["gui"]              = cfg.gui;
        j["netid"]            = cfg.netid;
        j["rpcport"]          = cfg.rpcport;
        j["userhome"]         = cfg.userhome;
        j["passphrase"]       = cfg.passphrase;
        j["rpc_password"]     = cfg.rpc_password;
        j["dbdir"]            = cfg.dbdir;
        j["metrics_interval"] = cfg.metrics_interval;
        if (not cfg.seednodes.empty())
        {
            j["seednodes"]    = cfg.seednodes;
        }
    }
} // namespace atomic_dex
