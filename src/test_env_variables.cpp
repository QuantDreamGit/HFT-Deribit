#include <iostream>
#include <atomic>

#include "deribit/logging.h"
#include "deribit/deribit_client.h"

/**
 * @brief Flag set when a private RPC response or error is received.
 *
 * This atomic is used by the main test loop to detect when an RPC
 * response has arrived so the program can finish.
 */
std::atomic<bool> got_private_response = false;

/**
 * @brief Private RPC success callback invoked when an RPC result arrives from Deribit.
 *
 * This callback sets the got_private_response flag and logs the
 * received result. The ParsedMessage::result field contains the raw JSON
 * result payload.
 *
 * @param pm ParsedMessage describing the RPC response.
 * @param userdata Opaque user pointer forwarded from the dispatcher.
 */
void on_private_response(const deribit::ParsedMessage& pm, void* userdata)
{
	got_private_response = true;

	LOG_INFO("Received PRIVATE RPC response");
	LOG_INFO("Result = {}", pm.result);
}

/**
 * @brief Error callback for private RPC responses.
 *
 * Sets the got_private_response flag and logs the error code and message
 * when the RPC returns an error payload.
 *
 * @param pm ParsedMessage containing error fields.
 * @param userdata Opaque user pointer forwarded from the dispatcher.
 */
void on_private_error(const deribit::ParsedMessage& pm, void* userdata)
{
	got_private_response = true;
	LOG_ERROR("Private RPC ERROR {} {}", pm.error_code, pm.error_msg);
}

/**
 * @brief Test program that authenticates and issues a private RPC request.
 *
 * The test initializes logging, loads credentials from environment
 * variables, connects the client and waits for authentication. It then
 * registers RPC handlers, sends a private get_user_trades_by_currency
 * RPC and polls until a response is received.
 *
 * @return int Exit code 0 on success.
 */
int main() {
	deribit::init_logging();
	SET_LOG_LEVEL(deribit::LogLevel::DEBUG);

	deribit::DeribitClient client;

	LOG_INFO("Connecting...");
	client.connect();

	/**
	 * @brief Wait for authentication token.
	 *
	 * Polls the client until an access token is available.
	 */
	while (client.get_access_token().empty()) {
		client.poll();
	}

	LOG_INFO("Authenticated. Token = {}", client.get_access_token());

	constexpr uint64_t RPC_ID = 9367;

	/**
	 * @brief Register RPC callbacks for the request id used in the test.
	 *
	 * The callbacks will set the shared atomic when a response or error
	 * is received so the polling loop can exit.
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
	 * @brief Poll until the RPC response or error arrives.
	 *
	 * The loop exits once the atomic got_private_response becomes true.
	 */
	while (!got_private_response.load()) {
		client.poll();
	}

	LOG_INFO("Private RPC test completed");
	client.close();
	return 0;
}
