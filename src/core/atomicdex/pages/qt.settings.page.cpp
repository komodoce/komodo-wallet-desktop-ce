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

#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QLocale>
#include <QSettings>
#include <boost/algorithm/string/case_conv.hpp>
#include "atomicdex/constants/dex.constants.hpp"
#include "atomicdex/api/kdf/rpc_v2/rpc2.get_public_key.hpp"
#include "atomicdex/config/enable.cfg.hpp"
#include "atomicdex/events/events.hpp"
#include "atomicdex/managers/qt.wallet.manager.hpp"
#include "atomicdex/models/qt.global.coins.cfg.model.hpp"
#include "atomicdex/pages/qt.portfolio.page.hpp"
#include "atomicdex/pages/qt.settings.page.hpp"
#include "atomicdex/pages/qt.wallet.page.hpp"
#include "atomicdex/services/kdf/kdf.service.hpp"
#include "atomicdex/services/price/global.provider.hpp"
#include "atomicdex/utilities/global.utilities.hpp"
#include "atomicdex/utilities/qt.utilities.hpp"

namespace atomic_dex
{
    settings_page::settings_page(entt::registry& registry, ag::ecs::system_manager& system_manager, std::shared_ptr<QApplication> app, QObject* parent) :
        QObject(parent), system(registry), m_system_manager(system_manager), m_app(app)
    {}

    void settings_page::init_lang()
    {
        set_current_lang(get_current_lang());
    }

    void settings_page::garbage_collect_qml()
    {
        SPDLOG_INFO("Garbage collecting QML Engine");
        m_qml_engine->collectGarbage();
        m_qml_engine->trimComponentCache();
        m_qml_engine->clearComponentCache();
    }
} // namespace atomic_dex

namespace atomic_dex { void settings_page::update() {} }

namespace atomic_dex
{
    bool settings_page::get_use_sync_date() const
    {
        QSettings& settings = entity_registry_.ctx().get<QSettings>();
        return settings.value("UseSyncDate").toBool();
    }
    int settings_page::get_pirate_sync_date() const
    {
        QSettings& settings = entity_registry_.ctx().get<QSettings>();
        return settings.value("PirateSyncDate").toInt();
    }
    int settings_page::get_pirate_sync_height(int sync_date, int checkpoint_height, int checkpoint_blocktime) const
    {
        if (checkpoint_height == 0)
        {
            return 0;
        }
        int blocktime_estimate = 65; // Based on average block time between checkpoint block and block 2575600 + margin of error
        SPDLOG_INFO("sync_date: {}", sync_date);
        SPDLOG_INFO("checkpoint_height: {}", checkpoint_height);
        SPDLOG_INFO("checkpoint_blocktime: {}", checkpoint_blocktime);
        int time_delta = sync_date - checkpoint_blocktime;
        SPDLOG_INFO("time_delta: {}", time_delta);
        int block_delta =  static_cast<int>(time_delta / blocktime_estimate);
        SPDLOG_INFO("block_delta: {}", block_delta);
        // As block time is variable, we round height to the nearest 1000 blocks
        int height = checkpoint_height + static_cast<int>(block_delta / 1000) * 1000;
        if (height < 0)
        {
            height = 0;
        }
        SPDLOG_INFO("height: {}", height);
        return height;
    }

    void settings_page::set_pirate_sync_date(int new_value)
    {
        QSettings&        settings     = entity_registry_.ctx().get<QSettings>();
        settings.setValue("UseSyncDate", new_value);
        settings.sync();
    }

    QString settings_page::get_current_lang() const
    {
        QSettings& settings = entity_registry_.ctx().get<QSettings>();
        return settings.value("CurrentLang").toString();
    }

    void atomic_dex::settings_page::set_current_lang(QString new_lang)
    {
        const std::string new_lang_std = new_lang.toStdString();
        QSettings&        settings     = entity_registry_.ctx().get<QSettings>();
        settings.setValue("CurrentLang", new_lang);
        settings.sync();

        auto get_locale = [](const std::string& current_lang)
        {
            if (current_lang == "tr")
            {
                return QLocale::Language::Turkish;
            }
            else if (current_lang == "en")
            {
                return QLocale::Language::English;
            }
            else if (current_lang == "es")
            {
                return QLocale::Language::Spanish;
            }
            else if (current_lang == "de")
            {
                return QLocale::Language::German;
            }
            else if (current_lang == "fr")
            {
                return QLocale::Language::French;
            }
            else if (current_lang == "ru")
            {
                return QLocale::Language::Russian;
            }
            return QLocale::Language::AnyLanguage;
        };

        auto path = QString{":/assets/languages/atomic_defi_" + new_lang};

        SPDLOG_INFO("Locale before parsing Komodo Wallet settings: {}", QLocale().name().toStdString());
        QLocale::setDefault(get_locale(new_lang.toStdString()));
        SPDLOG_INFO("Locale after parsing Komodo Wallet settings: {}", QLocale().name().toStdString());
        if (!this->m_translator.load(path))
        {
            SPDLOG_ERROR("Failed to load {} translation in {}.qm", new_lang.toStdString(), path.toStdString());
            return;
        }
        this->m_app->installTranslator(&m_translator);
        this->m_qml_engine->retranslate();
        SPDLOG_INFO("Successfully loaded {} translation in {}.qm", new_lang.toStdString(), path.toStdString());
        emit onLangChanged();
    }

    bool atomic_dex::settings_page::is_static_rpcpass_enabled() const
    {
        return m_config.static_rpcpass_enabled;
    }

    bool atomic_dex::settings_page::set_zhtlc_status(nlohmann::json data)
    {
        m_zhtlc_status = data;
        // SPDLOG_INFO("zhtlc status: {}", m_zhtlc_status.get().dump(4));
        emit onZhtlcStatusChanged();
        return true;
    }

    nlohmann::json atomic_dex::settings_page::get_task_activation_status()
    {
        return m_zhtlc_status.get();
    }

    void settings_page::set_static_rpcpass_enabled(bool is_enabled)
    {
        if (m_config.static_rpcpass_enabled != is_enabled)
        {
            change_static_rpcpass_status(m_config, is_enabled);
            emit onStaticRpcPassEnabledChanged();
        }
    }

    bool atomic_dex::settings_page::is_spamfilter_enabled() const
    {
        return m_config.spamfilter_enabled;
    }

    void settings_page::set_spamfilter_enabled(bool is_enabled)
    {
        if (m_config.spamfilter_enabled != is_enabled)
        {
            auto& kdf       = m_system_manager.get_system<kdf_service>();
            auto& wallet_pg = m_system_manager.get_system<wallet_page>();
            QString ticker  = QString::fromStdString(kdf.get_current_ticker());
            change_spamfilter_status(m_config, is_enabled);
            emit onSpamFilterEnabledChanged();
            wallet_pg.set_current_ticker(ticker, true);
        }
    }

    bool atomic_dex::settings_page::is_postorder_enabled() const
    {
        return m_config.postorder_enabled;
    }

    void settings_page::set_postorder_enabled(bool is_enabled)
    {
        if (m_config.postorder_enabled != is_enabled)
        {
            change_postorder_status(m_config, is_enabled);
            emit onPostOrderEnabledChanged();
        }
    }

    bool atomic_dex::settings_page::is_notification_enabled() const
    {
        return m_config.notification_enabled;
    }

    void settings_page::set_notification_enabled(bool is_enabled)
    {
        if (m_config.notification_enabled != is_enabled)
        {
            change_notification_status(m_config, is_enabled);
            emit onNotificationEnabledChanged();
        }
    }

    QString settings_page::get_current_currency_sign() const
    {
        return QString::fromStdString(this->m_config.current_currency_sign);
    }

    QString settings_page::get_current_fiat_sign() const
    {
        return QString::fromStdString(this->m_config.current_fiat_sign);
    }

    QString settings_page::get_current_currency() const
    {
        return QString::fromStdString(this->m_config.current_currency);
    }

    void settings_page::set_current_currency(const QString& current_currency)
    {
        if (m_config.possible_currencies.empty())
        {
            SPDLOG_ERROR("m_config.possible_currencies is empty!");
            return;
        }

        bool        can_proceed = true;
        std::string reason      = "";
        if (atomic_dex::is_this_currency_a_fiat(m_config, current_currency.toStdString()))
        {
            if (!m_system_manager.get_system<global_price_service>().is_fiat_available(current_currency.toStdString()))
            {
                can_proceed = false;
                reason      = "rate for fiat: " + current_currency.toStdString() + " not available";
            }
        }
        else
        {
            if (!m_system_manager.get_system<global_price_service>().is_currency_available(current_currency.toStdString()))
            {
                can_proceed = false;
                reason      = "rate for currency " + current_currency.toStdString() + " not available";
            }
        }

        if (current_currency.toStdString() != m_config.current_currency && can_proceed)
        {
            spdlog::stopwatch sw; using namespace std::chrono;
            atomic_dex::change_currency(m_config, current_currency.toStdString());
            this->dispatcher_.trigger(force_update_providers{});
            this->dispatcher_.trigger(update_portfolio_values{});
            emit onCurrencyChanged();
            emit onCurrencySignChanged();
            emit onFiatSignChanged();
            SPDLOG_DEBUG("Time elapsed in settings_page::set_current_currency to {}: {}", current_currency.toStdString(), duration_cast<milliseconds>(sw.elapsed()));
        }
        else
        {
            if (!reason.empty())
            {
                SPDLOG_WARN("Cannot change currency to {} for reason: {}", current_currency.toStdString(), reason);
                // Try next in line
                unsigned long selected_idx = utils::get_index_str(m_config.possible_currencies, current_currency.toStdString());
                if (selected_idx < m_config.possible_currencies.size() - 1)
                {
                    set_current_currency(QString::fromStdString(m_config.possible_currencies[selected_idx + 1]));
                }
                else
                {
                    set_current_currency(QString::fromStdString(m_config.possible_currencies[0]));
                }
            }
        }
    }

    QString settings_page::get_current_fiat() const
    {
        return QString::fromStdString(this->m_config.current_fiat);
    }

    void settings_page::set_current_fiat(const QString& current_fiat)
    {
        if (m_system_manager.get_system<global_price_service>().is_fiat_available(current_fiat.toStdString()))
        {
            if (current_fiat.toStdString() != m_config.current_fiat)
            {
                atomic_dex::change_fiat(m_config, current_fiat.toStdString());
                emit onFiatChanged();
            }
        }
        else
        {
            SPDLOG_ERROR("Cannot change fiat, because other rates are not available");
        }
    }

    bool settings_page::is_fetching_priv_key_busy() const
    {
        return m_fetching_priv_keys_busy.load();
    }

    void settings_page::set_fetching_priv_key_busy(bool status)
    {
        if (m_fetching_priv_keys_busy != status)
        {
            m_fetching_priv_keys_busy = status;
            emit privKeyStatusChanged();
        }
    }

    bool settings_page::is_fetching_public_key() const
    {
        return fetching_public_key;
    }

    const QString& settings_page::get_public_key() const
    {
        return public_key;
    }

    atomic_dex::cfg& settings_page::get_cfg()
    {
        return m_config;
    }

    const atomic_dex::cfg& settings_page::get_cfg() const
    {
        return m_config;
    }
} // namespace atomic_dex

// QML API
namespace atomic_dex
{
    QStringList settings_page::get_available_langs() const
    {
        QSettings& settings = entity_registry_.ctx().get<QSettings>();
        return settings.value("AvailableLang").toStringList();
    }

    QStringList settings_page::get_available_fiats() const
    {
        QStringList out;
        out.reserve(m_config.available_fiat.size());
        for (auto&& cur_fiat: m_config.available_fiat) { out.push_back(QString::fromStdString(cur_fiat)); }
        //out.sort(); // list already sorted in config
        return out;
    }

    QStringList settings_page::get_recommended_fiats()
    {
        static const auto nb_recommended = 6;
        QStringList       out;
        out.reserve(nb_recommended);
        for (auto&& it = m_config.recommended_fiat.begin(); it != m_config.recommended_fiat.end() && it < m_config.recommended_fiat.begin() + nb_recommended; it++)
        {
            out.push_back(QString::fromStdString(*it));
        }
        return out;
    }

    QStringList settings_page::get_available_currencies() const
    {
        QStringList out;
        out.reserve(m_config.possible_currencies.size());
        for (auto&& cur_currency: m_config.possible_currencies) { out.push_back(QString::fromStdString(cur_currency)); }
        return out;
    }

    bool settings_page::is_this_ticker_present_in_raw_cfg(const QString& ticker) const
    {
        return m_system_manager.get_system<kdf_service>().is_this_ticker_present_in_raw_cfg(ticker.toStdString());
    }

    bool settings_page::is_this_ticker_present_in_normal_cfg(const QString& ticker) const
    {
        return m_system_manager.get_system<kdf_service>().is_this_ticker_present_in_normal_cfg(ticker.toStdString());
    }

    void settings_page::set_qml_engine(QQmlApplicationEngine* engine)
    {
        m_qml_engine = engine;
    }

    QStringList settings_page::retrieve_seed(const QString& wallet_name, const QString& password)
    {
        QStringList     out;
        std::error_code ec;
        auto            key = atomic_dex::derive_password(password.toStdString(), ec);
        if (ec)
        {
            SPDLOG_ERROR("cannot derive the password: {}", ec.message());
            if (ec == dextop_error::derive_password_failed)
            {
                return {"wrong password"};
            }
        }
        using namespace std::string_literals;
        const std::filesystem::path seed_path = utils::get_atomic_dex_config_folder() / (wallet_name.toStdString() + ".seed"s);
        auto           seed      = atomic_dex::decrypt(seed_path, key.data(), ec);
        const std::filesystem::path rpcpass_path = utils::get_atomic_dex_config_folder() / (wallet_name.toStdString() + ".rpcpass"s);
        auto           rpcpass   = atomic_dex::decrypt(rpcpass_path, key.data(), ec);

        if (ec == dextop_error::corrupted_file_or_wrong_password)
        {
            SPDLOG_ERROR("cannot decrypt the seed with the derived password: {}", ec.message());
            return {"wrong password"};
        }

        if (!ec)
        {
            this->set_fetching_priv_key_busy(true);

            nlohmann::json batch   = nlohmann::json::array();
            const auto*    cfg_mdl = m_system_manager.get_system<portfolio_page>().get_global_cfg();
            const auto     coins   = cfg_mdl->get_enabled_coins();

            for (auto&& [coin, coin_cfg]: coins)
            {
                kdf::show_priv_key_request req{.coin = coin};
                nlohmann::json                    req_json = kdf::template_request("show_priv_key");
                to_json(req_json, req);
                batch.push_back(req_json);
            }

            auto&      kdf_system     = m_system_manager.get_system<kdf_service>();

            const auto answer_functor = [this](t_http_response resp)
            {
                std::string body = (resp.extract_string(true).get());
                if (resp.status_code() == 200)
                {
                    auto answers = nlohmann::json::parse(body);
                    SPDLOG_WARN("Priv keys fetched, those are sensitive data.");
                    for (auto&& answer: answers)
                    {
                        auto       show_priv_key_answer = kdf::rpc_process_answer_batch<kdf::show_priv_key_answer>(answer, "show_priv_key");
                        auto*      portfolio_mdl        = this->m_system_manager.get_system<portfolio_page>().get_portfolio();
                        const auto idx                  = portfolio_mdl->match(
                                             portfolio_mdl->index(0, 0), portfolio_model::TickerRole, QString::fromStdString(show_priv_key_answer.coin), 1,
                                             Qt::MatchFlag::MatchExactly);
                        if (not idx.empty())
                        {
                            update_value(portfolio_model::PrivKey, QString::fromStdString(show_priv_key_answer.priv_key), idx.at(0), *portfolio_mdl);
                            std::error_code ec;
                            QString public_address = QString::fromStdString(m_system_manager.get_system<kdf_service>().address(show_priv_key_answer.coin, ec));
                            update_value(portfolio_model::Address, public_address, idx.at(0), *portfolio_mdl);
                        }
                    }
                }
                this->set_fetching_priv_key_busy(false);
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
        return {QString::fromStdString(seed), QString::fromStdString(kdf::get_rpc_password())};
    }

    QString settings_page::get_version()
    {
        return QString::fromStdString(atomic_dex::get_version());
    }

    QString settings_page::get_commit_hash()
    {
        return QString::fromStdString(atomic_dex::get_commit_hash());
    }

    QString settings_page::get_log_folder()
    {
        return QString::fromStdString(utils::get_atomic_dex_logs_folder().string());
    }

    QString settings_page::get_kdf_version()
    {
        return QString::fromStdString(kdf::rpc_version());
    }

    QString settings_page::get_rpcport()
    {
        return QString::fromStdString(std::to_string(atomic_dex::g_dex_rpcport));
    }

    QString settings_page::get_peerid()
    {
        return QString::fromStdString(kdf::peer_id());
    }

    QString settings_page::get_export_folder()
    {
        return QString::fromStdString(utils::get_atomic_dex_export_folder().string());
    }

    void settings_page::fetchPublicKey()
    {
        auto& kdf_system = m_system_manager.get_system<kdf_service>();
        auto  get_pub_key_rpc_callback = [this](auto pub_key_rpc)
        {
            if (pub_key_rpc.error)
                public_key = tr("An error has occurred.");
            else
                public_key = QString::fromStdString(pub_key_rpc.result->public_key);
            fetching_public_key = false;
            emit publicKeyChanged();
            emit fetchingPublicKeyChanged();
        };

        fetching_public_key = true;
        emit fetchingPublicKeyChanged();

        kdf_system.get_kdf_client().process_rpc_async<atomic_dex::kdf::get_public_key_rpc>(get_pub_key_rpc_callback);
    }
} // namespace atomic_dex
