#pragma once

#include "connection.h"
#include <asio/io_context.hpp>
#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>

import std;
import http;

template<typename T>
concept ConnectionHandler =
requires(T t, Connection c) {
	{ t(std::move(c)) } -> std::same_as<asio::awaitable<void>>; // callable object
} || requires(T t, Connection c) {
	{ t.connect(std::move(c)) } -> std::same_as<asio::awaitable<void>>; // member function
};

class Server{
	asio::ip::tcp::endpoint endpoint;
	std::vector<std::jthread> threads;
	std::vector<std::unique_ptr<asio::io_context>> contexts;
	std::vector<asio::executor_work_guard<asio::io_context::executor_type>> workGuards;
	std::vector<asio::ip::tcp::acceptor> acceptors;

	template<typename ConnectionHandler>
	asio::awaitable<void> listen(int i, ConnectionHandler&& handler){
		auto& executor = *contexts[i];
		// auto executor = co_await asio::this_coro::executor;
		auto& acceptor = acceptors[i];

		for(;;)
		{
			Connection conn{ co_await acceptor.async_accept(executor) };
			if constexpr (requires { handler(std::move(conn)); }) {
				asio::co_spawn(
					executor,
					handler(std::move(conn)),
					[](std::exception_ptr e) {
						if(!e) return;
						try
						{
							std::rethrow_exception(e);
						}
						catch(std::exception const& e)
						{
							std::cerr << "Error in session: " << e.what() << "\n";
						}
					}
				);
			} else {
				asio::co_spawn(
					executor,
					handler.connect(std::move(conn)),
					[](std::exception_ptr e) {
						if(!e) return;
						try
						{
							std::rethrow_exception(e);
						}
						catch(std::exception const& e)
						{
							std::cerr << "Error in session: " << e.what() << "\n";
						}
					}
				);
			}
		}
	}
public:
	Server(std::string address, std::uint16_t port, std::size_t numThreads):
		endpoint(asio::ip::tcp::endpoint{asio::ip::make_address(address), port})
	{
		// create io_contexts
		for (auto i = 0u; i < numThreads; ++i) {
			contexts.emplace_back(std::make_unique<asio::io_context>(1));
			workGuards.emplace_back(asio::make_work_guard(*contexts[i]));
			auto& acc = acceptors.emplace_back(*contexts[i]);

			acc.open(endpoint.protocol());
			// acc.set_option(asio::socket_base::reuse_address(true));
			int yes = 1;
			::setsockopt(acc.native_handle(), SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
			::setsockopt(acc.native_handle(), SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(yes));
			acc.bind(endpoint);
			acc.listen();
		}
	}

	template<typename ConnectionHandler>
	void run(ConnectionHandler&& handler){
		for(int i = 0; i < contexts.size(); ++i){
			asio::co_spawn(
				*contexts[i],
				listen(i, std::forward<ConnectionHandler>(handler)),
				[](std::exception_ptr e)
				{
					std::println("error");
					if(e)
					{
						try
						{
							std::rethrow_exception(e);
						}
						catch(std::exception const& e)
						{
							std::cerr << "Error in session: " << e.what() << "\n";
						}
					}
				}
			);
		}

		// create worker threads
		for (auto i = 0u; i < contexts.size(); ++i) {
			threads.emplace_back([this, i]{
				contexts[i]->run();
			});
			// set_affinity(threads[i], i);
		}

		for (std::size_t i = 0; i < threads.size(); ++i)
			threads[i].join();

	}
};
