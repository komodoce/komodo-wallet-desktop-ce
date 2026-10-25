#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QrCode.hpp>
#include "atomicdex/api/faucet/faucet.hpp"
#include "atomicdex/api/kdf/rpc_v1/rpc.convertaddress.hpp"
#include "atomicdex/api/kdf/rpc_v1/rpc.electrum.hpp"
#include "atomicdex/api/kdf/rpc_v1/rpc.validateaddress.hpp"
#include "atomicdex/api/kdf/rpc_v2/rpc2.withdraw.hpp"
#include "atomicdex/api/kdf/rpc_v2/rpc2.task.withdraw.init.hpp"
#include "atomicdex/api/kdf/rpc_v2/rpc2.task.withdraw.status.hpp"
#include "atomicdex/services/kdf/kdf.service.hpp"
#include "atomicdex/services/price/global.provider.hpp"
#include "atomicdex/services/price/komodo_prices/komodo.prices.provider.hpp"
#include "atomicdex/utilities/qt.utilities.hpp"
#include "qt.portfolio.page.hpp"
#include "qt.settings.page.hpp"
#include "qt.wallet.page.hpp"

namespace atomic_dex
{
    wallet_page::wallet_page(entt::registry& registry, ag::ecs::system_manager& system_manager, QObject* parent) :
        QObject(parent), system(registry), m_system_manager(system_manager), m_transactions_mdl(new transactions_model(system_manager, this))
    {
        this->dispatcher_.sink<tx_fetch_finished>().connect<&wallet_page::on_tx_fetch_finished>(*this);
    }

    void
    wallet_page::update()
    {}
} // namespace atomic_dex

//! Private API
namespace atomic_dex
{
    void
    wallet_page::check_send_availability()
    {
        auto& kdf                  = m_system_manager.get_system<kdf_service>();
        std::string current_ticker = kdf.get_current_ticker();

        if (current_ticker.empty())
        {
            m_send_available                   = false;
            m_send_availability_state          = "";
            m_current_ticker_fees_coin_enabled = false;

            emit sendAvailableChanged();
            emit sendAvailabilityStateChanged();
            emit currentTickerFeesCoinEnabledChanged();
            return;
        }

        auto  global_coins_cfg = m_system_manager.get_system<portfolio_page>().get_global_cfg();
        const auto& ticker_info = global_coins_cfg->get_coin_info(current_ticker);

        m_send_available                   = true;
        m_send_availability_state          = "";
        m_current_ticker_fees_coin_enabled = true;

        if (not kdf.get_balance_info_f(ticker_info.ticker) > 0)
        {
            m_send_available                   = false;
            m_send_availability_state          = tr("You do not have enough funds.");
            m_current_ticker_fees_coin_enabled = true;
        }
        else if (ticker_info.has_parent_fees_ticker)
        {
            const auto& parent_ticker_info = global_coins_cfg->get_coin_info(ticker_info.fees_ticker);

            if (!parent_ticker_info.currently_enabled)
            {
                m_send_available = true;
                m_send_availability_state =
                    tr("%1 is not activated: click on the button to enable it or enable it manually").arg(QString::fromStdString(parent_ticker_info.ticker));
                m_current_ticker_fees_coin_enabled = false;
            }
            else if (not kdf.get_balance_info_f(parent_ticker_info.ticker) > 0)
            {
                m_send_available          = false;
                m_send_availability_state = tr("You need to have %1 to pay the gas for %2 transactions.")
                                                .arg(QString::fromStdString(parent_ticker_info.ticker))
                                                .arg(QString::fromStdString(parent_ticker_info.type));
                m_current_ticker_fees_coin_enabled = true;
            }
        }

        emit sendAvailableChanged();
        emit sendAvailabilityStateChanged();
        emit currentTickerFeesCoinEnabledChanged();
    }
} // namespace atomic_dex

//! Getters/Setters
namespace atomic_dex
{
    QString wallet_page::get_current_ticker() const
    {
        const auto& kdf_system = m_system_manager.get_system<kdf_service>();
        return QString::fromStdString(kdf_system.get_current_ticker());
    }

    void wallet_page::set_current_ticker(const QString& ticker, bool force, [[maybe_unused]] utils::caller_location location)
    {
        //SPDLOG_DEBUG("wallet_page::set_current_ticker to {} with force {} called by: {} ({}:{})", ticker.toStdString(), force, location.function_name(), location.file_name(), location.line());
        //called by atomic_dex::wallet_page::qt_static_metacall

        auto& kdf_system = m_system_manager.get_system<kdf_service>();

        if (kdf_system.set_current_ticker(ticker.toStdString()) || force)
        {
            m_transactions_mdl->reset();
            this->set_tx_fetching_busy(true);
            kdf_system.batch_balance_and_tx();
            emit currentTickerChanged();
            refresh_ticker_infos();
            check_send_availability();
        }
    }

    bool
    wallet_page::is_rpc_claiming_busy() const
    {
        return m_is_claiming_busy.load();
    }

    void
    wallet_page::set_claiming_is_busy(bool status)
    {
        if (m_is_claiming_busy != status)
        {
            m_is_claiming_busy = status;
            emit rpcClaimingStatusChanged();
        }
    }

    bool
    wallet_page::is_claiming_faucet_busy() const
    {
        return m_is_claiming_faucet_busy.load();
    }

    void
    wallet_page::set_claiming_faucet_is_busy(bool status)
    {
        if (m_is_claiming_faucet_busy != status)
        {
            m_is_claiming_faucet_busy = status;
            emit claimingFaucetStatusChanged();
        }
    }

    void
    wallet_page::set_send_busy(bool status)
    {
        if (m_is_send_busy != status)
        {
            m_is_send_busy = status;
            emit sendStatusChanged();
        }
    }

    bool
    wallet_page::is_send_busy() const
    {
        return m_is_send_busy.load();
    }

    bool
    wallet_page::is_broadcast_busy() const
    {
        return m_is_broadcast_busy.load();
    }

    void
    wallet_page::set_broadcast_busy(bool status)
    {
        if (m_is_broadcast_busy != status)
        {
            m_is_broadcast_busy = status;
            emit broadCastStatusChanged();
        }
    }

    bool
    wallet_page::is_convert_address_busy() const
    {
        return m_convert_address_busy.load();
    }

    void
    wallet_page::set_convert_address_busy(bool status)
    {
        if (m_convert_address_busy != status)
        {
            m_convert_address_busy = status;
            emit convertAddressBusyChanged();
        }
    }

    bool
    wallet_page::is_validate_address_busy() const
    {
        return m_validate_address_busy.load();
    }

    void
    wallet_page::set_validate_address_busy(bool status)
    {
        if (m_validate_address_busy != status)
        {
            m_validate_address_busy = status;
            emit validateAddressBusyChanged();
        }
    }

    bool atomic_dex::wallet_page::is_tx_fetching_busy() const
    {
        return m_tx_fetching_busy;
    }

    void atomic_dex::wallet_page::set_tx_fetching_busy(bool status)
    {
        if (m_tx_fetching_busy != status)
        {
            m_tx_fetching_busy = status;
            emit txFetchingStatusChanged();
        }
    }

    bool atomic_dex::wallet_page::is_tx_fetching_failed() const
    {
        return m_tx_fetching_failed;
    }

    void atomic_dex::wallet_page::set_tx_fetching_failed(bool status)
    {
        if (m_tx_fetching_failed != status)
        {
            m_tx_fetching_failed = status;
            emit txFetchingOutcomeChanged();
        }
    }

    QVariant wallet_page::get_ticker_infos() const
    {
        QJsonObject obj{
            {"balance", "0"},
            {"name", "Komodo"},
            {"type", "SmartChain"},
            {"is_claimable", true},
            {"address", "foo"},
            {"minimal_balance_asking_rewards", "10.00"},
            {"explorer_url", "foo"},
            {"current_currency_ticker_price", "0.00"},
            {"change_24h", "0"},
            {"tx_state", "InProgress"},
            {"fiat_amount", "0.00"},
            {"trend_7d", QJsonArray()},
            {"activation_status", QJsonObject()},
            {"fee_ticker", DEX_PRIMARY_COIN},
            {"blocks_left", 1},
            {"transactions_left", 0},
            {"current_block", 1},
            {"is_faucet_coin", false},
            {"is_vote_coin", false},
            {"qrcode_address", ""},
            {"segwit_supported", false}};

        std::error_code ec;
        auto&           kdf_system = m_system_manager.get_system<kdf_service>();

        if (kdf_system.is_kdf_running())
        {
            auto&       price_service                 = m_system_manager.get_system<global_price_service>();
            const auto& settings_system               = m_system_manager.get_system<settings_page>();
            const auto& provider                      = m_system_manager.get_system<komodo_prices_provider>();
            const auto& ticker                        = kdf_system.get_current_ticker();
            const auto& coin_info                     = kdf_system.get_coin_info(ticker);
            const auto& config                        = settings_system.get_cfg();
            obj["balance"]                            = QString::fromStdString(kdf_system.get_balance_info(ticker, ec));
            obj["name"]                               = QString::fromStdString(coin_info.name);
            obj["type"]                               = QString::fromStdString(coin_info.type);
            obj["segwit_supported"]                   = coin_info.segwit;
            obj["has_parent_fees_ticker"]             = coin_info.has_parent_fees_ticker;
            obj["fees_ticker"]                        = QString::fromStdString(coin_info.fees_ticker);
            obj["is_claimable"]                       = coin_info.is_claimable;
            obj["minimal_balance_for_asking_rewards"] = QString::fromStdString(coin_info.minimal_claim_amount);
            obj["explorer_url"]                       = QString::fromStdString(coin_info.explorer_url);
            obj["current_currency_ticker_price"]      = QString::fromStdString(price_service.get_rate_conversion(config.current_currency, ticker, true));
            obj["change_24h"]                         = retrieve_change_24h(provider, coin_info, config, m_system_manager);
            const auto& tx_state                      = kdf_system.get_tx_state(ec);
            obj["tx_state"]                           = QString::fromStdString(tx_state.state);
            obj["fiat_amount"]                        = QString::fromStdString(price_service.get_price_in_fiat(config.current_currency, ticker, ec));
            obj["activation_status"]                  = nlohmann_json_object_to_qt_json_object(coin_info.activation_status);
            obj["trend_7d"]                           = nlohmann_json_array_to_qt_json_array(provider.get_ticker_historical(ticker));
            obj["fee_ticker"]                         = QString::fromStdString(coin_info.fees_ticker);
            obj["blocks_left"]                        = static_cast<qint64>(tx_state.blocks_left);
            obj["transactions_left"]                  = static_cast<qint64>(tx_state.transactions_left);
            obj["current_block"]                      = static_cast<qint64>(tx_state.current_block);
            obj["is_faucet_coin"]                     = coin_info.is_faucet_coin;
            obj["is_vote_coin"]                       = coin_info.is_vote_coin;

            std::error_code   ec;
            if (!kdf_system.is_task_activation_ready(coin_info.ticker))
            {
                obj["address"]        = "activating";
                obj["qrcode_address"] = "";

            }
            else
            {
                std::string active_addr = kdf_system.address(ticker, ec);
                if (ec || active_addr.empty() || active_addr == "Invalid Ticker")
                {
                    obj["address"]        = "activating";
                    obj["qrcode_address"] = "";
                }
                else
                {
                    obj["address"] = QString::fromStdString(active_addr);
                    static std::string  cached_address;
                    static QString      cached_qrcode;

                    if (active_addr != cached_address)
                    {
                        qrcodegen::QrCode qr0 = qrcodegen::QrCode::encodeText(active_addr.c_str(), qrcodegen::QrCode::Ecc::MEDIUM);
                        std::string       svg = qr0.toSvgString(2);
                        cached_qrcode  = QStringLiteral("data:image/svg+xml;base64,") + QString::fromStdString(svg).toLocal8Bit().toBase64();
                        cached_address = active_addr;
                    }
                    obj["qrcode_address"] = cached_qrcode;
                }
            }
        }
        return obj;
    }

    QVariant
    wallet_page::get_validate_address_data() const
    {
        return m_validate_address_result.get();
    }

    void
    wallet_page::set_validate_address_data(QVariant rpc_data)
    {
        auto json_result = rpc_data.toJsonObject();
        if (json_result.contains("reason"))
        {
            auto reason = json_result["reason"].toString();
            if (!reason.isEmpty())
            {
                if (reason.contains("Checksum verification failed"))
                {
                    reason                     = tr("Checksum verification failed for %1.").arg(get_current_ticker());
                    json_result["convertible"] = false;
                }
                else if (reason.contains("Invalid address checksum"))
                {
                    reason =
                        tr("Invalid checksum for %1. Click the button to convert to mixed case address.").arg(json_result["ticker"].toString());
                    json_result["convertible"]       = true;
                    json_result["to_address_format"] = QJsonObject{{"format", "mixedcase"}};
                }
                else if (reason.contains("Cashaddress address format activated for BCH, but legacy format used instead. Try to call 'convertaddress'"))
                {
                    reason =
                        tr("Legacy address used for %1. Click the button to convert to a Cashaddress.").arg(json_result["ticker"].toString());
                    json_result["to_address_format"] = QJsonObject{{"format", "cashaddress"}, {"network", "bitcoincash"}};
                    json_result["convertible"]       = true;
                }
                else if (reason.contains("Address must be prefixed with 0x"))
                {
                    reason                     = tr("%1 address must be prefixed with 0x").arg(json_result["ticker"].toString());
                    json_result["convertible"] = false;
                }
                else if (reason.contains("Invalid input length"))
                {
                    reason                     = tr("%1 address length is invalid, please use a valid address.").arg(json_result["ticker"].toString());
                    json_result["convertible"] = false;
                }
                else if (reason.toLower().contains("invalid address"))
                {
                    reason                     = tr("%1 address is invalid.").arg(json_result["ticker"].toString());
                    json_result["convertible"] = false;
                }
                else if (reason.contains("Invalid Checksum"))
                {
                    reason                     = tr("Invalid checksum.");
                    json_result["convertible"] = false;
                }
                else if (reason.contains("has invalid prefixes") or reason.contains("Expected a valid P2PKH or P2SH prefix"))
                {
                    reason = tr("%1 address has invalid prefixes.").arg(json_result["ticker"].toString());
                }
                else
                {
                    reason                     = tr("Backend error: %1").arg(reason);
                    json_result["convertible"] = false;
                }
                json_result["reason"] = reason;
            }
        }
        m_validate_address_result = json_result;
        emit validateAddressDataChanged();
    }

    QVariant
    wallet_page::get_coin_enabling_status() const
    {
        return m_coin_enabling_status.get();
    }

    void
    wallet_page::set_coin_enabling_status(QVariant rpc_data)
    {
        m_coin_enabling_status = rpc_data.toJsonObject();
        emit coinEnablingStatusChanged();
    }

    QVariant
    wallet_page::get_rpc_claiming_data() const
    {
        return m_claiming_rpc_result.get();
    }

    void
    wallet_page::set_rpc_claiming_data(QVariant rpc_data)
    {
        m_claiming_rpc_result = rpc_data.toJsonObject();
        emit claimingRpcDataChanged();
    }

    QVariant
    wallet_page::get_rpc_claiming_faucet_data() const
    {
        return m_claiming_rpc_faucet_result.get();
    }

    void
    wallet_page::set_rpc_claiming_faucet_data(QVariant rpc_data)
    {
        m_claiming_rpc_faucet_result = rpc_data.toJsonObject();
        emit claimingFaucetRpcDataChanged();
    }

    QString
    wallet_page::get_rpc_broadcast_data() const
    {
        return m_broadcast_rpc_result.get();
    }

    void
    wallet_page::set_rpc_broadcast_data(QString rpc_data)
    {
        m_broadcast_rpc_result = rpc_data;
        emit broadcastDataChanged();
    }

    QVariant
    wallet_page::get_rpc_send_data() const
    {
        return m_send_rpc_result.get();
    }

    void
    wallet_page::set_rpc_send_data(QVariant rpc_data)
    {
        m_send_rpc_result = rpc_data.toJsonObject();
        emit sendDataChanged();
    }

    bool
    wallet_page::has_auth_succeeded() const
    {
        return m_auth_succeeded;
    }

    bool
    wallet_page::is_send_available()
    {
        return m_send_available;
    }

    QString
    wallet_page::get_send_availability_state()
    {
        return m_send_availability_state;
    }

    bool
    wallet_page::is_current_ticker_fees_coin_enabled()
    {
        return m_current_ticker_fees_coin_enabled;
    }

    bool
    wallet_page::is_page_open() const
    {
        return m_page_open;
    }

    void wallet_page::set_page_open(bool value)
    {
        m_page_open = value;
        if (value)
        {
            dispatcher_.trigger(gui_enter_wallet{});
        }
        else
        {
            dispatcher_.trigger(gui_leave_wallet{});
        }
        emit isPageOpenChanged();
    }
} // namespace atomic_dex

//! Public api
namespace atomic_dex
{
    void
    wallet_page::refresh_ticker_infos()
    {
        emit tickerInfosChanged();
        QMetaObject::invokeMethod(this, [this]() {
            this->check_send_availability();
        }, Qt::QueuedConnection);
    }

    void
    wallet_page::send(const QString& address, const QString& amount, bool max, bool with_fees, QVariantMap fees_data, const QString& memo, const QString& ibc_source_channel)
    {
        //! Preparation
        this->set_send_busy(true);
        nlohmann::json     batch      = nlohmann::json::array();
        auto&              kdf_system = m_system_manager.get_system<kdf_service>();
        const auto&        ticker     = kdf_system.get_current_ticker();
        const auto&        coin_info  = kdf_system.get_coin_info(ticker);

        // Sia's withdraw is a plain, synchronous MmCoin::withdraw on the KDF
        // side (mm2src/coins/siacoin/siacoin_mm_coin.rs) with no task-based
        // flow of its own, so it belongs on the general path below like any
        // other non-ZHTLC coin. This branch used to stop here after only
        // logging an error: set_send_busy(true) above was never undone, the
        // preview screen stayed busy for the rest of the session, and no
        // request was ever sent to KDF, with no user-facing error either.
        if (coin_info.is_zhtlc_family)
        {
            t_withdraw_init_request withdraw_init_req{.coin = ticker, .to = address.toStdString(), .amount = max ? "0" : amount.toStdString(), .memo = memo.toStdString(), .max = max};

            if (with_fees)
            {
                qDebug() << fees_data;
                auto json_fees    = nlohmann::json::parse(QString(QJsonDocument(QVariant(fees_data).toJsonObject()).toJson()).toStdString());
                withdraw_init_req.fees = t_withdraw_init_fees{
                    .type      = "UtxoPerKbyte",
                    .amount    = json_fees.at("fees_amount").get<std::string>()
                };
            }
            nlohmann::json json_data = kdf::template_request("task::withdraw::init", true);

            kdf::to_json(json_data, withdraw_init_req);

            batch.push_back(json_data);
            std::string amount_std = amount.toStdString();

            if (max)
            {
                std::error_code ec;
                amount_std = kdf_system.get_balance_info(ticker, ec);
            }

            auto answer_functor = [this, coin_info, ticker, amount_std](t_http_response resp)
            {
                const auto& settings_system     = m_system_manager.get_system<settings_page>();
                const auto& global_price_system = m_system_manager.get_system<global_price_service>();
                const auto& current_fiat        = settings_system.get_current_fiat().toStdString();
                auto        answers             = kdf::basic_batch_answer(resp);

                nlohmann::json root_node = (answers.is_array() && !answers.empty()) ? answers[0] : answers;
                std::int8_t task_id = -1;

                if (root_node.contains("result") && root_node["result"].contains("task_id")) {
                    task_id = root_node.at("result").at("task_id").get<std::int8_t>();
                } else if (root_node.contains("task_id")) {
                    task_id = root_node.at("task_id").get<std::int8_t>();
                }

                if (task_id != -1)
                {
                    SPDLOG_DEBUG("Task ID: {}", task_id);
                    using namespace std::chrono_literals;
                    auto&              kdf_system = m_system_manager.get_system<kdf_service>();
                    static std::size_t z_nb_try      = 1;
                    static std::size_t loop_limit    = 500;
                    nlohmann::json     z_error       = nlohmann::json::array();
                    nlohmann::json     z_batch_array = nlohmann::json::array();
                    QString            z_status;
                    t_withdraw_status_request z_request{.task_id = task_id};

                    nlohmann::json j = kdf::template_request("task::withdraw::status", true);
                    kdf::to_json(j, z_request);
                    z_batch_array.push_back(j);

                    do {
                        t_http_response             z_resp      = kdf_system.get_kdf_client().async_rpc_batch_standalone(std::move(z_batch_array)).get();
                        auto                        z_answers   = kdf::basic_batch_answer(z_resp);

                        z_error = z_answers;

                        nlohmann::json loop_node = (z_answers.is_array() && !z_answers.empty()) ? z_answers[0] : z_answers;

                        if (loop_node.contains("result") && loop_node["result"].contains("status")) {
                            z_status = QString::fromStdString(loop_node.at("result").at("status").get<std::string>());
                        } else if (loop_node.contains("status")) {
                            z_status = QString::fromStdString(loop_node.at("status").get<std::string>());
                        } else {
                            z_status = "Unknown";
                        }

                        SPDLOG_DEBUG("[{}/{}] Waiting for {} withdraw status [{}]...", z_nb_try, loop_limit, ticker, z_status.toUtf8().constData());

                        if (z_status == "Ok")
                        {
                            break;
                        }
                        else
                        {
                            set_withdraw_status("Generating transaction");
                        }

                        // Re-pack the status check parameters for the next iteration tick pass
                        z_batch_array = nlohmann::json::array();
                        nlohmann::json j_next = kdf::template_request("task::withdraw::status", true);
                        kdf::to_json(j_next, z_request);
                        z_batch_array.push_back(j_next);

                        std::this_thread::sleep_for(3s);
                        z_nb_try += 1;

                    } while (z_nb_try < loop_limit);

                    try {
                        nlohmann::json final_err_node = (z_error.is_array() && !z_error.empty()) ? z_error[0] : z_error;
                        nlohmann::json details_node = nlohmann::json::object();

                        if (final_err_node.contains("result")) {
                            if (final_err_node["result"].contains("details")) {
                                details_node = final_err_node["result"]["details"];
                            } else {
                                details_node = final_err_node["result"];
                            }
                        } else {
                            details_node = final_err_node;
                        }

                        if (details_node.contains("error"))
                        {
                            SPDLOG_DEBUG("Error zhtlc withdraw_status {}: {} ", ticker, z_status.toUtf8().constData());
                            z_status   = QString::fromStdString(details_node.at("error").get<std::string>());
                            set_withdraw_status(z_status);
                        }
                        else if (z_nb_try == loop_limit)
                        {
                            SPDLOG_DEBUG("Exited zhtlc withdraw loop after 120 tries");
                            SPDLOG_DEBUG("Bad answer for [{}] zhtlc withdraw_status: {}", ticker, final_err_node.dump(4));
                            set_withdraw_status("Timed out");
                        }
                        else
                        {
                            auto           withdraw_answer      = kdf::rpc_process_answer_batch<t_withdraw_status_answer>(final_err_node, "task::withdraw::status");
                            nlohmann::json j_out                = nlohmann::json::object();

                            if (final_err_node.contains("result") && final_err_node["result"].contains("details")) {
                                j_out["withdraw_answer"] = final_err_node["result"]["details"];
                            } else {
                                j_out["withdraw_answer"] = final_err_node;
                            }

                            j_out.at("withdraw_answer")["date"] = withdraw_answer.result.has_value() ? withdraw_answer.result.value().timestamp_as_date : "";

                            // Add total amount in fiat currency.
                            if (coin_info.coinpaprika_id == "test-coin")
                            {
                                j_out["withdraw_answer"]["total_amount_fiat"] = "0";
                            }
                            else
                            {
                                j_out["withdraw_answer"]["total_amount_fiat"] = global_price_system.get_price_as_currency_from_amount(current_fiat, ticker, amount_std);
                            }

                            // Add fees amount.
                            if (j_out.at("withdraw_answer").at("fee_details").contains("total_fee") && !j_out.at("withdraw_answer").at("fee_details").contains("amount"))
                            {
                                j_out["withdraw_answer"]["fee_details"]["amount"] = j_out["withdraw_answer"]["fee_details"]["total_fee"];
                            }
                            if (j_out.at("withdraw_answer").at("fee_details").contains("miner_fee") && !j_out.at("withdraw_answer").at("fee_details").contains("amount"))
                            {
                                j_out["withdraw_answer"]["fee_details"]["amount"] = j_out["withdraw_answer"]["fee_details"]["miner_fee"];
                            }
                            if (j_out.at("withdraw_answer").at("fee_details").contains("total_amount") && !j_out.at("withdraw_answer").at("fee_details").contains("amount"))
                            {
                                j_out["withdraw_answer"]["fee_details"]["amount"] = j_out["withdraw_answer"]["fee_details"]["total_amount"];
                            }

                            // Add fees amount in fiat currency.
                            auto fee = j_out["withdraw_answer"]["fee_details"]["amount"].get<std::string>();
                            if (coin_info.coinpaprika_id == "test-coin")
                            {
                                j_out["withdraw_answer"]["fee_details"]["amount_fiat"] = "0";
                            }
                            else
                            {
                                j_out["withdraw_answer"]["fee_details"]["amount_fiat"] =
                                    global_price_system.get_price_as_currency_from_amount(current_fiat, coin_info.fees_ticker, fee);
                            }
                            this->set_rpc_send_data(nlohmann_json_object_to_qt_json_object(j_out));
                            set_withdraw_status("Complete");
                        }
                        z_nb_try = 0;
                    }
                    catch (const std::exception& error)
                    {
                        SPDLOG_ERROR("exception caught in zhtlc withdraw_status: {}", error.what());
                        set_withdraw_status(QString::fromStdString(error.what()));
                    }
                }
                else
                {
                    std::string body                = (resp.extract_string(true).get());
                    auto error_json = QJsonObject({{"error_code", resp.status_code()}, {"error_message", QString::fromStdString(body)}});
                    this->set_rpc_send_data(error_json);
                }
                this->set_send_busy(false);
            };

            //! Process
            kdf_system.get_kdf_client().async_rpc_batch_standalone(std::move(batch)).then(
                [this, answer_functor](async::task<t_http_response> previous_task)
                {
                    try
                    {
                        answer_functor(previous_task.get());
                    }
                    catch (const std::exception& e)
                    {
                        SPDLOG_ERROR("exception caught in send: {}", e.what());
                        auto error_json = QJsonObject({{"error_code", 500}, {"error_message", QString::fromStdString(e.what())}});
                        this->set_rpc_send_data(error_json);
                        this->set_send_busy(false);
                    }
                });

        }
        else
        {
            t_withdraw_request withdraw_req{
                .coin = ticker,
                .to = address.toStdString(),
                .amount = max ? "0" : amount.toStdString(),
                .memo = memo.toStdString(),
                .max = max
            };

            if (ibc_source_channel.toStdString() != "")
            {
                withdraw_req.ibc_source_channel = ibc_source_channel.toStdString();
            }

            auto json_fees    = nlohmann::json::parse(QString(QJsonDocument(QVariant(fees_data).toJsonObject()).toJson()).toStdString());
            if (with_fees)
            {
                qDebug() << fees_data;
                withdraw_req.fees = t_withdraw_fees{
                    .type      = "UtxoPerKbyte",
                    .amount    = json_fees.at("fees_amount").get<std::string>(),
                    .gas_limit = json_fees.at("gas_limit").get<int>()};
                if (coin_info.coin_type == CoinType::ERC20)
                {
                    withdraw_req.fees->type = "EthGas";
                    withdraw_req.fees->gas_price = json_fees.at("gas_price").get<std::string>();
                }
                else if (coin_info.coin_type == CoinType::QRC20)
                {
                    withdraw_req.fees->type = "Qrc20Gas";
                    withdraw_req.fees->gas_price = json_fees.at("gas_price").get<std::string>();
                }
                else if (coin_info.coin_type == CoinType::TENDERMINTTOKEN or coin_info.coin_type == CoinType::TENDERMINT)
                {
                    withdraw_req.fees->type = "CosmosGas";
                    withdraw_req.fees->cosmos_gas_price = std::stod(json_fees.at("gas_price").get<std::string>());
                }
                else if (coin_info.has_parent_fees_ticker)
                {
                    withdraw_req.fees->type = "otherGas";
                    withdraw_req.fees->gas_price = json_fees.at("gas_price").get<std::string>();
                }
            }
            else if (coin_info.coin_type == CoinType::TENDERMINTTOKEN or coin_info.coin_type == CoinType::TENDERMINT)
            {
                withdraw_req.fees = t_withdraw_fees{
                    .type      = "CosmosGas",
                    .cosmos_gas_price = 0.05,
                    .gas_limit = 150000};
            }

            nlohmann::json json_data = kdf::template_request("withdraw", true);
            kdf::to_json(json_data, withdraw_req);
            //SPDLOG_DEBUG("withdraw request: {}", json_data.dump(4));

            batch.push_back(json_data);

            std::string amount_std = amount.toStdString();
            if (max)
            {
                std::error_code ec;
                amount_std = kdf_system.get_balance_info(ticker, ec);
            }

            //! Answer
            auto answer_functor = [this, coin_info, ticker, amount_std](t_http_response resp)
            {
                const auto& settings_system     = m_system_manager.get_system<settings_page>();
                const auto& global_price_system = m_system_manager.get_system<global_price_service>();
                const auto& current_fiat        = settings_system.get_current_fiat().toStdString();
                std::string body                = (resp.extract_string(true).get());

                if (resp.status_code() == 200 && body.find("error") == std::string::npos)
                {
                    auto           answers              = nlohmann::json::parse(body);
                    auto           withdraw_answer      = kdf::rpc_process_answer_batch<t_withdraw_answer>(answers[0], "withdraw");
                    nlohmann::json j_out                = nlohmann::json::object();
                    j_out["withdraw_answer"]            = answers[0]["result"];
                    j_out.at("withdraw_answer")["date"] = withdraw_answer.result.value().timestamp_as_date;

                    // Add total amount in fiat currency.
                    if (coin_info.coinpaprika_id == "test-coin")
                    {
                        j_out["withdraw_answer"]["total_amount_fiat"] = "0";
                    }
                    else
                    {
                        j_out["withdraw_answer"]["total_amount_fiat"] = global_price_system.get_price_as_currency_from_amount(current_fiat, ticker, amount_std);
                    }

                    // Add fees amount.
                    if (j_out.at("withdraw_answer").at("fee_details").contains("total_fee") && !j_out.at("withdraw_answer").at("fee_details").contains("amount"))
                    {
                        j_out["withdraw_answer"]["fee_details"]["amount"] = j_out["withdraw_answer"]["fee_details"]["total_fee"];
                    }
                    if (j_out.at("withdraw_answer").at("fee_details").contains("miner_fee") && !j_out.at("withdraw_answer").at("fee_details").contains("amount"))
                    {
                        j_out["withdraw_answer"]["fee_details"]["amount"] = j_out["withdraw_answer"]["fee_details"]["miner_fee"];
                    }
                    // Sia's TxFeeDetails names this field "total_amount", not "amount"
                    // (KDF Reloaded CRD ch.20 R-W4 -- corpus-confirmed dictated wire name,
                    // not a naming inconsistency to fix on the KDF side). This is the path
                    // Sia's synchronous withdraw actually takes; the block above is the
                    // ZHTLC task-based path, fixed the same way for consistency in case
                    // Sia ever moves onto it.
                    if (j_out.at("withdraw_answer").at("fee_details").contains("total_amount") && !j_out.at("withdraw_answer").at("fee_details").contains("amount"))
                    {
                        j_out["withdraw_answer"]["fee_details"]["amount"] = j_out["withdraw_answer"]["fee_details"]["total_amount"];
                    }

                    // Add fees amount in fiat currency.
                    auto fee = j_out["withdraw_answer"]["fee_details"]["amount"].get<std::string>();
                    if (coin_info.coinpaprika_id == "test-coin")
                    {
                        j_out["withdraw_answer"]["fee_details"]["amount_fiat"] = "0";
                    }
                    else
                    {
                        j_out["withdraw_answer"]["fee_details"]["amount_fiat"] =
                            global_price_system.get_price_as_currency_from_amount(current_fiat, coin_info.fees_ticker, fee);
                    }

                    this->set_rpc_send_data(nlohmann_json_object_to_qt_json_object(j_out));
                }
                else
                {
                    auto error_json = QJsonObject({{"error_code", resp.status_code()}, {"error_message", QString::fromStdString(body)}});
                    this->set_rpc_send_data(error_json);
                }
                this->set_send_busy(false);
            };

            //! Process
            kdf_system.get_kdf_client().async_rpc_batch_standalone(std::move(batch)).then(
                [this, answer_functor](async::task<t_http_response> previous_task)
                {
                    try
                    {
                        answer_functor(previous_task.get());
                    }
                    catch (const std::exception& e)
                    {
                        SPDLOG_ERROR("exception caught in send: {}", e.what());
                        auto error_json = QJsonObject({{"error_code", 500}, {"error_message", QString::fromStdString(e.what())}});
                        this->set_rpc_send_data(error_json);
                        this->set_send_busy(false);
                    }
                });
        }
    }

    void
    wallet_page::broadcast(const QString& tx_hex, bool is_claiming, bool is_max, const QString& amount, const QString& tx_json)
    {
        broadcast_on_auth_finished(true, tx_hex, is_claiming, is_max, amount, tx_json);
    }

    void
    wallet_page::broadcast_on_auth_finished(
        bool is_auth, const QString& tx_hex, bool is_claiming, bool is_max, const QString& amount, const QString& tx_json)
    {
        if (!is_auth)
        {
            m_auth_succeeded = false;
            emit auth_succeededChanged();
            return;
        }
        m_auth_succeeded = true;
        emit auth_succeededChanged();
        this->set_rpc_broadcast_data("");
        this->set_broadcast_busy(true);
        auto&               kdf_system = m_system_manager.get_system<kdf_service>();
        const auto&         ticker     = kdf_system.get_current_ticker();
        nlohmann::json      batch      = nlohmann::json::array();
        t_broadcast_request broadcast_request{
            .tx_hex = tx_hex.toStdString(), .coin = ticker, .tx_json = tx_json.toStdString()};
        nlohmann::json      json_data = kdf::template_request("send_raw_transaction");
        kdf::to_json(json_data, broadcast_request);
        batch.push_back(json_data);

        //! Answer
        auto answer_functor = [this, is_claiming, is_max, amount](t_http_response resp)
        {
            std::string body = (resp.extract_string(true).get());
            if (resp.status_code() == 200)
            {
                auto&       kdf_system = m_system_manager.get_system<kdf_service>();
                const auto& ticker     = kdf_system.get_current_ticker();
                auto        answers    = nlohmann::json::parse(body);
                // SPDLOG_INFO("broadcast answer: {}", answers.dump(4));
                if (answers[0].contains("tx_hash"))
                {
                    this->set_rpc_broadcast_data(QString::fromStdString(answers[0].at("tx_hash").get<std::string>()));
                    if (kdf_system.is_pin_cfg_enabled() && (not is_claiming && is_max))
                    {
                        kdf_system.reset_fake_balance_to_zero(ticker);
                    }
                    else if (kdf_system.is_pin_cfg_enabled() && (not is_claiming && not is_max))
                    {
                        kdf_system.decrease_fake_balance(ticker, amount.toStdString());
                    }
                    kdf_system.batch_balance_and_tx();
                }
                else
                {
                    this->set_rpc_broadcast_data(QString::fromStdString(body));
                }
            }
            else
            {
                this->set_rpc_broadcast_data(QString::fromStdString(body));
            }
            this->set_broadcast_busy(false);
        };

        kdf_system.get_kdf_client().async_rpc_batch_standalone(std::move(batch)).then(
            [this, answer_functor](async::task<t_http_response> previous_task)
            {
                try
                {
                    answer_functor(previous_task.get());
                }
                catch (const std::exception& e)
                {
                    SPDLOG_ERROR("exception caught in broadcast finished: {}", e.what());
                    this->set_rpc_broadcast_data(QString::fromStdString(e.what()));
                    this->set_broadcast_busy(false);
                }
            });
    }

    void
    wallet_page::claim_rewards()
    {
        this->set_claiming_is_busy(true);
        nlohmann::json     batch      = nlohmann::json::array();
        auto&              kdf_system = m_system_manager.get_system<kdf_service>();
        std::error_code    ec;
        t_withdraw_request withdraw_req{.coin = "KMD", .to = kdf_system.address("KMD", ec), .amount = "0", .max = true};
        nlohmann::json     json_data = kdf::template_request("withdraw", true);
        kdf::to_json(json_data, withdraw_req);
        batch.push_back(json_data);
        json_data = kdf::template_request("kmd_rewards_info");
        batch.push_back(json_data);

        auto answer_functor = [this](t_http_response resp)
        {
            std::string body = (resp.extract_string(true).get());
            if (resp.status_code() == 200 && body.find("error") == std::string::npos)
            {
                auto           answers              = nlohmann::json::parse(body);
                auto           withdraw_answer      = kdf::rpc_process_answer_batch<t_withdraw_answer>(answers[0], "withdraw");
                nlohmann::json j_out                = nlohmann::json::object();
                j_out["withdraw_answer"]            = answers[0]["result"];
                j_out.at("withdraw_answer")["date"] = withdraw_answer.result.value().timestamp_as_date;
                auto kmd_rewards_answer             = kdf::process_kmd_rewards_answer(answers[1]);
                j_out["kmd_rewards_info"]           = kmd_rewards_answer.result;
                this->set_rpc_claiming_data(nlohmann_json_object_to_qt_json_object(j_out));
            }
            else
            {
                auto error_json = QJsonObject({{"error_code", resp.status_code()}, {"error_message", QString::fromStdString(body)}});
                this->set_rpc_claiming_data(error_json);
            }
            this->set_claiming_is_busy(false);
        };

        kdf_system.get_kdf_client().async_rpc_batch_standalone(std::move(batch)).then(
            [this, answer_functor](async::task<t_http_response> previous_task)
            {
                try
                {
                    answer_functor(previous_task.get());
                }
                catch (const std::exception& e)
                {
                    SPDLOG_ERROR("exception caught in claim_rewards: {}", e.what());
                    auto error_json = QJsonObject({{"error_code", 500}, {"error_message", QString::fromStdString(e.what())}});
                    this->set_rpc_claiming_data(error_json);
                    this->set_claiming_is_busy(false);
                }
            });
    }

    void
    wallet_page ::claim_faucet()
    {
        const auto&                kdf_system = m_system_manager.get_system<kdf_service>();
        const auto&                ticker     = kdf_system.get_current_ticker();
        const auto&                coin_info  = kdf_system.get_coin_info(ticker);
        std::error_code            ec;
        faucet::api::claim_request claim_request{.coin_name = coin_info.ticker, .wallet_address = kdf_system.address(ticker, ec)};

        this->set_claiming_faucet_is_busy(true);
        faucet::api::claim(claim_request)
            .then(
                [this](async::task<t_http_response> previous_task)
                {
                    try
                    {
                        auto claim_result = faucet::api::get_claim_result(previous_task.get());
                        this->set_rpc_claiming_faucet_data(
                            QJsonObject({{"message", QString::fromStdString(claim_result.message)}, {"status", QString::fromStdString(claim_result.status)}}));
                    }
                    catch (...)
                    {
                        handle_exception_async_task(std::current_exception());
                    }
                    this->set_claiming_faucet_is_busy(false);
                });
    }

    transactions_model*
    wallet_page::get_transactions_mdl() const
    {
        return m_transactions_mdl;
    }

    void
    wallet_page::on_tx_fetch_finished(const tx_fetch_finished& evt)
    {
        if (!evt.with_error && QString::fromStdString(evt.ticker) == get_current_ticker())
        {
            std::error_code ec;
            const auto& settings         = m_system_manager.get_system<settings_page>();
            t_transactions  transactions = m_system_manager.get_system<kdf_service>().get_tx_history(ec);
            t_transactions  to_init;

            if (settings.is_spamfilter_enabled())
            {
                for (auto&& cur_tx: transactions)
                {
                    if (safe_float(cur_tx.total_amount) != 0)
                    {
                        to_init.push_back(cur_tx);
                    }
                }
            }
            else
            {
                to_init = transactions;
            }

            if (m_transactions_mdl->rowCount() == 0)
            {
                //! insert all transactions
                m_transactions_mdl->init_transactions(to_init);
            }
            else
            {
                //! Update tx (only unconfirmed) or insert (new tx)
                m_transactions_mdl->update_or_insert_transactions(to_init);
            }

            if (ec)
            {
                this->set_tx_fetching_failed(true);
            }
            else
            {
                this->set_tx_fetching_failed(false);
            }
        }
        else
        {
            this->m_transactions_mdl->reset();
        }
        this->set_tx_fetching_busy(false);
    }

    void
    wallet_page::validate_address(QString address)
    {
        auto& kdf_system = m_system_manager.get_system<kdf_service>();
        if (kdf_system.is_kdf_running())
        {
            const auto& ticker = kdf_system.get_current_ticker();
            validate_address(address, QString::fromStdString(ticker));
        }
    }

    void
    wallet_page::validate_address(QString address, QString ticker)
    {
        // SPDLOG_INFO("validate_address: {} - ticker: {}", address.toStdString(), ticker.toStdString());
        auto& kdf_system = m_system_manager.get_system<kdf_service>();
        if (kdf_system.is_kdf_running())
        {
            std::error_code            ec;
            t_validate_address_request req{.coin = ticker.toStdString(), .address = address.toStdString()};
            this->set_validate_address_busy(true);
            nlohmann::json batch     = nlohmann::json::array();
            nlohmann::json json_data = kdf::template_request("validateaddress");
            kdf::to_json(json_data, req);
            batch.push_back(json_data);
            auto answer_functor = [this, ticker](t_http_response resp)
            {
                std::string body = (resp.extract_string(true).get());
                nlohmann::json j_out = nlohmann::json::object();
                j_out["ticker"]      = ticker.toStdString();
                if (resp.status_code() == 200)
                {
                    auto answers         = nlohmann::json::parse(body);
                    auto validate_answer = kdf::rpc_process_answer_batch<t_validate_address_answer>(answers[0], "validateaddress");
                    if (validate_answer.result.has_value())
                    {
                        auto res          = validate_answer.result.value();
                        j_out["is_valid"] = res.is_valid;
                        j_out["reason"]   = res.reason.value_or("");
                    }
                    else
                    {
                        if (!m_system_manager.get_system<kdf_service>().is_task_activation_ready(ticker.toStdString()))
                        {
                            j_out["reason"]   = "Validation error: Coin not fully enabled";
                        }
                        else
                        {
                            j_out["reason"]   = "Validation error: Unknown";
                        }

                        j_out["is_valid"] = false;
                    }
                }
                else
                {
                    j_out["is_valid"] = false;
                    j_out["reason"]   = "Validation error: Problem with connection";
                }
                this->set_validate_address_data(nlohmann_json_object_to_qt_json_object(j_out));
                this->set_validate_address_busy(false);
            };

            kdf_system.get_kdf_client().async_rpc_batch_standalone(std::move(batch)).then(
                [answer_functor](async::task<t_http_response> previous_task)
                {
                    try
                    {
                        answer_functor(previous_task.get());
                    }
                    catch (...)
                    {
                        handle_exception_async_task(std::current_exception());
                    }
                });
        }
    }

    void
    wallet_page::convert_address(QString from, QVariant to_address_format)
    {
        auto& kdf_system = m_system_manager.get_system<kdf_service>();
        if (kdf_system.is_kdf_running())
        {
            const auto& ticker = kdf_system.get_current_ticker();
            convert_address(from, QString::fromStdString(ticker), to_address_format);
        }
    }

    void
    wallet_page::convert_address(QString from, QString ticker, QVariant to_address_format)
    {
        auto& kdf_system = m_system_manager.get_system<kdf_service>();
        if (kdf_system.is_kdf_running())
        {
            QVariantMap               out         = to_address_format.value<QVariantMap>();
            auto                      address_fmt = nlohmann::json::parse(QJsonDocument::fromVariant(out).toJson().toStdString());
            t_convert_address_request req{.coin = ticker.toStdString(), .from = from.toStdString(), .to_address_format = address_fmt};
            this->set_convert_address_busy(true);
            nlohmann::json batch     = nlohmann::json::array();
            nlohmann::json json_data = kdf::template_request("convertaddress");
            kdf::to_json(json_data, req);
            batch.push_back(json_data);
            auto answer_functor = [this](t_http_response resp)
            {
                std::string body = (resp.extract_string(true).get());
                if (resp.status_code() == 200)
                {
                    auto answers        = nlohmann::json::parse(body);
                    auto convert_answer = kdf::rpc_process_answer_batch<t_convert_address_answer>(answers[0], "convertaddress");
                    if (convert_answer.result.has_value())
                    {
                        auto res = QString::fromStdString(convert_answer.result.value().address);
                        this->set_converted_address(res);
                    }
                }
                this->set_convert_address_busy(false);
            };

            kdf_system.get_kdf_client().async_rpc_batch_standalone(std::move(batch)).then(
                [answer_functor](async::task<t_http_response> previous_task)
                {
                    try
                    {
                        answer_functor(previous_task.get());
                    }
                    catch (...)
                    {
                        handle_exception_async_task(std::current_exception());
                    }
                });
        }
    }

    QString
    wallet_page::get_withdraw_status() const
    {
        return m_withdraw_status.get();
    }

    void
    wallet_page::set_withdraw_status(QString status)
    {
        m_withdraw_status = status;
        emit withdrawStatusChanged();
    }

    QString
    wallet_page::get_converted_address() const
    {
        return m_converted_address.get();
    }

    void
    wallet_page::set_converted_address(QString converted_address)
    {
        m_converted_address = converted_address;
        emit convertedAddressChanged();
    }
} // namespace atomic_dex
