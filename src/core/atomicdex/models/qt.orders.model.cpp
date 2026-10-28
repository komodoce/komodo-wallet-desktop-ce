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

#include <chrono>
#include <algorithm>
#include <QTimer>
#include "atomicdex/api/kdf/rpc_v1/rpc.recover_funds_of_swap.hpp"
#include "atomicdex/events/qt.events.hpp"
#include "atomicdex/models/qt.orders.model.hpp"
#include "atomicdex/pages/qt.settings.page.hpp"
#include "atomicdex/services/kdf/kdf.service.hpp"
#include "atomicdex/utilities/qt.utilities.hpp"

//! Constructor
namespace atomic_dex
{
    orders_model::orders_model(ag::ecs::system_manager& system_manager, entt::dispatcher& dispatcher, QObject* parent) :
        QAbstractListModel(parent), m_system_manager(system_manager), m_dispatcher(dispatcher), m_model_proxy(new orders_proxy_model(this, system_manager))
    {
        this->m_model_proxy->setSourceModel(this);
        this->m_model_proxy->setDynamicSortFilter(true);
        this->m_model_proxy->setSortRole(UnixTimestampRole);
        this->m_model_proxy->setFilterRole(TickerPairRole);
        this->m_model_proxy->sort(0, Qt::DescendingOrder);
    }
} // namespace atomic_dex

//! Qt model override
namespace atomic_dex
{
    int
    orders_model::rowCount([[maybe_unused]] const QModelIndex& parent) const
    {
        return static_cast<int>(this->m_model_data.orders_and_swaps.size());
    }

    bool
    orders_model::setData(const QModelIndex& index, const QVariant& value, int role)
    {
        if (!hasIndex(index.row(), index.column(), index.parent()) || !value.isValid())
        {
            return false;
        }

        t_order_swaps_data& item = m_model_data.orders_and_swaps[index.row()];
        switch (static_cast<OrdersRoles>(role))
        {
        case BaseCoinRole:
            item.base_coin = value.toString();
            break;
        case RelCoinRole:
            item.rel_coin = value.toString();
            break;
        case TickerPairRole:
            item.ticker_pair = value.toString();
            break;
        case BaseCoinAmountRole:
            item.base_amount = value.toString();
            break;
        case BaseCoinAmountCurrentCurrencyRole:
            item.base_amount_fiat = value.toString();
            break;
        case RelCoinAmountRole:
            item.rel_amount = value.toString();
            break;
        case RelCoinAmountCurrentCurrencyRole:
            item.rel_amount_fiat = value.toString();
            break;
        case MinVolumeRole:
            item.min_volume = value.toString();
            break;
        case MaxVolumeRole:
            item.max_volume = value.toString();
            break;
        case OrderTypeRole:
            item.order_type = value.toString();
            break;
        case HumanDateRole:
            item.human_date = value.toString();
            break;
        case UnixTimestampRole:
            item.unix_timestamp = value.toULongLong();
            break;
        case PaymentLockRole:
            item.paymentLock = value.toULongLong();
            break;
        case OrderIdRole:
            item.order_id = value.toString();
            break;
        case OrderStatusRole:
            item.order_status = value.toString();
            break;
        case MakerPaymentIdRole:
            item.maker_payment_id = value.toString();
            break;
        case TakerPaymentIdRole:
            item.taker_payment_id = value.toString();
            break;
        case CancellableRole:
            item.is_cancellable = value.toBool();
            break;
        case IsMakerRole:
            item.is_maker = value.toBool();
            break;
        case IsSwapRole:
            item.is_swap = value.toBool();
            break;
        case IsRecoverableRole:
            item.is_recoverable = value.toBool();
            break;
        case OrderErrorStateRole:
            item.order_error_state = value.toString();
            break;
        case OrderErrorMessageRole:
            item.order_error_message = value.toString();
            break;
        case EventsRole:
            item.events = value.toJsonArray();
            break;
        case SuccessEventsRole:
            item.success_events = value.toStringList();
            break;
        case ErrorEventsRole:
            item.error_events = value.toStringList();
            break;
        default:
            break;
        }

        emit dataChanged(index, index, {role});
        return true;
    }

    QVariant
    orders_model::data(const QModelIndex& index, int role) const
    {
        if (!hasIndex(index.row(), index.column(), index.parent()))
        {
            return {};
        }

        const t_order_swaps_data& item = m_model_data.orders_and_swaps.at(index.row());
        switch (static_cast<OrdersRoles>(role))
        {
        case BaseCoinRole:
            return item.base_coin;
        case RelCoinRole:
            return item.rel_coin;
        case TickerPairRole:
            return item.ticker_pair;
        case BaseCoinAmountRole:
            return item.base_amount;
        case BaseCoinAmountCurrentCurrencyRole:
            return item.base_amount_fiat;
        case RelCoinAmountRole:
            return item.rel_amount;
        case RelCoinAmountCurrentCurrencyRole:
            return item.rel_amount_fiat;
        case MinVolumeRole:
            return item.min_volume;
        case MaxVolumeRole:
            return item.max_volume;
        case OrderTypeRole:
            return item.order_type;
        case HumanDateRole:
            return item.human_date;
        case UnixTimestampRole:
            return item.unix_timestamp;
        case PaymentLockRole:
            return item.paymentLock;
        case OrderIdRole:
            return item.order_id;
        case OrderStatusRole:
            return item.order_status;
        case MakerPaymentIdRole:
            return item.maker_payment_id;
        case TakerPaymentIdRole:
            return item.taker_payment_id;
        case CancellableRole:
            return item.is_cancellable;
        case IsMakerRole:
            return item.is_maker;
        case IsSwapRole:
            return item.is_swap;
        case IsRecoverableRole:
            return item.is_recoverable;
        case OrderErrorStateRole:
            return item.order_error_state;
        case OrderErrorMessageRole:
            return item.order_error_message;
        case EventsRole:
            return item.events;
        case SuccessEventsRole:
            return item.success_events;
        case ErrorEventsRole:
            return item.error_events;
        default:
            break;
        }
        return {};
    }

    bool
    orders_model::removeRows(int position, int rows, [[maybe_unused]] const QModelIndex& parent)
    {
        beginRemoveRows(QModelIndex(), position, position + rows - 1);
        for (int i = position + rows - 1; i >= position; --i)
        {
            this->m_model_data.orders_and_swaps.erase(begin(m_model_data.orders_and_swaps) + i);
        }
        endRemoveRows();
        emit lengthChanged();
        return true;
    }

    QHash<int, QByteArray>
    orders_model::roleNames() const
    {
        return {
            {BaseCoinRole, "base_coin"},
            {RelCoinRole, "rel_coin"},
            {TickerPairRole, "ticker_pair"},
            {BaseCoinAmountRole, "base_amount"},
            {BaseCoinAmountCurrentCurrencyRole, "base_amount_current_currency"},
            {RelCoinAmountRole, "rel_amount"},
            {RelCoinAmountCurrentCurrencyRole, "rel_amount_current_currency"},
            {MinVolumeRole, "min_volume"},
            {MaxVolumeRole, "max_volume"},
            {OrderTypeRole, "type"},
            {IsMakerRole, "is_maker"},
            {HumanDateRole, "date"},
            {UnixTimestampRole, "timestamp"},
            {PaymentLockRole, "paymentLock"},
            {OrderIdRole, "order_id"},
            {OrderStatusRole, "order_status"},
            {MakerPaymentIdRole, "maker_payment_id"},
            {TakerPaymentIdRole, "taker_payment_id"},
            {IsSwapRole, "is_swap"},
            {CancellableRole, "cancellable"},
            {IsRecoverableRole, "recoverable"},
            {OrderErrorStateRole, "order_error_state"},
            {OrderErrorMessageRole, "order_error_message"},
            {EventsRole, "events"},
            {SuccessEventsRole, "success_events"},
            {ErrorEventsRole, "error_events"}};
    }
} // namespace atomic_dex

//! Properties
namespace atomic_dex
{
    int
    orders_model::get_length() const
    {
        return this->m_model_proxy->rowCount(QModelIndex());
    }

    orders_proxy_model*
    orders_model::get_orders_proxy_mdl() const
    {
        return m_model_proxy;
    }

    QVariant
    atomic_dex::orders_model::get_average_events_time_registry() const
    {
        return m_json_time_registry;
    }

    void
    atomic_dex::orders_model::set_average_events_time_registry(const QVariant& average_time_registry)
    {
        m_json_time_registry = average_time_registry;
        emit onAverageEventsTimeRegistryChanged();
    }

    int
    orders_model::get_current_page() const
    {
        return static_cast<int>(m_model_data.current_page);
    }

    void
    orders_model::set_current_page(int current_page)
    {
        if (this->is_fetching_busy())
        {
            SPDLOG_WARN("set_current_page ignored (fetching busy), requested={}", current_page);
            return;
        }

        if (static_cast<std::size_t>(current_page) != m_model_data.current_page)
        {
            this->set_fetching_busy(true);
            this->reset_backend("set_current_page");
            auto& kdf = this->m_system_manager.get_system<kdf_service>();
            kdf.set_orders_and_swaps_pagination_infos(static_cast<std::size_t>(current_page), m_model_data.limit, m_model_data.filtering_infos);
        }
    }

    int
    orders_model::get_limit_nb_elements() const
    {
        return static_cast<int>(m_model_data.limit);
    }

    void
    orders_model::set_limit_nb_elements(int limit)
    {
        if (static_cast<std::size_t>(limit) != m_model_data.limit)
        {
            this->m_model_data.limit = limit;

            if (m_model_data.current_page == 1)
            {
                this->set_fetching_busy(true);
                this->reset_backend("set_limit_nb_elements");
                auto& kdf = this->m_system_manager.get_system<kdf_service>();
                kdf.set_orders_and_swaps_pagination_infos(m_model_data.current_page, static_cast<std::size_t>(limit), m_model_data.filtering_infos);
            }
            else
            {
                this->set_current_page(1);
            }
        }
    }

    int
    orders_model::get_total_swaps() const
    {
        return static_cast<int>(m_model_data.total_swaps);
    }

    QVariant
    orders_model::get_recover_fund_data() const
    {
        return m_recover_funds_data.get();
    }

    void
    orders_model::set_recover_fund_data(QVariant rpc_data)
    {
        auto json_result     = rpc_data.toJsonObject();
        m_recover_funds_data = json_result;
        emit recoverFundDataChanged();
    }

    bool
    orders_model::is_fetching_busy() const
    {
        return m_fetching_busy.load();
    }

    bool
    orders_model::is_recover_fund_busy() const
    {
        return m_recover_funds_busy.load();
    }

    void
    orders_model::set_fetching_busy(bool fetching_status)
    {
        if (fetching_status != m_fetching_busy)
        {
            m_fetching_busy = fetching_status;
            emit fetchingStatusChanged();
        }
    }

    void
    orders_model::set_recover_fund_busy(bool recover_funds_status)
    {
        if (recover_funds_status != m_recover_funds_busy)
        {
            m_recover_funds_busy = recover_funds_status;
            emit recoverFundBusyChanged();
        }
    }

    int
    orders_model::get_nb_pages() const
    {
        return static_cast<int>(m_model_data.nb_pages);
    }
} // namespace atomic_dex

//! Private API
namespace atomic_dex
{
    void
    orders_model::update_existing_order(const t_order_swaps_data& contents)
    {
        if (const auto res = this->match(index(0, 0), OrderIdRole, contents.order_id); !res.isEmpty())
        {
            const QModelIndex& idx = res.at(0);
            update_value(OrdersRoles::CancellableRole, contents.is_cancellable, idx, *this);
            update_value(OrdersRoles::IsMakerRole, contents.order_type == "maker", idx, *this);
            update_value(OrdersRoles::OrderTypeRole, contents.order_type, idx, *this);
            update_value(OrdersRoles::BaseCoinAmountCurrentCurrencyRole, contents.base_amount_fiat, idx, *this);
            update_value(OrdersRoles::RelCoinAmountCurrentCurrencyRole, contents.rel_amount_fiat, idx, *this);

            if (contents.order_type == "maker")
            {
                update_value(OrdersRoles::BaseCoinAmountRole, contents.base_amount, idx, *this);
                update_value(OrdersRoles::RelCoinAmountRole, contents.rel_amount, idx, *this);
            }
        }
    }

    void
    orders_model::update_swap(const t_order_swaps_data& contents)
    {
        if (const auto res = this->match(index(0, 0), OrderIdRole, contents.order_id); !res.isEmpty())
        {
            const QModelIndex& idx = res.at(0);
            update_value(OrdersRoles::IsRecoverableRole, contents.is_recoverable, idx, *this);
            auto&& [prev_value, is_change] = update_value(OrdersRoles::OrderStatusRole, contents.order_status, idx, *this);

            update_value(OrdersRoles::UnixTimestampRole, contents.unix_timestamp, idx, *this);
            update_value(OrdersRoles::PaymentLockRole, contents.paymentLock, idx, *this);
            auto&& [prev_value_d, __] = update_value(OrdersRoles::HumanDateRole, contents.human_date, idx, *this);

            if (is_change)
            {
                const QString& base_coin = data(idx, OrdersRoles::BaseCoinRole).toString();
                const QString& rel_coin  = data(idx, OrdersRoles::RelCoinRole).toString();
                m_dispatcher.trigger(
                    swap_status_notification{.uuid = contents.order_id,
                                             .prev_status = prev_value.toString(),
                                             .new_status = contents.order_status,
                                             .base = base_coin,
                                             .rel = rel_coin,
                                             .human_date = contents.human_date});
                auto& kdf = m_system_manager.get_system<kdf_service>();
                kdf.process_orderbook(true);
            }

            update_value(OrdersRoles::MakerPaymentIdRole, contents.maker_payment_id, idx, *this);
            update_value(OrdersRoles::TakerPaymentIdRole, contents.taker_payment_id, idx, *this);
            update_value(OrdersRoles::OrderErrorStateRole, contents.order_error_state, idx, *this);
            update_value(OrdersRoles::OrderErrorMessageRole, contents.order_error_message, idx, *this);
            update_value(OrdersRoles::EventsRole, contents.events, idx, *this);
            update_value(OrdersRoles::SuccessEventsRole, contents.success_events, idx, *this);
            update_value(OrdersRoles::ErrorEventsRole, contents.error_events, idx, *this);
            update_value(OrdersRoles::BaseCoinAmountCurrentCurrencyRole, contents.base_amount_fiat, idx, *this);
            update_value(OrdersRoles::RelCoinAmountCurrentCurrencyRole, contents.rel_amount_fiat, idx, *this);

            //emit lengthChanged();
        }
    }

    void
    orders_model::init_model(const orders_and_swaps& contents)
    {
        const auto size = contents.orders_and_swaps.size();
        if (size == 0)
            return;

        beginResetModel();
        m_model_data = contents;
        m_orders_id_registry = std::move(m_model_data.orders_registry);
        m_swaps_id_registry  = std::move(m_model_data.swaps_registry);
        endResetModel();
        emit lengthChanged();
        emit currentPageChanged();
        emit limitNbElementsChanged();
        emit nbPageChanged();
        emit totalSwapsChanged();
        this->set_average_events_time_registry(nlohmann_json_object_to_qt_json_object(m_model_data.average_events_time));
    }

    void
    orders_model::common_insert(const std::vector<t_order_swaps_data>& contents, const std::string& kind)
    {
        auto& data = m_model_data.orders_and_swaps;
        beginInsertRows(QModelIndex(), rowCount(), rowCount() + static_cast<int>(contents.size()) - 1);
        data.insert(end(data), begin(contents), end(contents));
        if (kind == "orders")
        {
            m_model_data.nb_orders += contents.size();
        }
        endInsertRows();
        emit lengthChanged();

        // Fire notifications for newly initialized active swaps
        if (kind == "swaps")
        {
            for (auto&& cur : contents)
            {
                // Trigger notification instantly on discovery for new uncompleted tracking loops
                if (cur.order_status == "matching" || cur.order_status == "ongoing" || cur.order_status == "matched")
                {
                    m_dispatcher.trigger(
                        swap_status_notification{.uuid = cur.order_id,
                                                 .prev_status = "None",
                                                 .new_status = cur.order_status,
                                                 .base = cur.base_coin,
                                                 .rel = cur.rel_coin,
                                                 .human_date = cur.human_date});
                }
            }
        }

        if (m_system_manager.has_system<kdf_service>())
        {
            this->m_system_manager.get_system<kdf_service>().process_orderbook(true);
        }
    }

    void
    orders_model::update_or_insert_swaps(const orders_and_swaps& contents)
    {
        const auto&                     data = contents.orders_and_swaps;
        std::vector<t_order_swaps_data> to_init;
        bool                            was_updated = false;
        bool                            requires_filter_invalidation = false; // Track tab transitions

        auto& model_array = this->m_model_data.orders_and_swaps;

        std::for_each(
            begin(data), end(data),
            [this, &to_init, &was_updated, &requires_filter_invalidation, &model_array](const auto& cur)
            {
                if (cur.is_swap)
                {
                    const auto& uuid = cur.order_id.toStdString();

                    auto it = std::find_if(model_array.begin(), model_array.end(),
                        [&cur](const t_order_swaps_data& item) { return item.order_id == cur.order_id; });

                    if (it != model_array.end())
                    {
                        if (it->order_status != cur.order_status)
                        {
                            m_dispatcher.trigger(
                                swap_status_notification{.uuid = cur.order_id,
                                                         .prev_status = it->order_status,
                                                         .new_status = cur.order_status,
                                                         .base = it->base_coin,
                                                         .rel = it->rel_coin,
                                                         .human_date = cur.human_date});

                            // If a swap shifts to finished, flag it for a tab migration pass
                            if (cur.order_status == "successful" || cur.order_status == "failed")
                            {
                                requires_filter_invalidation = true;
                            }
                        }

                        *it = cur;
                        was_updated = true;
                    }
                    else
                    {
                        if (!this->m_swaps_id_registry.contains(uuid))
                        {
                            to_init.emplace_back(cur);
                            m_swaps_id_registry.emplace(uuid);
                        }
                    }
                }
            });

        if (was_updated)
        {
            emit dataChanged(index(0, 0), index(rowCount() - 1, 0));

            if (requires_filter_invalidation)
            {
                QMetaObject::invokeMethod(this->m_model_proxy, [this]() {
                    QTimer::singleShot(10, this->m_model_proxy, [this]() {
                        this->m_model_proxy->refresh_filter();
                    });
                }, Qt::QueuedConnection);
            }
        }

        if (!to_init.empty())
        {
            this->common_insert(to_init, "swaps");
        }
    }

    void
    orders_model::update_or_insert_orders(const orders_and_swaps& contents)
    {
        const auto&                     data = contents.orders_and_swaps;
        std::unordered_set<std::string> are_present;
        std::vector<t_order_swaps_data> to_init;
        bool                            was_updated = false;

        auto& model_array = this->m_model_data.orders_and_swaps;

        std::for_each(
            begin(data), end(data),
            [this, &to_init, &are_present, &was_updated, &model_array](const auto& cur)
            {
                if (!cur.is_swap)
                {
                    std::string uuid_str = cur.order_id.toStdString();
                    auto it = std::find_if(model_array.begin(), model_array.end(),
                        [&cur](const t_order_swaps_data& item) { return item.order_id == cur.order_id; });

                    if (it != model_array.end())
                    {
                        *it = cur;
                        was_updated = true;
                    }
                    else
                    {
                        to_init.emplace_back(cur);
                        m_orders_id_registry.emplace(uuid_str);
                    }
                    are_present.emplace(uuid_str);
                }
            });

        if (!to_init.empty())
        {
            this->common_insert(to_init, "orders");
        }

        if (was_updated)
        {
            emit dataChanged(index(0, 0), index(rowCount() - 1, 0));
        }

        remove_orders(are_present);
    }

    void
    orders_model::remove_orders(const t_orders_id_registry& are_present)
    {
        std::vector<std::string> to_remove;

        for (auto&& id: this->m_orders_id_registry)
        {
            if (!are_present.contains(id))
            {
                auto res_list = this->match(index(0, 0), OrderIdRole, QString::fromStdString(id));
                if (!res_list.empty())
                {
                    int target_row = res_list.at(0).row();
                    if (target_row >= 0 && target_row < rowCount())
                    {
                        if (m_model_data.orders_and_swaps[target_row].is_swap)
                        {
                            continue;
                        }
                    }

                    this->removeRow(target_row);
                    m_model_data.nb_orders -= 1;
                    to_remove.emplace_back(id);
                }
            }
        }
        for (auto&& cur_to_remove: to_remove) { m_orders_id_registry.erase(cur_to_remove); }
    }

    void
    orders_model::set_common_data(const orders_and_swaps& contents)
    {
        this->set_average_events_time_registry(nlohmann_json_object_to_qt_json_object(contents.average_events_time));

        if (m_model_data.nb_pages != contents.nb_pages)
        {
            m_model_data.nb_pages = contents.nb_pages;
            emit nbPageChanged();
        }

        if (m_model_data.limit != contents.limit)
        {
            SPDLOG_DEBUG("UNUSED");
            m_model_data.limit = contents.limit;
            emit limitNbElementsChanged();
        }

        if (m_model_data.current_page != contents.current_page)
        {
            SPDLOG_DEBUG("UNUSED");
            m_model_data.current_page = contents.current_page;
            emit currentPageChanged();
        }

        if (m_model_data.total_swaps != contents.total_swaps)
        {
            m_model_data.total_swaps = contents.total_swaps;
            emit totalSwapsChanged();
        }
    }
} // namespace atomic_dex

//! Public API
namespace atomic_dex
{
    void
    orders_model::reset()
    {
        this->beginResetModel();
        reset_backend("reset");
        this->endResetModel();
        this->set_fetching_busy(false);
    }

    void
    orders_model::reset_backend([[maybe_unused]] const std::string& from)
    {
        const auto limit     = this->m_model_data.limit;
        const auto filtering = this->m_model_data.filtering_infos;
        this->m_swaps_id_registry.clear();
        this->m_orders_id_registry.clear();
        this->m_model_data = {.limit = limit, .filtering_infos = filtering};
    }

    bool
    atomic_dex::orders_model::swap_is_in_progress(const QString& coin) const
    {
        auto functor = [coin](const auto& cur_hist_swap)
        {
            return (cur_hist_swap.base_coin == coin || cur_hist_swap.rel_coin == coin) &&
                   (cur_hist_swap.order_status == "matched" || cur_hist_swap.order_status == "ongoing" || cur_hist_swap.order_status == "matching");
        };

        return std::any_of(m_model_data.orders_and_swaps.begin(), m_model_data.orders_and_swaps.end(), functor);
    }

    void
    orders_model::refresh_or_insert([[maybe_unused]] utils::caller_location location)
    {
        const auto& kdf      = m_system_manager.get_system<kdf_service>();
        const auto  contents = kdf.get_orders_and_swaps();

        if (m_model_data.orders_and_swaps.empty())
        {
            init_model(contents);
        }
        else
        {
            update_or_insert_orders(contents);
            update_or_insert_swaps(contents);
            this->set_common_data(contents);
            m_model_data.nb_orders = contents.nb_orders;
        }

        if (this->is_fetching_busy())
        {
            this->set_fetching_busy(false);
        }
    }

    void
    orders_model::set_filtering_infos(t_filtering_infos infos, [[maybe_unused]] utils::caller_location location)
    {
        if (this->is_fetching_busy())
        {
            SPDLOG_WARN("orders_model::set_filtering_infos is busy called by: {} ({}:{})", location.function_name(), location.file_name(), location.line());
            return;
        }

        m_model_data.filtering_infos = std::move(infos);

        if (m_model_data.current_page == 1 && m_model_proxy->am_i_in_history())
        {
            this->set_fetching_busy(true);
            this->reset();

            if (this->m_system_manager.has_system<kdf_service>())
            {
                auto& kdf = this->m_system_manager.get_system<kdf_service>();
                kdf.set_orders_and_swaps_pagination_infos(m_model_data.current_page, m_model_data.limit, m_model_data.filtering_infos);
            }
        }
        else
        {
            this->set_current_page(1);
        }
    }

    t_filtering_infos
    orders_model::get_filtering_infos() const
    {
        return m_model_data.filtering_infos;
    }

    void
    orders_model::recover_fund(QString uuid)
    {
        this->set_recover_fund_busy(true);

        auto&                                   kdf_system = m_system_manager.get_system<kdf_service>();
        nlohmann::json                          batch      = nlohmann::json::array();
        nlohmann::json                          json_data  = kdf::template_request("recover_funds_of_swap");

        kdf::recover_funds_of_swap_request req{.swap_uuid = uuid.toStdString()};
        kdf::to_json(json_data, req);
        batch.push_back(json_data);

        auto answer_functor = [this](t_http_response resp)
        {
            nlohmann::json j_out = nlohmann::json::object();
            std::string    body  = (resp.extract_string(true).get());

            SPDLOG_DEBUG("recover_funds_of_swap answer received: {}", body);

            if (resp.status_code() == http_status_codes::ok)
            {
                auto answers        = nlohmann::json::parse(body);
                auto recover_answer = kdf::rpc_process_answer_batch<t_recover_funds_of_swap_answer>(answers[0], "recover_funds_of_swap");

                if (recover_answer.result.has_value())
                {
                    auto answer                    = recover_answer.result.value();
                    j_out["is_valid"]              = true;
                    nlohmann::json success_payload = nlohmann::json::object();
                    success_payload["action"]      = answer.action;
                    success_payload["coin"]        = answer.coin;
                    success_payload["tx_hash"]     = answer.tx_hash;
                    success_payload["tx_hex"]      = answer.tx_hex;
                    j_out["result"]                = success_payload;
                }
                else if (recover_answer.error.has_value())
                {
                    j_out["is_valid"] = false;
                    j_out["error"]    = recover_answer.error.value();
                }
                else
                {
                    j_out["is_valid"] = false;
                    j_out["error"]    = recover_answer.raw_result;
                }
            }
            else if (resp.status_code() == http_status_codes::request_timeout)
            {
                j_out["is_valid"] = false;
                j_out["error"]    = "Request to kdf timeout - skipping";
            }
            else
            {
                j_out["is_valid"] = false;
                j_out["error"]    = body;
            }

            this->set_recover_fund_data(nlohmann_json_object_to_qt_json_object(j_out));
            this->set_recover_fund_busy(false);
        };

        kdf_system.get_kdf_client()
            .async_rpc_batch_standalone(
                std::move(batch),
                t_http_priority::interactive,
                std::chrono::minutes(10))
            .then(
            [this, answer_functor](async::task<t_http_response> previous_task)
            {
                try
                {
                    answer_functor(previous_task.get());
                }
                catch (const std::exception& e)
                {
                    SPDLOG_ERROR("exception in orders_model::recover_fund(QString uuid): {}", e.what());
                    nlohmann::json j_out = nlohmann::json::object();
                    j_out["is_valid"]    = false;
                    j_out["error"]       = e.what();
                    this->set_recover_fund_data(nlohmann_json_object_to_qt_json_object(j_out));
                    this->set_recover_fund_busy(false);
                }
            });
    }
} // namespace atomic_dex
