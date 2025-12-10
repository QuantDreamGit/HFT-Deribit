#ifndef HFT_DERIBIT_WEBSOCKET_BEAST_H
#define HFT_DERIBIT_WEBSOCKET_BEAST_H

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio.hpp>

#include "logging.h"

namespace deribit {
	namespace beast		= boost::beast;
	namespace websocket = beast::websocket;
	namespace ssl		= boost::asio::ssl;
	namespace net		= boost::asio;

	// Deribit WS endpoint
	static constexpr const char* DERIBIT_HOST = "www.deribit.com";
	static constexpr const char* DERIBIT_PORT = "443";
	static constexpr const char* DERIBIT_PATH = "/ws/api/v2";

	class WebSocketBeast {
		net::io_context ioc_;
		ssl::context ctx_;
		net::ip::tcp::resolver resolver_;
		websocket::stream<ssl::stream<net::ip::tcp::socket>> ws_;

	public:
		WebSocketBeast() :
			ctx_(ssl::context::tlsv12_client),			// Use TLS v1.2
			resolver_(net::make_strand(ioc_)),		// Resolver uses the same strand as the I/O context
			ws_(net::make_strand(ioc_), ctx_) {		// WebSocket stream uses the same strand as the I/O context

			// TODO: Configure SSL context properly for production use!
			ctx_.set_default_verify_paths();				// Load system default CA certificates
			ctx_.set_verify_mode(ssl::verify_none);		// Disable certificate verification
		};

		void connect() {
			LOG_INFO("Starting Deribit WebSocket connection...");

			LOG_DEBUG("Resolving {}:{}" + std::string(DERIBIT_HOST) + std::string( DERIBIT_PORT));
			auto const results = resolver_.resolve(DERIBIT_HOST, DERIBIT_PORT);

			LOG_DEBUG("TCP connecting...");
			auto ep = net::connect(ws_.next_layer().next_layer(), results);

			const std::string host_port = std::string(DERIBIT_HOST) + ":" + std::to_string(ep.port());

			LOG_DEBUG("Setting SNI...");
			if (!SSL_set_tlsext_host_name(ws_.next_layer().native_handle(), DERIBIT_HOST)) {
				beast::error_code ec(static_cast<int>(::ERR_get_error()), net::error::get_ssl_category());
				throw beast::system_error{ec, "Failed to set SNI hostname"};
			}

			LOG_DEBUG("Starting TLS handshake...");
			ws_.next_layer().handshake(ssl::stream_base::client);

			LOG_DEBUG("TLS handshake complete, starting WebSocket handshake...");
			ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));

			ws_.set_option(websocket::stream_base::decorator(
				[](websocket::request_type& req)
				{
					req.set(beast::http::field::user_agent, "Deribit-HFT-Client");
				}
			));

			ws_.handshake(host_port, DERIBIT_PATH);
			LOG_INFO("Deribit WebSocket connected.");
		}


		void send(const std::string& msg)
		{
			try {
				ws_.write(net::buffer(msg));
				LOG_DEBUG("WS Send: {}" + msg);
			}
			catch (const std::exception& e) {
				LOG_ERROR("WS Send error: {}" + std::string(e.what()));
			}
		}

		std::string read()
		{
			try {
				beast::flat_buffer buffer;
				ws_.read(buffer);
				std::string msg = beast::buffers_to_string(buffer.cdata());
				LOG_DEBUG("WS Recv: {}" + msg);
				return msg;
			}
			catch (const std::exception& e) {
				LOG_ERROR("WS Read error: {}" + std::string(e.what()));
				return "";
			}
		}

		void close()
		{
			try {
				ws_.close(websocket::close_code::normal);
				LOG_INFO("WebSocket closed.");
			}
			catch (const std::exception& e) {
				LOG_ERROR("Close error: {}" + std::string(e.what()));
			}
		}

	};
} // namespace deribit
#endif //HFT_DERIBIT_WEBSOCKET_BEAST_H
