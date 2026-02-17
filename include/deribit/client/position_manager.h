#ifndef HFTDERIBIT_POSITION_MANAGER_H
#define HFTDERIBIT_POSITION_MANAGER_H
#include "deribit_client.h"
#include "deribit/models/order_details.h"


namespace deribit {

	class PositionManager {
	private:
		DeribitClient* client_;

	public:
		explicit PositionManager(DeribitClient& client);

		OrderDetails market_buy(
			const std::string& instrument,
			double usd_amount,
			const std::string& margin_currency,
			Direction side = Direction::Buy
		);

	private:
		OrderDetails parse_order_from_result(
			const std::string& result_json
		);
	};

}

#endif //HFTDERIBIT_POSITION_MANAGER_H