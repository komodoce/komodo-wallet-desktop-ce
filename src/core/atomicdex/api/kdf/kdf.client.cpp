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
#include <filesystem>
#include <meta/detection/detection.hpp>
#include "kdf.hpp"
#include "atomicdex/api/kdf/rpc.hpp"
#include "kdf.client.hpp"
#include "rpc.tx.history.hpp"
#include "atomicdex/constants/dex.constants.hpp"
#include "atomicdex/api/kdf/rpc_v1/rpc.my_tx_history.hpp"
#include "atomicdex/api/kdf/rpc_v2/rpc2.get_public_key.hpp"
#include "atomicdex/api/kdf/rpc_v2/rpc2.my_tx_history.hpp"
#include "atomicdex/api/kdf/rpc_v2/rpc2.orderbook.hpp"
#include "atomicdex/api/kdf/rpc_v2/rpc2.bestorders.hpp"
#include "atomicdex/api/kdf/rpc_v2/rpc2.enable_tendermint_token.hpp"
#include "atomicdex/api/kdf/rpc_v2/rpc2.enable_tendermint_with_assets.hpp"
#include "atomicdex/api/kdf/rpc_v2/rpc2.enable_erc20.hpp"
#include "atomicdex/api/kdf/rpc_v2/rpc2.enable_eth_with_tokens.hpp"

namespace
{
    template <typename T>
    using have_error_field = decltype(std::declval<T&>().error.has_value());

    t_http_client generate_client(std::chrono::seconds timeout = std::chrono::seconds::zero())
    {
        atomic_dex::http::client_config cfg;
        if (timeout > std::chrono::seconds::zero())
        {
            cfg.set_timeout(timeout);
        }
        return t_http_client(atomic_dex::g_dex_rpc, cfg);
    }

    template <atomic_dex::kdf::rpc Rpc>
    t_http_request make_request(typename Rpc::expected_request_type data_req = {})
    {
        t_http_request request;
        nlohmann::json json_req = {{"method", Rpc::endpoint}, {"userpass", atomic_dex::kdf::get_rpc_password()}};
        nlohmann::json json_data;

        nlohmann::to_json(json_data, data_req);
        request.set_method(http_method::POST);
        if (Rpc::is_v2)
        {
            json_req["mmrpc"] = "2.0";
            json_req["id"] = 42;
            json_req.push_back({"params", json_data});
        }
        else
        {
            json_req.insert(json_req.end(), json_data);
        }
        request.set_body(json_req.dump());
        // SPDLOG_DEBUG("request: {}", json_req.dump());
        return request;
    }

    template <atomic_dex::kdf::rpc Rpc>
    Rpc process_rpc_answer(const t_http_response& answer)
    {
        std::string body = (answer.extract_string(true).get());
        Rpc rpc;
        rpc.raw_result = body;

        //! Every answer leaves here with exactly one of result / error set.
        //! Callers branch on `if (rpc.error) ... else rpc.result->...`, so an
        //! answer with neither is read as success and dereferences an empty
        //! optional -- which is what crashed the wallet when KDF reported a
        //! platform activation failure the error parser could not read.
        const auto fail = [&rpc, &answer](std::string reason)
        {
            SPDLOG_ERROR("unusable KDF answer (HTTP {}): {} | body: {}", answer.status_code(), reason, rpc.raw_result);
            rpc.result.reset();
            typename Rpc::expected_error_type error;
            error.error      = std::move(reason);
            error.error_type = "UnusableAnswer";
            rpc.error        = std::move(error);
            return rpc;
        };

        nlohmann::json json_answer;
        if (body.empty() || !nlohmann::json::accept(body))
        {
            return fail("empty or non-JSON answer");
        }
        try
        {
            json_answer = nlohmann::json::parse(body);
        }
        catch (const nlohmann::json::parse_error& error)
        {
            return fail(std::string("cannot parse answer: ") + error.what());
        }

        try
        {
            if constexpr (Rpc::is_v2)
            {
                if (answer.status_code() == 200 && json_answer.contains("result"))
                {
                    rpc.result     = json_answer.at("result").get<typename Rpc::expected_result_type>();
                    rpc.raw_result = json_answer.at("result").dump();
                }
                else if (json_answer.contains("error"))
                {
                    rpc.error      = json_answer.get<typename Rpc::expected_error_type>();
                    rpc.raw_result = json_answer.dump();
                }
                else
                {
                    return fail("answer has neither result nor error");
                }
            }
            else
            {
                rpc.result = json_answer.get<typename Rpc::expected_result_type>();
            }
        }
        catch (const std::exception& error)
        {
            return fail(std::string("cannot read answer: ") + error.what());
        }
        return rpc;
    }
} // namespace

namespace atomic_dex::kdf
{
    template <typename RpcReturnType>
    RpcReturnType kdf_client::rpc_process_answer(const t_http_response& resp, const std::string& rpc_command)
    {
        std::string body = (resp.extract_string(true).get());
        RpcReturnType answer;

        try
        {
            if (resp.status_code() not_eq 200)
            {
                answer.rpc_result_code = resp.status_code();
                answer.raw_result      = body;

                // Only attempt logic if it's metadata-detectable and valid JSON
                if constexpr (doom::meta::is_detected_v<have_error_field, RpcReturnType>)
                {
                    if constexpr (std::is_same_v<std::optional<std::string>, decltype(answer.error)>)
                    {
                        // Check if the body contains valid JSON without throwing exceptions
                        if (!body.empty() && nlohmann::json::accept(body))
                        {
                            auto json_data = nlohmann::json::parse(body);
                            if (json_data.contains("error") && json_data.at("error").is_string())
                            {
                                answer.error = json_data.at("error").get<std::string>();
                            }
                            else
                            {
                                answer.error = body;
                            }
                        }
                        else
                        {
                            // It's plain text (like "Not Found") or empty, assign directly
                            answer.error = body.empty() ? "HTTP Status " + std::to_string(resp.status_code()) : body;
                        }
                    }
                }

                //SPDLOG_DEBUG("kdf_client::rpc_process_answer: gracefully handled status {} for command {}", resp.status_code(), rpc_command);
                return answer;
            }

            auto json_answer       = nlohmann::json::parse(body);
            answer.rpc_result_code = resp.status_code();
            answer.raw_result      = body;
            from_json(json_answer, answer);
        }
        catch (const std::exception& error)
        {
            answer.rpc_result_code = -1;
            answer.raw_result      = error.what();
            SPDLOG_ERROR("exception in kdf_client::rpc_process_answer for rpc_command {} with body {} and answer.raw_result: {}", rpc_command, body, answer.raw_result);
        }

        return answer;
    }

    async::task<t_http_response>
    kdf_client::async_rpc_batch_standalone(nlohmann::json&& batch_array, t_http_priority prio, std::chrono::seconds timeout)
    {
        auto& scheduler = (prio == t_http_priority::background)
                          ? atomic_dex::http::client::get_background_scheduler()
                          : atomic_dex::http::client::get_interactive_scheduler();

        return async::spawn(scheduler, [batch_array = std::move(batch_array), prio, timeout]() mutable {
            try
            {
                t_http_request request;
                request.set_method(http_method::POST);
                request.set_body(batch_array.dump());
                auto resp = generate_client(timeout).request(request, prio);
                return resp.get();
            }
            catch (const std::exception& error)
            {
                nlohmann::json masked_batch = batch_array;
                for (auto& item : masked_batch) {
                    if (item.contains("userpass")) {
                        item["userpass"] = "*******";
                    }
                }
                SPDLOG_ERROR("exception in kdf_client::async_rpc_batch_standalone: {} | Request payload: {}", error.what(), masked_batch.dump());
                throw;
            }
        });
    }

    template <rpc Rpc>
    void kdf_client::process_rpc_async(const std::function<void(Rpc)>& on_rpc_processed, t_http_priority prio)
    {
        using request_type = typename Rpc::expected_request_type;
        process_rpc_async<Rpc>(request_type{}, on_rpc_processed, prio);
    }

    template <kdf::rpc Rpc>
    void kdf_client::process_rpc_async(typename Rpc::expected_request_type request, const std::function<void(Rpc)>& on_rpc_processed, t_http_priority prio)
    {
        auto http_request = make_request<Rpc>(request);

        auto& scheduler = (prio == t_http_priority::background)
                          ? atomic_dex::http::client::get_background_scheduler()
                          : atomic_dex::http::client::get_interactive_scheduler();

        generate_client()
            .request(http_request, prio)
            .then(scheduler, [on_rpc_processed, request](const t_http_response& resp)
            {
                if (atomic_dex::kdf::get_rpc_password().empty())
                {
                    return;
                }

                try
                {
                    auto rpc = process_rpc_answer<Rpc>(resp);
                    rpc.request = request;
                    on_rpc_processed(rpc);
                    nlohmann::json json_data;
                    nlohmann::to_json(json_data, request);
                }
                catch (const std::exception& ex)
                {
                    SPDLOG_ERROR("exception in kdf_client::process_rpc_async: {}", ex.what());
                }
            });
    }

    template void kdf_client::process_rpc_async<orderbook_rpc>(const std::function<void(orderbook_rpc)>&, t_http_priority);
    template void kdf_client::process_rpc_async<bestorders_rpc>(const std::function<void(bestorders_rpc)>&, t_http_priority);
    template void kdf_client::process_rpc_async<enable_erc20_rpc>(const std::function<void(enable_erc20_rpc)>&, t_http_priority);
    template void kdf_client::process_rpc_async<get_public_key_rpc>(const std::function<void(get_public_key_rpc)>&, t_http_priority);
    template void kdf_client::process_rpc_async<my_tx_history_v1_rpc>(const std::function<void(my_tx_history_v1_rpc)>&, t_http_priority);
    template void kdf_client::process_rpc_async<my_tx_history_v2_rpc>(const std::function<void(my_tx_history_v2_rpc)>&, t_http_priority);
    template void kdf_client::process_rpc_async<enable_eth_with_tokens_rpc>(const std::function<void(enable_eth_with_tokens_rpc)>&, t_http_priority);
    template void kdf_client::process_rpc_async<enable_tendermint_token_rpc>(const std::function<void(enable_tendermint_token_rpc)>&, t_http_priority);
    template void kdf_client::process_rpc_async<enable_tendermint_with_assets_rpc>(const std::function<void(enable_tendermint_with_assets_rpc)>&, t_http_priority);

    template <typename TRequest, typename TAnswer>
    TAnswer
    kdf_client::process_rpc(TRequest&& request, std::string rpc_command, bool is_v2, std::chrono::seconds timeout)
    {
        nlohmann::json json_data = kdf::template_request(rpc_command, is_v2);
        kdf::to_json(json_data, request);
        auto json_copy        = json_data;
        json_copy["userpass"] = "*******";

        t_http_request rpc_request(http_method::POST);
        rpc_request.headers().set_content_type(("application/json"));
        rpc_request.set_body(json_data.dump());
        //! The request task throws when KDF cannot be reached. The callers are
        //! synchronous and run on the Qt main thread (disable coin, recover
        //! funds), where an escaping exception terminates the process -- so a
        //! transport failure becomes an ordinary failed answer instead.
        try
        {
            auto resp = generate_client(timeout).request(rpc_request).get();
            return rpc_process_answer<TAnswer>(resp, rpc_command);
        }
        catch (const std::exception& error)
        {
            SPDLOG_ERROR("exception in kdf_client::process_rpc for rpc_command {}: {} | Request payload: {}", rpc_command, error.what(), json_copy.dump());
            TAnswer answer;
            answer.rpc_result_code = -1;
            answer.raw_result      = error.what();
            if constexpr (doom::meta::is_detected_v<have_error_field, TAnswer>)
            {
                if constexpr (std::is_same_v<std::optional<std::string>, decltype(answer.error)>)
                {
                    answer.error = error.what();
                }
            }
            return answer;
        }
    }

    t_enable_z_coin_cancel_answer
    kdf_client::rpc_enable_z_coin_cancel(t_enable_z_coin_cancel_request&& request)
    {
        return process_rpc<t_enable_z_coin_cancel_request, t_enable_z_coin_cancel_answer>(std::forward<t_enable_z_coin_cancel_request>(request), "task::enable_z_coin::cancel", true);
    }

    t_disable_coin_answer
    kdf_client::rpc_disable_coin(t_disable_coin_request&& request)
    {
        return process_rpc<t_disable_coin_request, t_disable_coin_answer>(std::forward<t_disable_coin_request>(request), "disable_coin");
    }

    t_recover_funds_of_swap_answer
    kdf_client::rpc_recover_funds(t_recover_funds_of_swap_request&& request)
    {
        return process_rpc<t_recover_funds_of_swap_request, t_recover_funds_of_swap_answer>(
            std::forward<t_recover_funds_of_swap_request>(request),
            "recover_funds_of_swap",
            false,
            std::chrono::minutes(10));
    }
} // namespace atomic_dex

template atomic_dex::kdf::tx_history_answer   atomic_dex::kdf::kdf_client::rpc_process_answer(const t_http_response& resp, const std::string& rpc_command);
template atomic_dex::kdf::disable_coin_answer atomic_dex::kdf::kdf_client::rpc_process_answer(const t_http_response& resp, const std::string& rpc_command);
