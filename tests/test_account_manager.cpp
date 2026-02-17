#include "deribit/client/account_manager.h"
#include "deribit/client/deribit_client.h"
#include "deribit/infra/logging.h"

int main() {
	// Initialize the internal logging framework
	deribit::init_logging();

	/**
	 * @brief Client instance for Deribit communication.
	 * * Handles the underlying WebSocketBeast connection and RPC dispatching.
	 */
	deribit::DeribitClient client;
	client.connect_sync();

	const deribit::CurrencyAccountState ret = client.account_manager.get_currency_state("BTC");

	std::cout << "Currency: " << ret.margin.balance << std::endl;

	// Sleep for a moment to allow the RPC response to be processed
	std::this_thread::sleep_for(std::chrono::seconds(30));
}
