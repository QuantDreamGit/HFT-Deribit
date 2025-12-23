#include <iostream>
#include <mutex>
#include <condition_variable>

#include "deribit/logging.h"
#include "deribit/deribit_client.h"

/**
 * @brief Flag set when a private RPC response or error is received.
 *
 * This flag is protected by a mutex and used together with a condition
 * variable to block the main thread until the RPC completes.
 */
bool got_private_response = false;

/** Mutex protecting the completion flag. */
std::mutex rpc_mtx;

/** Condition variable used to signal RPC completion. */
std::condition_variable rpc_cv;

/**
 * @brief Private RPC success callback invoked when an RPC result arrives from Deribit.
 *
 * This callback sets the completion flag and wakes the main thread.
 * The ParsedMessage::result field contains the raw JSON result payload.
 *
 * @param pm ParsedMessage describing the RPC response.
 * @param userdata Opaque user pointer forwarded from the dispatcher.
 */
void on_private_response(const deribit::ParsedMessage& pm, void* userdata)
{
	(void)userdata;

	LOG_INFO("Received PRIVATE RPC response");
	LOG_INFO("Result = {}", pm.result);

	{
		std::lock_guard<std::mutex> lock(rpc_mtx);
		got_private_response = true;
	}
	rpc_cv.notify_one();
}

/**
 * @brief Error callback for private RPC responses.
 *
 * Sets the completion flag and wakes the main thread when the RPC
 * returns an error payload.
 *
 * @param pm ParsedMessage containing error fields.
 * @param userdata Opaque user pointer forwarded from the dispatcher.
 */
void on_private_error(const deribit::ParsedMessage& pm, void* userdata)
{
	(void)userdata;

	LOG_ERROR("Private RPC ERROR {} {}", pm.error_code, pm.error_msg);

	{
		std::lock_guard<std::mutex> lock(rpc_mtx);
		got_private_response = true;
	}
	rpc_cv.notify_one();
}

/**
 * @brief Test program that authenticates and issues a private RPC request.
 *
 * The test initializes logging, loads credentials from environment
 * variables, connects the client and waits for authentication. It then
 * registers RPC handlers, sends a private get_user_trades_by_currency
 * RPC and blocks until a response is received.
 *
 * @return int Exit code 0 on success.
 */
int main()
{
	deribit::init_logging();
	SET_LOG_LEVEL(deribit::LogLevel::DEBUG);

	deribit::DeribitClient client;

	LOG_INFO("Connecting...");
	client.connect();

	/**
	 * @brief Wait for authentication token.
	 *
	 * Blocks until authentication completes.
	 */
	while (client.get_access_token().empty()) {
		std::this_thread::yield();
	}

	LOG_INFO("Authenticated. Token = {}", client.get_access_token());

	constexpr uint64_t RPC_ID = 9367;

	/**
	 * @brief Register RPC callbacks for the request id used in the test.
	 */
	client.get_dispatcher().register_rpc(
		RPC_ID,
		on_private_response,
		on_private_error,
		nullptr
	);

	LOG_INFO("Sending private RPC request: get_user_trades_by_currency");

	std::string params = R"({"count":2,"currency":"ETH"})";
	client.send_rpc(RPC_ID, "private/get_user_trades_by_currency", params);

	/**
	 * @brief Block until the RPC response or error arrives.
	 */
	{
		std::unique_lock<std::mutex> lock(rpc_mtx);
		rpc_cv.wait(lock, [] { return got_private_response; });
	}

	LOG_INFO("Private RPC test completed");
	client.close();

	return 0;
}
