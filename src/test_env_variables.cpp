#include <iostream>
#include <atomic>

#include "deribit/logging.h"
#include "deribit/deribit_client.h"

std::atomic<bool> got_private_response = false;

/**
 * Private RPC callback.
 * This is called once the RPC response arrives from Deribit.
 */
void on_private_response(const deribit::ParsedMessage& pm, void* userdata)
{
	got_private_response = true;

	LOG_INFO("Received PRIVATE RPC response");
	LOG_INFO("Result = {}", pm.result);
}

/**
 * Error callback for the private RPC.
 */
void on_private_error(const deribit::ParsedMessage& pm, void* userdata)
{
	got_private_response = true;
	LOG_ERROR("Private RPC ERROR {} {}", pm.error_code, pm.error_msg);
}

int main() {
	deribit::init_logging();
	SET_LOG_LEVEL(deribit::LogLevel::DEBUG);

	deribit::DeribitClient client;

	LOG_INFO("Loading credentials...");
	client.load_credentials_from_env();

	LOG_INFO("Connecting...");
	client.connect();

	// Wait until token is received
	while (client.get_access_token().empty()) {
		client.poll();
	}

	LOG_INFO("Authenticated. Token = {}", client.get_access_token());

	constexpr uint64_t RPC_ID = 9367;

	// Register RPC handler for this RPC ID
	client.get_dispatcher().register_rpc(
		RPC_ID,
		on_private_response,
		on_private_error,
		nullptr
	);

	LOG_INFO("Sending private RPC request: get_user_trades_by_currency");

	std::string params = R"({"count":2,"currency":"ETH"})";
	client.send_rpc(RPC_ID, "private/get_user_trades_by_currency", params);

	// Poll until we receive the RPC response
	while (!got_private_response.load()) {
		client.poll();
	}

	LOG_INFO("Private RPC test completed");
	client.close();
	return 0;
}
