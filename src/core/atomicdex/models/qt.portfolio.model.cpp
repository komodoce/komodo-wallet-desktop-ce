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

#include <iomanip>
#include <sstream>
#include <QJSValue>
#include "atomicdex/events/qt.events.hpp"
#include "atomicdex/managers/qt.wallet.manager.hpp"
#include "atomicdex/pages/qt.portfolio.page.hpp"
#include "atomicdex/pages/qt.trading.page.hpp"
#include "atomicdex/pages/qt.wallet.page.hpp"
#include "atomicdex/services/price/global.provider.hpp"
#include "atomicdex/services/price/komodo_prices/komodo.prices.provider.hpp"
#include "atomicdex/utilities/global.utilities.hpp"
#include "atomicdex/utilities/qt.utilities.hpp"
#include "qt.portfolio.model.hpp"

namespace atomic_dex
{
    portfolio_model::portfolio_model(ag::ecs::system_manager& system_manager, entt::dispatcher& dispatcher, QObject* parent) :
        QAbstractListModel(parent), m_system_manager(system_manager), m_dispatcher(dispatcher),
        m_model_proxy(new portfolio_proxy_model(m_system_manager, parent))
    {
        m_model_proxy->setSourceModel(this);
        m_model_proxy->setDynamicSortFilter(true);
        m_model_proxy->sort_by_currency_balance(false);
        m_model_proxy->setFilterRole(NameAndTicker);
        m_model_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    }

    QString format_to_precision(const std::string& raw_val, int precision) {
        if (raw_val.empty()) return QString::fromStdString("0." + std::string(precision, '0'));
        try {
            double val = std::stod(raw_val);
            std::stringstream stream;
            stream << std::fixed << std::setprecision(precision) << val;
            return QString::fromStdString(stream.str());
        } catch (...) {
            return QString::fromStdString(raw_val);
        }
    }

    double safe_string_to_double(const std::string& str) {
        if (str.empty()) return 0.0;
        try { return std::stod(str); }
        catch (...) { return 0.0; }
    }

    void
    atomic_dex::portfolio_model::initialize_portfolio(const std::vector<std::string>& tickers)
    {
        QVector<portfolio_data> datas;

        for (auto&& ticker: tickers)
        {
            if (m_ticker_registry.find(ticker) != m_ticker_registry.end())
            {
                // see "already in vector" warning in src/app/app.cpp
                // SPDLOG_WARN("ticker {} already in m_ticker_registry, skipping duplicate", ticker);
                continue;
            }

            const auto& kdf_system    = this->m_system_manager.get_system<kdf_service>();
            const auto& price_service = this->m_system_manager.get_system<global_price_service>();
            const auto& provider      = this->m_system_manager.get_system<komodo_prices_provider>();
            const auto& coin          = kdf_system.get_coin_info(ticker);

            std::error_code ec;
            std::string balance_raw = kdf_system.get_balance_info(coin.ticker, ec);
            std::string fiat_balance_raw = price_service.get_price_in_fiat(m_config->current_currency, coin.ticker, ec);
            std::string price_raw = price_service.get_rate_conversion(m_config->current_currency, coin.ticker, true);
            std::string fiat_price_raw = price_service.get_rate_conversion(m_config->current_fiat, coin.ticker);
            QString     change_24h_raw = retrieve_change_24h(provider, coin, *m_config, m_system_manager);

            portfolio_data  data{
                .ticker                           = QString::fromStdString(coin.ticker),
                .coin_type                        = QString::fromStdString(coin.type),
                .name                             = QString::fromStdString(coin.name),
                .balance                          = format_to_precision(balance_raw, 8),
                .main_currency_balance            = format_to_precision(fiat_balance_raw, 2),
                .change_24h                       = format_to_precision(change_24h_raw.toStdString(), 3),
                .main_currency_price_for_one_unit = format_to_precision(price_raw, 8),
                .main_fiat_price_for_one_unit     = format_to_precision(fiat_price_raw, 2),
                .raw_balance                      = safe_string_to_double(balance_raw),
                .raw_main_currency_balance        = safe_string_to_double(fiat_balance_raw),
                .raw_change_24h                   = safe_string_to_double(change_24h_raw.toStdString()),
                .raw_main_currency_price          = safe_string_to_double(price_raw),
                .trend_7d                         = nlohmann_json_array_to_qt_json_array(provider.get_ticker_historical(coin.ticker)),
                .activation_status                = nlohmann_json_object_to_qt_json_object(coin.activation_status),
                .price_provider                   = QString::fromStdString(provider.get_price_provider(coin.ticker)),
                .price_last_timestamp             = static_cast<int>(provider.get_last_price_timestamp(coin.ticker)),
                .is_excluded                      = false,
                .public_address                   = QString::fromStdString(kdf_system.address(coin.ticker, ec))};

            data.display         = QString::fromStdString(coin.ticker) + " (" + data.balance + ")";
            data.ticker_and_name = QString::fromStdString(coin.ticker) + data.name;
            datas.push_back(std::move(data));
            m_ticker_registry.emplace(ticker);
        }

        if (not datas.isEmpty())
        {
            beginInsertRows(QModelIndex(), this->m_model_data.count(), this->m_model_data.count() + tickers.size() - 1);
            this->m_model_data.append(datas);
            endInsertRows();
            emit lengthChanged();
        }
    }

    bool
    portfolio_model::update_currency_values()
    {
        const auto&        kdf_system    = this->m_system_manager.get_system<kdf_service>();
        const auto&        price_service = this->m_system_manager.get_system<global_price_service>();
        const auto&        provider      = this->m_system_manager.get_system<komodo_prices_provider>();
        const auto         coins         = this->m_system_manager.get_system<portfolio_page>().get_global_cfg()->get_enabled_coins();
        const std::string& currency      = m_config->current_currency;
        const std::string& fiat          = m_config->current_fiat;

        for (auto&& [_, coin]: coins)
        {
            if (m_ticker_registry.find(coin.ticker) == m_ticker_registry.end())
            {
                SPDLOG_WARN("[update_currency_values] ticker: {} not inserted yet in the model, skipping", coin.ticker);
                return false;
            }

            const std::string& ticker = coin.ticker;
            if (const auto res = this->match(this->index(0, 0), TickerRole, QString::fromStdString(ticker), 1, Qt::MatchFlag::MatchExactly);
                not res.isEmpty())

            {
                std::error_code    ec;
                const QModelIndex& idx                         = res.at(0);

                const std::string  main_currency_balance_raw   = price_service.get_price_in_fiat(currency, ticker, ec);
                update_value(MainCurrencyBalanceRole, format_to_precision(main_currency_balance_raw, 2), idx, *this);

                const std::string  currency_price_raw          = price_service.get_rate_conversion(currency, ticker, true);
                update_value(MainCurrencyPriceForOneUnit, format_to_precision(currency_price_raw, 8), idx, *this);

                const std::string  currency_fiat_raw           = price_service.get_rate_conversion(fiat, ticker, false);
                update_value(MainFiatPriceForOneUnit, format_to_precision(currency_fiat_raw, 2), idx, *this);

                const QString      price_provider              = QString::fromStdString(provider.get_price_provider(ticker));
                update_value(PriceProvider, price_provider, idx, *this);

                int last_price_timestamp = static_cast<int>(provider.get_last_price_timestamp(ticker));
                update_value(LastPriceTimestamp, last_price_timestamp, idx, *this);

                QString            change24_h_raw              = retrieve_change_24h(provider, coin, *m_config, m_system_manager);
                update_value(Change24H, format_to_precision(change24_h_raw.toStdString(), 3), idx, *this);

                const std::string  balance_raw                 = kdf_system.get_balance_info(coin.ticker, ec);
                QString            formatted_balance           = format_to_precision(balance_raw, 8);
                auto&& [prev_balance, is_change_b] = update_value(BalanceRole, formatted_balance, idx, *this);

                const QString display = QString::fromStdString(coin.ticker) + " (" + formatted_balance + ")";
                update_value(Display, display, idx, *this);

                if (is_change_b)
                {
                    balance_update_handler(prev_balance.toString(), formatted_balance, QString::fromStdString(ticker));
                }

                QJsonArray trend = nlohmann_json_array_to_qt_json_array(provider.get_ticker_historical(ticker));
                update_value(Trend7D, trend, idx, *this);
                QJsonObject status = nlohmann_json_object_to_qt_json_object(coin.activation_status);
                update_value(ActivationStatus, status, idx, *this);
            }
        }
        return true;
    }

    bool
    portfolio_model::update_balance_values(const std::vector<std::string>& tickers, [[maybe_unused]] utils::caller_location location)
    {
        const auto& kdf_system      = this->m_system_manager.get_system<kdf_service>();
        const auto* global_cfg      = this->m_system_manager.get_system<portfolio_page>().get_global_cfg();
        const auto& price_service   = this->m_system_manager.get_system<global_price_service>();
        const auto& provider        = this->m_system_manager.get_system<komodo_prices_provider>();
        const std::string& currency = m_config->current_currency;
        const std::string& fiat     = m_config->current_fiat;
        const std::string current_active_ticker = kdf_system.get_current_ticker();
        bool active_ticker_changed  = false;

        for (auto&& ticker: tickers)
        {
            if (ticker.empty()) [[unlikely]]
            {
                SPDLOG_WARN("portfolio_model::update_balance_values called with empty ticker by: {} ({}:{})", location.function_name(), location.file_name(), location.line());
                continue;
            }

            if (!m_ticker_registry.contains(ticker))
            {
                SPDLOG_WARN("portfolio_model::update_balance_values | ticker: {} not inserted yet in the model | called by: {} ({}:{})", ticker, location.function_name(), location.file_name(), location.line());
                continue;
            }

            auto it = std::find_if(m_model_data.begin(), m_model_data.end(),
                [&ticker](const portfolio_data& item) { return item.ticker.toStdString() == ticker; });

            if (it != m_model_data.end())
            {
                int row_idx = static_cast<int>(std::distance(m_model_data.begin(), it));
                QModelIndex idx = this->index(row_idx, 0);

                std::error_code ec;
                const std::string balance_raw = kdf_system.get_balance_info(ticker, ec);
                QString formatted_balance     = format_to_precision(balance_raw, 8);

                bool is_balance_altered = (it->balance != formatted_balance);
                QString prev_balance    = it->balance;

                const std::string main_currency_balance_raw = price_service.get_price_in_fiat(currency, ticker, ec);
                QString formatted_fiat_balance              = format_to_precision(main_currency_balance_raw, 2);

                const std::string currency_price_raw = price_service.get_rate_conversion(currency, ticker, true);
                QString formatted_price              = format_to_precision(currency_price_raw, 8);

                it->balance                          = formatted_balance;
                it->main_currency_balance            = formatted_fiat_balance;
                it->main_currency_price_for_one_unit = formatted_price;
                it->main_fiat_price_for_one_unit     = format_to_precision(price_service.get_rate_conversion(fiat, ticker, false), 2);
                it->price_provider                   = QString::fromStdString(provider.get_price_provider(ticker));
                it->price_last_timestamp             = static_cast<int>(provider.get_last_price_timestamp(ticker));
                it->display                          = QString::fromStdString(ticker) + " (" + formatted_balance + ")";
                const auto& coin                     = global_cfg->get_coin_info(ticker);
                it->change_24h                       = format_to_precision(retrieve_change_24h(provider, coin, *m_config, m_system_manager).toStdString(), 3);
                it->trend_7d                         = nlohmann_json_array_to_qt_json_array(provider.get_ticker_historical(ticker));
                it->activation_status                = nlohmann_json_object_to_qt_json_object(coin.activation_status);
                it->raw_balance                      = safe_string_to_double(balance_raw);
                it->raw_main_currency_balance        = safe_string_to_double(main_currency_balance_raw);
                it->raw_main_currency_price          = safe_string_to_double(currency_price_raw);
                it->raw_change_24h                   = safe_string_to_double(it->change_24h.toStdString());

                emit dataChanged(idx, idx);

                if (is_balance_altered)
                {
                    balance_update_handler(prev_balance, formatted_balance, QString::fromStdString(ticker));
                }

                if (ticker == current_active_ticker)
                {
                    active_ticker_changed = true;
                }
            }
        }

        if (active_ticker_changed)
        {
            m_system_manager.get_system<wallet_page>().refresh_ticker_infos();
        }

        return true;
    }

    QVariant
    atomic_dex::portfolio_model::data(const QModelIndex& index, int role) const
    {
        if (!hasIndex(index.row(), index.column(), index.parent()))
        {
            return {};
        }

        const portfolio_data& item = m_model_data.at(index.row());
        switch (static_cast<PortfolioRoles>(role))
        {
        case TickerRole:
            return item.ticker;
        case BalanceRole:
            return item.balance;
        case MainCurrencyBalanceRole:
            return item.main_currency_balance;
        case Change24H:
            return item.change_24h;
        case MainCurrencyPriceForOneUnit:
            return item.main_currency_price_for_one_unit;
        case MainFiatPriceForOneUnit:
            return item.main_fiat_price_for_one_unit;
        case NameRole:
            return item.name;
        case ActivationStatus:
            return item.activation_status;
        case Trend7D:
            return item.trend_7d;
        case Excluded:
            return item.is_excluded;
        case Display:
            return item.display;
        case NameAndTicker:
            return item.ticker_and_name;
        case MultiTickerCurrentlyEnabled:
            return item.is_multi_ticker_enabled;
        case MultiTickerData:
            return item.multi_ticker_data.has_value() ? item.multi_ticker_data.value() : QJsonObject{};
        case CoinType:
            return item.coin_type;
        case MultiTickerError:
            return static_cast<qint32>(item.multi_ticker_error.value_or(TradingError::None));
        case MultiTickerPrice:
            return item.multi_ticker_price.value_or("0");
        case MultiTickerReceiveAmount:
            return item.multi_ticker_receive_amount.value_or("0");
        case MultiTickerFeesInfo:
            return item.multi_ticker_fees_info.value_or(QJsonObject());
        case Address:
            return item.public_address;
        case PrivKey:
            return item.priv_key;
        case PercentMainCurrency:
            return item.percent_main_currency;
        case PriceProvider:
            return item.price_provider;
        case LastPriceTimestamp:
            return item.price_last_timestamp;
        case RawMainCurrencyPriceRole:
            return safe_string_to_double(item.main_currency_price_for_one_unit.toStdString());
        case RawBalanceRole:
            return safe_string_to_double(item.balance.toStdString());
        case RawMainCurrencyBalanceRole:
            return safe_string_to_double(item.main_currency_balance.toStdString());
        case RawChange24HRole:
            return safe_string_to_double(item.change_24h.toStdString());
        }
        return {};
    }

    bool
    atomic_dex::portfolio_model::setData(const QModelIndex& index, const QVariant& value, int role)
    {
        if (!hasIndex(index.row(), index.column(), index.parent()) || !value.isValid())
        {
            return false;
        }

        portfolio_data& item = m_model_data[index.row()];
        switch (static_cast<PortfolioRoles>(role))
        {
        case CoinType:
            item.coin_type = value.toString();
            break;
        case BalanceRole:
            item.balance = value.toString();
            break;
        case MainCurrencyBalanceRole:
        {
            item.main_currency_balance = value.toString();
            break;
        }
        case Change24H:
            item.change_24h = value.toString();
            break;
        case MainCurrencyPriceForOneUnit:
            item.main_currency_price_for_one_unit = value.toString();
            break;
        case MainFiatPriceForOneUnit:
            item.main_fiat_price_for_one_unit = value.toString();
            break;
        case Trend7D:
            item.trend_7d = value.toJsonArray();
            break;
        case ActivationStatus:
            item.activation_status = value.toJsonObject();
            break;
        case Excluded:
            item.is_excluded = value.toBool();
            break;
        case Display:
            item.display = value.toString();
            break;
        case NameAndTicker:
            item.ticker_and_name = value.toString();
            break;
        case MultiTickerCurrentlyEnabled:
            if (item.is_multi_ticker_enabled != value.toBool())
            {
                item.is_multi_ticker_enabled = value.toBool();
                if (item.is_multi_ticker_enabled == true)
                {
                    this->m_dispatcher.trigger(multi_ticker_enabled{.ticker = item.ticker});
                }
            }
            break;
        case MultiTickerData:
            item.multi_ticker_data = QJsonObject::fromVariantMap(value.value<QVariantMap>());
            break;
        case MultiTickerError:
            item.multi_ticker_error = static_cast<TradingError>(value.toInt());
            break;
        case MultiTickerPrice:
        {
            item.multi_ticker_price = value.toString();
            break;
        }
        case MultiTickerReceiveAmount:
            item.multi_ticker_receive_amount = value.toString();
            break;
        case MultiTickerFeesInfo:
            item.multi_ticker_fees_info = QJsonObject::fromVariantMap(value.value<QVariantMap>());
            break;
        case Address:
            item.public_address = value.toString();
            break;
        case PrivKey:
            item.priv_key = value.toString();
            break;
        case PercentMainCurrency:
            item.percent_main_currency = value.toString();
            break;
        case PriceProvider:
            item.price_provider = value.toString();
            break;
        case LastPriceTimestamp:
            item.price_last_timestamp = value.toInt();
            break;
        case RawBalanceRole:
            item.raw_balance = value.toDouble();
            break;
        case RawMainCurrencyBalanceRole:
            item.raw_main_currency_balance = value.toDouble();
            break;
        case RawChange24HRole:
            item.raw_change_24h = value.toDouble();
            break;
        case RawMainCurrencyPriceRole:
            item.raw_main_currency_price = value.toDouble();
            break;
        default:
            return false;
        }

        emit dataChanged(index, index, {role});
        return true;
    }

    QString
    portfolio_model::coin_balance(QString coin)
    {
        auto res = this->match(this->index(0, 0), TickerRole, coin, 1, Qt::MatchFlag::MatchExactly);

        if (not res.empty())
        {
            return QString(this->data(res.at(0), BalanceRole).toString());
        } else {
            SPDLOG_ERROR("res.empty in portfolio_model::coin_balance");
        }
        return "0";
    }

    void
    portfolio_model::disable_coins(const QStringList& coins)
    {
        for (auto&& coin: coins)
        {
            this->m_ticker_registry.erase(coin.toStdString());
            auto res = this->match(this->index(0, 0), TickerRole, coin, 1, Qt::MatchFlag::MatchExactly);

            if (not res.empty())
            {
                int row = res.at(0).row();
                this->beginRemoveRows(QModelIndex(), row, row);
                this->m_model_data.removeAt(row);
                this->endRemoveRows();
            }
            else {
                SPDLOG_ERROR("res.empty in portfolio_model::disable_coins for coin: {}", coin.toStdString());
            }
        }
        emit lengthChanged();
        m_model_proxy->reset();
    }

    int
    atomic_dex::portfolio_model::rowCount([[maybe_unused]] const QModelIndex& parent) const
    {
        return this->m_model_data.count();
    }

    QHash<int, QByteArray>
    portfolio_model::roleNames() const
    {
        return {
            {TickerRole, "ticker"},
            {CoinType, "type"},
            {NameRole, "name"},
            {BalanceRole, "balance"},
            {MainCurrencyBalanceRole, "main_currency_balance"},
            {Change24H, "change_24h"},
            {MainCurrencyPriceForOneUnit, "main_currency_price_for_one_unit"},
            {MainFiatPriceForOneUnit, "main_fiat_price_for_one_unit"},
            {Trend7D, "trend_7d"},
            {ActivationStatus, "activation_status"},
            {Excluded, "excluded"},
            {Display, "display"},
            {NameAndTicker, "name_and_ticker"},
            {MultiTickerCurrentlyEnabled, "is_multi_ticker_currently_enabled"},
            {MultiTickerData, "multi_ticker_data"},
            {MultiTickerPrice, "multi_ticker_price"},
            {MultiTickerError, "multi_ticker_error"},
            {MultiTickerReceiveAmount, "multi_ticker_receive_amount"},
            {MultiTickerFeesInfo, "multi_ticker_fees_info"},
            {Address, "public_address"},
            {PrivKey, "priv_key"},
            {PercentMainCurrency, "percent_main_currency"},
            {LastPriceTimestamp, "lastPriceTimestamp"},
            {PriceProvider, "priceProvider"}};
    }

    portfolio_proxy_model*
    atomic_dex::portfolio_model::get_portfolio_proxy_mdl() const
    {
        return m_model_proxy;
    }

    int
    portfolio_model::get_length() const
    {
        return this->rowCount(QModelIndex());
    }

    void
    portfolio_model::set_cfg(cfg& cfg)
    {
        m_config = &cfg;
    }

    void
    portfolio_model::reset()
    {
        this->beginResetModel();
        this->m_ticker_registry.clear();
        this->m_model_data.clear();
        this->endResetModel();
    }

    portfolio_model::t_portfolio_datas
    portfolio_model::get_underlying_data() const
    {
        return m_model_data;
    }

    void
    portfolio_model::clean_priv_keys()
    {
        const auto coins = this->m_system_manager.get_system<portfolio_page>().get_global_cfg()->get_enabled_coins();

        for (auto&& [coin, cfg]: coins)
        {
            auto res = this->match(this->index(0, 0), TickerRole, QString::fromStdString(coin), 1, Qt::MatchFlag::MatchExactly);

            if (not res.empty())
            {
                update_value(PortfolioRoles::PrivKey, "", res.at(0), *this);
            } else {
                SPDLOG_ERROR("res.empty in portfolio_model::clean_priv_keys");
            }
        }
    }
} // namespace atomic_dex

namespace atomic_dex
{
    void
    portfolio_model::balance_update_handler(const QString& prev_balance, const QString& new_balance, const QString& ticker)
    {
        using namespace std::chrono;
        t_float_50 prev_balance_f = safe_float(prev_balance.toStdString());
        t_float_50 new_balance_f  = safe_float(new_balance.toStdString());
        bool       am_i_sender    = false;
        if (prev_balance_f > new_balance_f)
        {
            am_i_sender = true;
        }
        t_float_50 amount_f   = am_i_sender ? prev_balance_f - new_balance_f : new_balance_f - prev_balance_f;
        QString    amount     = QString::fromStdString(amount_f.str(8, std::ios_base::fixed));
        qint64     timestamp  = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
        QString    human_date = QString::fromStdString(utils::to_human_date<std::chrono::seconds>(timestamp, "%e %b %Y, %H:%M"));

        if (amount_f > 0.0)
        {
            this->m_dispatcher.enqueue(balance_update_notification{.am_i_sender = am_i_sender,
                                                                   .amount = amount,
                                                                   .ticker = ticker,
                                                                   .human_date = human_date,
                                                                   .timestamp = timestamp});
        }
        emit portfolioItemDataChanged();
    }

    void
    portfolio_model::adjust_percent_current_currency(QString balance_all)
    {
        const auto coins = this->m_system_manager.get_system<portfolio_page>().get_global_cfg()->get_enabled_coins();
        for (auto&& [coin, cfg]: coins)
        {
            auto res = this->match(this->index(0, 0), TickerRole, QString::fromStdString(coin), 1, Qt::MatchFlag::MatchExactly);
            if (not res.empty())
            {
                t_float_50 balance_all_f         = safe_float(balance_all.toStdString());
                t_float_50 main_currency_balance = safe_float(this->data(res.at(0), MainCurrencyBalanceRole).toString().toStdString());
                if (balance_all_f > 0 && main_currency_balance > 0)
                {
                    t_float_50 res_f   = (100 * main_currency_balance) / balance_all_f;
                    auto       percent = QString::fromStdString(res_f.str(2, std::ios::fixed));
                    update_value(PortfolioRoles::PercentMainCurrency, percent, res.at(0), *this);
                }
            } else {
                SPDLOG_ERROR("res.empty in portfolio_model::adjust_percent_current_currency");
            }
        }
    }
} // namespace atomic_dex
