#ifndef HFT_DERIBIT_WEBSOCKET_BEAST_H
#define HFT_DERIBIT_WEBSOCKET_BEAST_H

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio.hpp>
#include <string>

#include "logging.h"

namespace deribit {
    namespace beast      = boost::beast;
    namespace websocket  = beast::websocket;
    namespace ssl        = boost::asio::ssl;
    namespace net        = boost::asio;

    /**
     * Hostname and connection parameters for the Deribit test network.
     *
     * These constants are used by the WebSocket helper to form the
     * TCP/TLS connection and perform the WebSocket handshake.
     */
    static constexpr const char* DERIBIT_HOST = "test.deribit.com";
    static constexpr const char* DERIBIT_PORT = "443";
    static constexpr const char* DERIBIT_PATH = "/ws/api/v2";

    /**
     * Simple blocking WebSocket wrapper using Boost.Beast and OpenSSL.
     *
     * This helper manages an io_context, SSL context and a websocket
     * stream. It provides synchronous connect, send, read and close
     * helpers that the rest of the codebase can call from background
     * threads. Error handling is performed by logging and by throwing
     * exceptions when low-level failures occur during connect.
     */
    class WebSocketBeast {
        net::io_context ioc_;
        ssl::context ctx_;
        net::ip::tcp::resolver resolver_;
        websocket::stream<ssl::stream<net::ip::tcp::socket>> ws_;
        std::atomic<bool> shutting_down_{false};

    public:
        /**
         * Construct the helper and configure basic TLS parameters.
         *
         * The SSL context is initialized for a TLS client and default
         * verification paths are used. For testnet we disable certificate
         * verification to simplify development; adjust this for production.
         */
        WebSocketBeast()
            : ctx_(ssl::context::tlsv12_client),
              resolver_(net::make_strand(ioc_)),
              ws_(net::make_strand(ioc_), ctx_)
        {
            ctx_.set_default_verify_paths();
            ctx_.set_verify_mode(ssl::verify_none); // OK for testnet
        }

        /**
         * Establish a TLS connection and perform the WebSocket handshake.
         *
         * This method resolves the host, connects the underlying TCP
         * socket, performs a TLS handshake, and then completes the
         * WebSocket opening handshake. It logs progress and throws on
         * failures that occur while setting SNI or during the handshakes.
         */
        void connect() {
            LOG_INFO("Starting Deribit WebSocket connection...");

            LOG_DEBUG("Resolving {}:{}", DERIBIT_HOST, DERIBIT_PORT);
            auto const results = resolver_.resolve(DERIBIT_HOST, DERIBIT_PORT);

            LOG_DEBUG("TCP connecting...");
            auto ep = net::connect(ws_.next_layer().next_layer(), results);

            std::string host_port = std::string(DERIBIT_HOST) + ":" + std::to_string(ep.port());

            LOG_DEBUG("Setting SNI to {}", DERIBIT_HOST);
            if (!SSL_set_tlsext_host_name(ws_.next_layer().native_handle(), DERIBIT_HOST)) {
                beast::error_code ec(static_cast<int>(::ERR_get_error()), net::error::get_ssl_category());
                throw beast::system_error{ec, "Failed to set SNI"};
            }

            LOG_DEBUG("Starting TLS handshake...");
            ws_.next_layer().handshake(ssl::stream_base::client);

            LOG_DEBUG("Performing WebSocket handshake...");
            ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));
            ws_.handshake(host_port, DERIBIT_PATH);

            LOG_INFO("Deribit WebSocket connected (Testnet).");
        }

        /**
         * Send a text message synchronously over the WebSocket.
         *
         * The function logs errors encountered during the write operation
         * and returns without rethrowing to allow caller code to handle
         * transient failures gracefully.
         *
         * @param msg The JSON text message to send.
         */
        void send(const std::string& msg) {
            try {
                ws_.write(net::buffer(msg));
                LOG_DEBUG("WS Send: {}", msg);
            } catch (const std::exception& e) {
                LOG_ERROR("WS Send error: {}", e.what());
            }
        }

        /**
         * Read a text message synchronously from the WebSocket.
         *
         * Returns the received message as a std::string. On error an
         * empty string is returned and the error is logged. This helper
         * uses a Beast flat_buffer to collect the incoming payload.
         *
         * @return The received text message, or an empty string on error.
         */
        std::string read() {
            // Check if the WebSocket is shutting down before attempting to read
            if (shutting_down_.load(std::memory_order_acquire)) {
                LOG_WARN("WebSocket is shutting down, aborting read.");
                return "";  // Return empty string if shutting down
            }

            try {
                beast::flat_buffer buffer;
                ws_.read(buffer);  // Read from the WebSocket
                std::string msg = beast::buffers_to_string(buffer.cdata());  // Convert the buffer to string
                LOG_DEBUG("WS Recv: {}", msg);  // Log the received message
                return msg;
            } catch (const std::exception& e) {
                if (shutting_down_.load(std::memory_order_acquire)) {
                    // If the WebSocket is shutting down, log the termination but don't propagate the error
                    LOG_DEBUG("WS read terminated during shutdown: {}", e.what());
                } else {
                    // Log errors that occurred while reading
                    LOG_ERROR("WS Read error: {}", e.what());
                }
                return "";  // Return empty string in case of error
            }
        }


        /**
         * Mark the WebSocket as shutting down.
         *
         * This sets an atomic flag that can be checked by other
         * components to determine whether shutdown is in progress.
         */
        void mark_shutting_down() {
            shutting_down_.store(true, std::memory_order_release);
        }

        /**
         * Close the WebSocket connection politely.
         *
         * This attempts a normal close handshake and logs any errors that
         * occur during the operation.
         */
        void close() {
            shutting_down_.store(true, std::memory_order_release);

            try {
                ws_.close(websocket::close_code::normal);
                LOG_INFO("WebSocket closed.");
            }
            catch (const std::exception& e) {
                // Expected during shutdown
                LOG_DEBUG("WebSocket close during shutdown: {}", e.what());
            }
        }
    };
}

#endif // HFT_DERIBIT_WEBSOCKET_BEAST_H
