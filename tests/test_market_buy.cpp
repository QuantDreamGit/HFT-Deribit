#include <iostream>

#include "deribit/client/deribit_client.h"
#include "deribit/client/position_manager.h"


int main() {
	deribit::init_logging();
	deribit::DeribitClient client;

	// 1. Connect (auth blocks internally)
	client.connect_sync();

	// 2. Create position manager
	deribit::PositionManager pm(client);

	// 3. Check current balance first
	auto state =
		client.account_manager.get_currency_state("BTC");

	std::cout << "Available funds: "
			  << state.reserves.available_funds
			  << std::endl;

	// 4. Place small market buy
	double usd_amount = 10000000000.0;  // small notional

	deribit::OrderDetails buy_order = pm.market_buy(
			"BTC-PERPETUAL",
			usd_amount,
			"BTC",
			deribit::Direction::Buy
			);
	std::cout << "Buy Order placed\n";
	std::cout << "Order ID: " << buy_order.order_id << std::endl;
	std::cout << "Avg price: " << buy_order.average_price << std::endl;
	std::cout << "Filled: " << buy_order.filled_amount << std::endl;

	deribit::OrderDetails sell_order = pm.market_buy(
			"BTC-PERPETUAL",
			usd_amount,
			"BTC",
			deribit::Direction::Sell
			);
	std::cout << "Sell Order placed\n";
	std::cout << "Order ID: " << sell_order.order_id << std::endl;
	std::cout << "Avg price: " << sell_order.average_price << std::endl;
	std::cout << "Filled: " << sell_order.filled_amount << std::endl;


	// 5. Close client cleanly
	client.close();


	return 0;
}
