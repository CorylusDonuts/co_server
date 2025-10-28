// module;

// import std;
// #include "msgpack23.h"

export module binaryMessage;

import msgpack23;
import std;
import error;

export
template <typename T>
concept Packable = requires(const T cobj, T obj, msgpack23::Packer<std::back_insert_iterator<std::vector<std::byte>>> packer, msgpack23::Unpacker unpacker) {
	{ cobj.pack(packer) } -> std::same_as<void>;
	{ obj.unpack(unpacker) } -> std::same_as<void>;
};

export
template <Packable T>
class BinaryMessage{
	std::uint64_t length_;
	std::size_t headIdx_ = 0;

	std::vector<std::byte> buffer_;
	std::size_t bodyIdx_ = 0;

	T value_;
public:
	BinaryMessage() = default;
	explicit BinaryMessage(const T& val) : value_(val) {}
	explicit BinaryMessage(T&& val) noexcept(std::is_nothrow_move_constructible_v<T>)
	: value_(std::move(val)) {}

	const T& operator*() const noexcept { return value_; }
	T& operator*() noexcept { return value_; }
	const T* operator->() const noexcept { return &value_; }
	T* operator->() noexcept { return &value_; }
	const T& value() const noexcept { return value_; }
	T& value() noexcept { return value_; }

	Error pack(){
		length_ = 0;
		headIdx_ = 0;
		buffer_.clear();
		bodyIdx_ = 0;

		msgpack23::Packer packer{std::back_insert_iterator(buffer_)};
		try{
			packer(value_);
			value_ = {};
		} catch (...) {
			return {ErrorCode::SERIALIZATION_ERROR};
		}
		length_ = buffer_.size();
		return {};
	}
	Error unpack(){
		msgpack23::Unpacker unpacker{buffer_};
		try{
			value_ = {};
			unpacker(value_);
			buffer_.clear();
		} catch (...) {
			return {ErrorCode::DESERIALIZATION_ERROR};
		}
		buffer_.clear();
		return {};
	}


	std::tuple<Error, bool, std::size_t> consumeHeaderSome(std::span<const std::byte> data) {
		std::size_t remaining = sizeof(length_) - headIdx_;
		std::size_t numCopy = std::min(data.size(), remaining);

		auto* headBytes = reinterpret_cast<std::byte*>(&length_);
		std::memcpy(headBytes + headIdx_, data.data(), numCopy);
		if constexpr (std::endian::native != std::endian::big) length_ = temp::byteswap(length_);
		headIdx_ += numCopy;

		bool finished = headIdx_ >= sizeof(length_);
		return {{}, finished, numCopy};
	}
	std::tuple<Error, bool, std::size_t> consumeBodySome(std::span<const std::byte> data) {
		if(buffer_.size() != length_) buffer_.resize(length_);
		std::size_t remaining = length_ - bodyIdx_;
		std::size_t numCopy = std::min(data.size(), remaining);

		std::memcpy(buffer_.data() + bodyIdx_, data.data(), numCopy);
		bodyIdx_ += numCopy;

		bool finished = bodyIdx_ >= length_;
		return {{}, finished, numCopy};
	}

	std::tuple<Error, bool, std::size_t> produceHeaderSome(std::span<std::byte> out){
		std::size_t remaining = sizeof(length_) - headIdx_;
		std::size_t numCopy = std::min(out.size(), remaining);

		std::uint64_t lengthVal = length_;
		if constexpr (std::endian::native != std::endian::big) lengthVal = temp::byteswap(length_);
		auto* headBytes = reinterpret_cast<std::byte*>(&lengthVal);
		std::memcpy(out.data(), headBytes + headIdx_, numCopy);
		headIdx_ += numCopy;

		bool finished = headIdx_ >= sizeof(length_);
		if (finished) headIdx_ = 0;
		return {{}, finished, numCopy};
	}
	std::tuple<Error, bool, std::size_t> produceBodySome(std::span<std::byte> out){
		if(buffer_.size() != length_) buffer_.resize(length_);
		std::size_t remaining = length_ - bodyIdx_;
		std::size_t numCopy = std::min(out.size(), remaining);

		std::memcpy(out.data(), buffer_.data() + bodyIdx_, numCopy);
		bodyIdx_ += numCopy;

		bool finished = bodyIdx_ >= length_;
		if (finished) bodyIdx_ = 0;
		return {{}, finished, numCopy};
	}
};
