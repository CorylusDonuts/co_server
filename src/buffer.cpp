export module buffer;

import std;

export
template<std::size_t bufSize>
class StaticBuffer final {
	std::array<std::byte, bufSize> buffer_ = {};
	std::size_t read_ = 0;
	std::size_t write_ = 0;

public:
	std::size_t size() const noexcept {
		return write_ - read_;
	}

	bool empty() const noexcept {
		return size() == 0;
	}

	std::span<std::byte> writableSpan() noexcept {
		return {buffer_.data() + write_, buffer_.size() - write_};
	}
	void commit(std::size_t size) noexcept {
		write_ += size;
		if (write_ > buffer_.size()) write_ = buffer_.size(); // safety clamp
	}

	std::span<const std::byte> readableSpan() const noexcept {
		return {buffer_.data() + read_, write_ - read_};
	}
	void consume(std::size_t size) noexcept {
		read_ += size;
		if(read_ >= write_){ //no more data, reset the buffer
			read_ = 0;
			write_ = 0;
		}
	}
};
