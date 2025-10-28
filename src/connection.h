#pragma once
// module;
// #include "asio.hpp"

#include "asio/awaitable.hpp"
#include "asio/use_awaitable.hpp"
#include "asio/ip/tcp.hpp"
#include "asio/as_tuple.hpp"

// import <asio.hpp>;

// export module client;

// import asio;
import error;
import buffer;
import std;

// export
template <typename T>
concept MessageLike = requires(
	T t,
	std::span<const std::byte> in,
	std::span<std::byte> out
) {
	{ t.consumeHeaderSome(in) } -> std::same_as<std::tuple<Error, bool, std::size_t>>;
	{ t.consumeBodySome(in) }   -> std::same_as<std::tuple<Error, bool, std::size_t>>;
	{ t.produceHeaderSome(out) } -> std::same_as<std::tuple<Error, bool, std::size_t>>;
	{ t.produceBodySome(out) }   -> std::same_as<std::tuple<Error, bool, std::size_t>>;
};

// export
class Connection{
	enum class ReadState { START, READ_HEADER, READ_BODY };
	enum class WriteState { START, WRITE_HEADER, WRITE_BODY };

	asio::ip::tcp::socket socket_;
	StaticBuffer<4096> readBuffer_;
	StaticBuffer<4096> writeBuffer_;
	ReadState readState_ = ReadState::START;
	WriteState writeState_ = WriteState::START;
public:
	explicit Connection(asio::ip::tcp::socket&& socket): socket_(std::move(socket)) {}

	Connection(const Connection&) = delete;
	Connection& operator=(const Connection&) = delete;
	Connection(Connection&&) noexcept = default;
	Connection& operator=(Connection&&) noexcept = default;

	template <MessageLike M>
	asio::awaitable<Error> read(M& msg){
		if(readState_ != ReadState::START) co_return Error{ErrorCode::INVALID_STATE};
		readState_ = ReadState::READ_HEADER;

		bool doneRead = false;
		while(!doneRead){
			if(readBuffer_.empty()) {
				// std::println("readable size: {}", readBuffer_.writableSpan().size());
				auto [ec, n] = co_await socket_.async_read_some(asio::buffer(readBuffer_.writableSpan()), asio::as_tuple(asio::use_awaitable));
				if(ec || n == 0) {
					if(n == 0) co_return Error{ErrorCode::CONNECTION_ENDED};
					std::println("socket read error: {}", ec.message());
					co_return Error{ErrorCode::SOCKET_READ_ERROR};
				}
				readBuffer_.commit(n);

				// std::string_view r{reinterpret_cast<const char*>(readBuffer_.readableSpan().data()), readBuffer_.readableSpan().size()};
				// std::println("read:\n{}", r);
			}

			while(!readBuffer_.empty()){
				if(readState_ == ReadState::READ_HEADER){
					auto [err, complete, numBytes] = msg.consumeHeaderSome(readBuffer_.readableSpan());
					if(err) co_return err;
					readBuffer_.consume(numBytes);
					if(complete) {
						readState_ = ReadState::READ_BODY;
					}
				}
				if(readState_ == ReadState::READ_BODY){
					// std::println("read body");
					auto [err, complete, numBytes] = msg.consumeBodySome(readBuffer_.readableSpan());
					if(err) co_return err;
					readBuffer_.consume(numBytes);
					if(complete) {
						readState_ = ReadState::START;
						doneRead = true;
						break;
					}
				}
			}
		}
		co_return Error{};
	}

	template <MessageLike M>
	asio::awaitable<Error> write(M& msg){
		if(writeState_ != WriteState::START) co_return Error{ErrorCode::INVALID_STATE};
		writeState_ = WriteState::WRITE_HEADER;

		bool doneWrite = false;
		while (!doneWrite) {
			// Fill write buffer until either header/body complete or buffer full
			while (writeBuffer_.writableSpan().size() > 0) {
				if (writeState_ == WriteState::WRITE_HEADER){
					auto [err, complete, numBytes] = msg.produceHeaderSome(writeBuffer_.writableSpan());
					if(err) co_return err;
					writeBuffer_.commit(numBytes);
					if(complete){
						writeState_ = WriteState::WRITE_BODY;
					}
				}
				if(writeState_ == WriteState::WRITE_BODY){
					auto [err, complete, numBytes] = msg.produceBodySome(writeBuffer_.writableSpan());
					if(err) co_return err;
					writeBuffer_.commit(numBytes);
					if(complete){
						writeState_ = WriteState::START;
						doneWrite = true;
						break;
					}
				}
			}

			while(!writeBuffer_.empty()){
				// std::string_view w{reinterpret_cast<const char*>(writeBuffer_.readableSpan().data()), writeBuffer_.readableSpan().size()};
				// std::println("write:\n{}", w);
				// std::println("writeRaw: ");
				// for(int i = 0; i < writeBuffer_.readableSpan().size(); ++i) std::print("{} ", std::to_integer<int>(writeBuffer_.readableSpan()[i]));
				// std::print("\n");

				// std::println("writeBuf readableSpan: {}", writeBuffer_.readableSpan().size());
				auto [ec, n] = co_await socket_.async_write_some(asio::buffer(writeBuffer_.readableSpan()), asio::as_tuple(asio::use_awaitable));
				if(ec || n == 0) {
					if(n == 0) co_return Error{ErrorCode::CONNECTION_ENDED};
					std::println("socket write error: {}", ec.message());
					co_return Error{ErrorCode::SOCKET_WRITE_ERROR};
				}
				// std::println("writable: {}, written: {}", writeBuffer_.readableSpan().size(), n);
				writeBuffer_.consume(n);
			}
		}
		co_return Error{};
	}
};
