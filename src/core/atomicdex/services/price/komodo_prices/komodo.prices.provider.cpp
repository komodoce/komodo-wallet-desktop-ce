#include <nlohmann/json.hpp>
#include "atomicdex/events/events.hpp"
#include "atomicdex/services/price/komodo_prices/komodo.prices.provider.hpp"
#include "atomicdex/utilities/global.utilities.hpp"

//! Constructor
namespace atomic_dex
{
    komodo_prices_provider::komodo_prices_provider(entt::registry& registry) : system(registry)
    {
        m_clock = std::chrono::high_resolution_clock::now();
        process_update();
    }
} // namespace atomic_dex

//! Private functions
namespace atomic_dex
{
    const komodo_prices::api::komodo_ticker_infos&
    komodo_prices_provider::get_info_answer(const std::string& ticker) const
    {
        std::shared_lock lock(m_market_mutex);
        const auto it = m_market_registry.find(ticker);
        if (it != m_market_registry.cend())
        {
            return it->second;
        }

        // Static fallback container guarantees thread safety and valid references
        static const komodo_prices::api::komodo_ticker_infos fallback_empty{};
        return fallback_empty;
    }

    void
    komodo_prices_provider::process_update(bool fallback)
    {
        atomic_dex::komodo_prices::api::async_market_infos(fallback).then([this, fallback](async::task<t_http_response> previous_task) {
            try
            {
                auto resp = previous_task.get();
                auto body = (resp.extract_string(true).get());

                if (resp.status_code() == 200)
                {
                    nlohmann::json    j = nlohmann::json::parse(body);
                    t_market_registry answer;
                    answer = j.get<t_market_registry>();
                    {
                        std::unique_lock lock(m_market_mutex);
                        m_market_registry = std::move(answer);
                    }
                }
                else
                {
                    SPDLOG_ERROR("resp.status_code is {} in komodo_prices_provider::process_update and body: {}", resp.status_code(), body);
                    if (!fallback)
                    {
                        process_update(true);
                    }
                }
                dispatcher_.enqueue(fiat_rate_updated{.ticker = ""});
            }
            catch (const std::exception& e)
            {
                SPDLOG_ERROR("exception in komodo_prices_provider::process_update with fallback {}: {}", fallback, e.what());
                if (!fallback)
                {
                    process_update(true);
                }
                dispatcher_.enqueue(fiat_rate_updated{.ticker = ""});
            }
        });
    }
} // namespace atomic_dex

//! Public Functions
namespace atomic_dex
{
    void
    komodo_prices_provider::update()
    {
        using namespace std::chrono_literals;
        const auto now = std::chrono::high_resolution_clock::now();
        const auto s   = std::chrono::duration_cast<std::chrono::seconds>(now - m_clock);

        if (s >= 149s)
        {
            process_update();
            m_clock = std::chrono::high_resolution_clock::now();
        }
    }

    std::string
    komodo_prices_provider::get_total_volume(const std::string& ticker) const
    {
        return get_info_answer(ticker).volume24_h;
    }

    nlohmann::json
    komodo_prices_provider::get_ticker_historical(const std::string& ticker) const
    {
        nlohmann::json j = get_info_answer(ticker).sparkline_7_d;
        if (j.is_null())
        {
            j = nlohmann::json::array();
        }
        return j;
    }

    std::string
    komodo_prices_provider::get_change_24h(const std::string& ticker) const
    {
        return get_info_answer(ticker).change_24_h;
    }

    std::string
    komodo_prices_provider::get_rate_conversion(const std::string& ticker) const
    {
        return get_info_answer(ticker).last_price;
    }

    std::string
    komodo_prices_provider::get_price_provider(const std::string& ticker) const
    {
        auto provider = get_info_answer(utils::retrieve_main_ticker(ticker)).price_provider;
        switch (provider)
        {
        case komodo_prices::api::provider::binance:
            return "binance";
        case komodo_prices::api::provider::coingecko:
            return "coingecko";
        case komodo_prices::api::provider::coinpaprika:
            return "coinpaprika";
        case komodo_prices::api::provider::forex:
            return "forex";
        case komodo_prices::api::provider::livecoinwatch:
            return "livecoinwatch";
        default:
            return "unknown";
        }
    }

    int64_t
    komodo_prices_provider::get_last_price_timestamp(const std::string& ticker) const
    {
        return get_info_answer(ticker).last_updated_timestamp;
    }
} // namespace atomic_dex
