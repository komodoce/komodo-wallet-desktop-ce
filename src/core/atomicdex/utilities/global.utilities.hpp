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

#include <vector>
#include <filesystem>
#include <date/date.h>           ///< date::format
#include <date/tz.h>             ///< date::make_zoned
#include <entt/config/config.h> ///< ENTT_API
#include "safe.float.hpp"
#include "atomicdex/config/coins.cfg.hpp"
#include "log.prerequisites.hpp"

namespace atomic_dex::utils
{
    //! Float numbers helpers
    std::string get_formated_float(t_float_50 value);
    std::string adjust_precision(const std::string& current);
    std::string format_float(t_float_50 value);
    std::string extract_large_float(const std::string& current);

    //! Fs helpers
    bool        create_if_doesnt_exist(const std::filesystem::path& path);
    std::string u8string(const std::filesystem::path& path);
    //std::string u8string(const std::wstring& p);
    //std::string wstring_to_utf8(const std::wstring& str);
    //std::string to_utf8(const wchar_t* w);

    double determine_balance_factor(bool with_pin_cfg);

    //! The local time zone, resolved once. current_zone() loads the whole
    //! timezone database on first use and retries that load on every call
    //! for as long as it keeps failing -- which made every formatted date
    //! (thousands per minute on the orders/swaps refresh) repeat the failed
    //! load. nullptr when it cannot be determined: dates are then in UTC.
    inline const date::time_zone*
    local_time_zone()
    {
        static const date::time_zone* const zone = []() -> const date::time_zone*
        {
            try
            {
                return date::current_zone();
            }
            catch (const std::exception& error)
            {
                SPDLOG_ERROR("cannot determine the local time zone, dates are shown in UTC: {}", error.what());
                return nullptr;
            }
        }();
        return zone;
    }

    template <typename TimeFormat = std::chrono::milliseconds>
    inline std::string
    to_human_date(std::size_t timestamp, std::string format)
    {
        using namespace date;

        const sys_time<TimeFormat> tp{TimeFormat{timestamp}};

        if (const auto* zone = local_time_zone(); zone != nullptr)
        {
            try
            {
                return date::format(format, date::make_zoned(zone, tp));
            }
            catch (const std::exception& error)
            {
                SPDLOG_ERROR("exception in to_human_date: {}", error.what());
            }
        }
        return date::format(std::move(format), tp);
    }

    ENTT_API std::filesystem::path get_atomic_dex_data_folder();

    /// \brief  Gets the path where addressbook configs are stored.
    /// \return An std::filesystem::path object.
    [[nodiscard]] std::filesystem::path get_atomic_dex_addressbook_folder();

    std::filesystem::path get_atomic_dex_logs_folder() ;

    ENTT_API std::filesystem::path get_atomic_dex_current_log_file();

    ENTT_API std::filesystem::path get_current_configs_path();

    std::filesystem::path get_kdf_atomic_dex_current_log_file();

    std::filesystem::path get_atomic_dex_config_folder();

    nlohmann::json read_json_file(std::filesystem::path filepath);

    //std::string minimal_trade_amount_str();

    //const t_float_50 minimal_trade_amount();

    std::filesystem::path get_atomic_dex_export_folder();

    std::filesystem::path get_atomic_dex_current_export_recent_swaps_file();

    ENTT_API std::filesystem::path get_themes_path();
    ENTT_API std::filesystem::path get_logo_path();

    std::string retrieve_main_ticker(const std::string& ticker, bool segwit_only=false, bool exclude_segwit=false);

    void to_eth_checksum(std::string& address);
    //void json_keys(nlohmann::json j);
    int8_t get_index_str(std::vector<std::string> vec, std::string val);
    std::vector<std::string> coin_cfg_to_ticker_cfg(std::vector<coin_config_t> in);

    [[nodiscard]] std::string safe_json_string(const std::optional<nlohmann::json>& json_obj, const std::string& key);

} // namespace atomic_dex::utils
