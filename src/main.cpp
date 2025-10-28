#include "glaze/json.hpp"

#include "connection.h"
#include "server.h"

#include "gdal.h"
// import client;
// import httpMessage;
// import glaze;
// import "glaze/json.hpp";
// import asio;

import binaryMessage;

import std;

import http;
import error;

// struct Chat{
// 	std::awaitable<void> add();
// };

// import <asio.hpp>


struct TestServer{
	using RetType = asio::awaitable<Http::Response>;
	Http::Router<RetType(const Http::Request&, std::span<std::byte> resBuffer)> router;

	std::atomic<int> i{1};


	void addCors(Http::Response& res){
		res.set("Access-Control-Allow-Origin", "*");
	}

	TestServer(){
		router.add("/abc/one/:z/:x/:y", [](auto req, auto resBuffer) -> RetType{
			Http::Response res{Http::Status::OK, req.version(), resBuffer};
			res.setBody("Hello twooo2234567!");
			res.set(Http::Field::Connection, "keep-alive");

			co_return res;
		});
		router.add("/*/three/*/a/b", [](auto req, auto resBuffer) -> RetType{
			Http::Response res{Http::Status::OK, req.version(), resBuffer};
			res.setBody("Hello three!");
			res.set(Http::Field::Connection, "keep-alive");

			co_return res;
		});
		router.notFound([](auto req, auto resBuffer) -> RetType{
			Http::Response res{Http::Status::NotFound, req.version(), resBuffer};
			res.set(Http::Field::Connection, "keep-alive");
			// res.setBody("not found");

			co_return res;
		});
	}


	asio::awaitable<void> connect(Connection conn){
		Error err;
		std::println("{} connections", i.fetch_add(1));
		std::array<std::byte, 4096> headBuff;

		for(;;){
			Http::Request req{headBuff};
			err = co_await conn.read(req);
			if(err) break;

			// std::println("method: {}, path: {}, params: {}", req.method(), req.path(), req.params());

			auto res = co_await router.handle(req.path(), req, headBuff);
			if(!res) break;

			err = co_await conn.write(*res);
			if(err) break;
		}

		if(err) std::println("error: {}", err.what());
		co_return;
	}

};

int main(int argc, char* argv[]){
	std::println("starting server...");
	auto obj = glz::obj{"pi", 3.14, "happy", true, "name", "Stephen", "arr", glz::arr{"Hello", "World", 2}};
	std::string s{};
	glz::write_json(obj, s);
	std::println("{}", s);


	TestServer t;

	auto numThread = std::thread::hardware_concurrency();
	Server server{"127.0.0.1", 8000, numThread};

	server.run(t);
	// server.run(handleConnection);
	// server.run([](Connection conn) -> asio::awaitable<void> {
	// 	co_return;
	// });
}
