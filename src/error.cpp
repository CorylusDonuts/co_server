export module error;

import std;

export
enum class ErrorCode{
	SERIALIZATION_ERROR,
	DESERIALIZATION_ERROR,
	NULL_VALUE,
	INVALID_STATE,
	INVALID_MESSAGE,
	SOCKET_READ_ERROR,
	SOCKET_WRITE_ERROR,
	CONNECTION_ENDED,

	NO_ERROR,
	_count
};
static constexpr std::array<std::string_view, static_cast<std::size_t>(ErrorCode::_count)> _ErrorCodeStr{
	"Serialization Error",
	"Deserialization Error",
	"Null Value",
	"Invalid State",
	"Invalid Message",
	"Socket Read Error",
	"Socket Write Error",
	"Connection Ended",

	"No Error"
};

export
class Error{
	std::optional<ErrorCode> ec_ = std::nullopt;
public:
	Error(ErrorCode ec): ec_(ec) {}
	Error() = default;
	std::string_view what() {
		std::size_t idx = ec_ ? static_cast<std::size_t>(*ec_) : static_cast<std::size_t>(ErrorCode::NO_ERROR);
		return _ErrorCodeStr[idx];
	}
	explicit operator bool() const noexcept { return ec_.has_value(); }
};
