module;
#include "picohttpparser/picohttpparser.h"

export module http;
import std;
import error;

export
namespace Http {
	enum class Status {
		SwitchingProtocols,

		OK,
		Created,
		NoContent,

		BadRequest,
		Unauthorized,
		Forbidden,
		NotFound,
		MethodNotAllowed,
		Conflict,

		InternalServerError,
		NotImplemented,
		BadGateway,
		ServiceUnavailable,
		GatewayTimeout,

		_Count
	};


	enum class Field{
		Host,
		ContentLength,
		ContentType,
		Connection,
		Accept,
		UserAgent,
		TransferEncoding,
		Authorization,
		Cookie,

		_Count
	};

	class Request{
		std::vector<std::byte> headerBufferOptionalData_;
		std::span<std::byte> headerBuffer_;
		std::size_t maxHeaderSize_;
		std::size_t headerBufferIdx_ = 0;
		char last_{0};
		unsigned char numFound_{0};

		bool deserialize_();

		std::vector<std::byte> body_;
		std::size_t maxBodySize_ = 1024 * 1024;
		std::size_t bodyIdx_ = 0;
		phr_chunked_decoder chunkedDecoder_;

		std::string_view method_;
		std::string_view target_;
		std::string version_;
		std::vector<std::string_view> path_;
		std::vector<std::pair<std::string_view, std::string_view>> params_;

		std::optional<std::size_t> contentLength_ = std::nullopt;
		bool chunked_ = false;

		static constexpr std::size_t maxNumHeaders_ = 32;
		std::array<std::pair<std::string_view, std::string_view>, maxNumHeaders_> fields_;
		std::size_t numHeaders_ = 0;
	public:
		Request(std::span<std::byte> headerBuffer) noexcept;
		Request(std::size_t maxHeaderSize = 4096);

		std::string_view method() const noexcept { return method_; }
		std::string_view target() const noexcept { return target_; }
		std::string_view version() const noexcept { return version_; }

		std::span<const std::string_view> path() const noexcept { return path_; }
		std::span<const std::pair<std::string_view, std::string_view>> params() const noexcept { return params_; }

		std::optional<std::string_view> get(Http::Field field) const noexcept;
		std::optional<std::string_view> get(std::string_view field) const noexcept;
		std::vector<std::string_view> getList(Http::Field field) const noexcept;
		std::vector<std::string_view> getList(std::string_view field) const noexcept;

		std::span<const std::byte> body() const noexcept { return body_; }

		std::tuple<Error, bool, std::size_t> consumeHeaderSome(std::span<const std::byte> data);
		std::tuple<Error, bool, std::size_t> consumeBodySome(std::span<const std::byte> data);
		std::tuple<Error, bool, std::size_t> produceHeaderSome(std::span<std::byte> out);
		std::tuple<Error, bool, std::size_t> produceBodySome(std::span<std::byte> out);
	};

	class Response{
		std::vector<std::byte> headerBufferOptionalData_;
		std::span<std::byte> headerBuffer_;
		std::size_t headerBufferIdx_ = 0;
		std::size_t headerSize_ = 0;

		bool defaultKeepAlive_ = true;
		std::size_t contentLength_ = 0;
		bool hasContentType_ = false;
		bool hasConnection_ = false;
		//bool hasTransferEncoding

		void init_(std::string_view status, std::string_view reason, std::string_view version);
		void alloc_(std::size_t num);
		bool serialize_();
		bool serialized_ = false;

		std::string stringBody_;
		std::vector<std::byte> body_;
		std::size_t bodyIdx_ = 0;
		bool isBodyString_;
	public:
		Response(Http::Status status, std::string_view version, std::span<std::byte> headerBuffer) noexcept;
		Response(std::pair<int, std::string_view> status, std::string_view version, std::span<std::byte> headerBuffer) noexcept;
		Response(Http::Status status, std::string_view version);
		Response(std::pair<int, std::string_view> status, std::string_view version);

		void set(Http::Field field, std::string_view value);
		void set(std::string_view field, std::string_view value);

		void setBody(std::span<const std::byte> data);
		void setBody(std::vector<std::byte>&& data);
		void setBody(const std::string& data);
		void setBody(std::string&& data);

		std::tuple<Error, bool, std::size_t> consumeHeaderSome(std::span<const std::byte> data);
		std::tuple<Error, bool, std::size_t> consumeBodySome(std::span<const std::byte> data);
		std::tuple<Error, bool, std::size_t> produceHeaderSome(std::span<std::byte> out);
		std::tuple<Error, bool, std::size_t> produceBodySome(std::span<std::byte> out);
	};


	template <typename T>
	concept Awaitable = requires(T t) {
		t.operator co_await(); // if user-defined
	} || requires(T t) {
		operator co_await(t);  // if ADL
	};

	// template <typename F, Awaitable AwaitT, typename Ret, typename... Args>
	// concept CoAwaitableFunction =
	// requires(F f, Args&&... args) {
	// 	{ f(std::forward<Args>(args)...) } -> std::same_as<AwaitT<Ret>>;
	// };
	// template<typename T>
	// concept HttpHandler =
	// requires(T t) {
	// 	{ t(std::declval<Args>()...) } -> std::same_as<AwaitT<Ret>>; // callable object
	// };

	template<typename Signature>
	class Router;

	template <template<typename> typename AwaitT, typename Ret, typename... Args>
	class Router<AwaitT<Ret>(Args...)> {
		using Handler = std::function<AwaitT<Ret>(Args...)>;
		struct Node {
			std::unordered_map<std::string_view, std::unique_ptr<Node>> children{};
			std::unique_ptr<Node> paramChild = nullptr;  // : here
			std::unique_ptr<Node> wildcardChild = nullptr; // * here
			std::optional<Handler> handler;
		};
		std::optional<Handler> notFoundHandler;
		Node root;
		std::vector<std::vector<std::string>> segmentsList;

		const Node* find_(const Node* n, std::span<const std::string_view> path) const {
			// std::println("path: {}", path);
			if(path.empty()){
				if(n->handler) return n;
				return nullptr;
			}

			// std::println("segments: {}", segments);
			// for(const auto& [k, v] : n->children) std::println("child: {}", k);

			auto it = n->children.find(path.front());
			if (it != n->children.end()) {
				// std::println("found child: {}", it->first);
				auto ret = find_(it->second.get(), path.subspan(1));
				if(ret) return ret;
			}

			if(n->paramChild){
				auto ret = find_(n->paramChild.get(), path.subspan(1));
				if(ret) return ret;
			}

			if (n->wildcardChild) {
				// for(int i = path.size(); i >= 1; --i){
				for(int i = 1; i <= path.size(); ++i){
					auto ret = find_(n->wildcardChild.get(), path.subspan(i));
					if(ret) return ret;
				}
			}

			return nullptr;
		}
	public:
		template<typename HttpHandler>
		void add(std::string path, HttpHandler&& func) {
			std::vector<std::string> segments;
			std::size_t start = 0;
			while (start < path.size()) {
				std::size_t slash = path.find('/', start);
				if (slash == start) { start++; continue; } // skip leading /
				if (slash == std::string::npos) slash = path.size();
				segments.emplace_back(path.substr(start, slash - start));
				start = slash + 1;
			}
			if(path.back() == '/') segments.emplace_back("");


			Node* n = &root;
			for(auto& seg : segments){
				if(seg == "*"){
					if (!n->wildcardChild) n->wildcardChild = std::make_unique<Node>();
					// std::println("wildChild");
					n = n->wildcardChild.get();
				} else if(seg.size() && seg[0] == ':'){
					if (!n->paramChild) n->paramChild = std::make_unique<Node>();
					// std::println("paramChild");
					n = n->paramChild.get();
				} else{
					auto [it, inserted] = n->children.try_emplace(std::move(seg), std::make_unique<Node>());
					// for(const auto& [k, v] : n->children) std::println("child: {}", k);
					n = it->second.get();

				}
			}
			segmentsList.push_back(std::move(segments));

			// std::println("segments: {}", segments);
			// for(const auto& [k, v] : root.children) std::print("root children added: {}", k);
			// std::println(", have wild: {}, have param {}", bool(root.wildcardChild), bool(root.paramChild));

			n->handler = std::move(func);

		}

		template<typename HttpHandler>
		void notFound(HttpHandler&& func){
			notFoundHandler = std::move(func);
		}

		AwaitT<std::optional<Ret>> handle(std::span<const std::string_view> path, Args... args) const {
			// std::println("finding path...");
			const Node* node = find_(&root, path);
			// for(const auto& [k, v] : root.children) std::println("root children: {}", k);
			if (node) {
				// std::println("found path: {}", path);
				co_return std::optional<Ret>{ co_await (*(node->handler))(std::forward<Args>(args)...) };
			} else if(notFoundHandler){
				co_return std::optional<Ret>{ co_await (*notFoundHandler)(std::forward<Args>(args)...) };
			}
			co_return std::nullopt;
		}
	};

};

// module : private;

static constexpr std::array<std::tuple<int, std::string_view, std::string_view>, static_cast<std::size_t>(Http::Status::_Count)> statusStrArr_{{
	{101, "101", "Switching Protocols"},

	{200, "200", "OK"},
	{201, "201", "Created"},
	{204, "204", "No Content"},

	{400, "400", "Bad Request"},
	{401, "401", "Unauthorized"},
	{403, "403", "Forbidden"},
	{404, "404", "Not Found"},
	{405, "405", "Method Not Allowed"},
	{409, "409", "Conflict"},

	{500, "500", "Internal Server Error"},
	{501, "501", "Not Implemented"},
	{502, "502", "Bad Gateway"},
	{503, "503", "Service Unavailable"},
	{504, "504", "Gateway Timeout"}
}};

static constexpr std::array<std::string_view, static_cast<std::size_t>(Http::Field::_Count)> fieldStrArr_{
	"Host",
	"Content-Length",
	"Content-Type",
	"Connection",
	"Accept",
	"User-Agent",
	"Transfer-Encoding",
	"Authorization",
	"Cookie"
};
static constexpr std::array<std::string_view, static_cast<std::size_t>(Http::Field::_Count)> fieldStrArrLower_{
	"host",
	"content-length",
	"content-type",
	"connection",
	"accept",
	"user-agent",
	"transfer-encoding",
	"authorization",
	"cookie"
};

export
namespace Http{
	/*~~~~~~~~~~~~~~~~~~~~~~~REQUEST~~~~~~~~~~~~~~~~~~~~~~~*/
	Request::Request(std::span<std::byte> headerBuffer) noexcept:
		headerBuffer_(headerBuffer),
		maxHeaderSize_(headerBuffer.size())
	{
		chunkedDecoder_ = {};
		chunkedDecoder_.consume_trailer = 1;
	}
	Request::Request(std::size_t maxHeaderSize):
		maxHeaderSize_(maxHeaderSize)
	{
		chunkedDecoder_ = {};
		chunkedDecoder_.consume_trailer = 1;
		headerBufferOptionalData_.resize(maxHeaderSize_);
		headerBuffer_ = {headerBufferOptionalData_.data(), maxHeaderSize_};
	}

	bool Request::deserialize_(){ //TODO: use error code
		const char* method = nullptr;
		const char* target = nullptr;
		size_t methodLen, targetLen;
		int minorVersion;
		numHeaders_ = maxNumHeaders_;
		phr_header headers[maxNumHeaders_];

		int ret = phr_parse_request(reinterpret_cast<const char*>(headerBuffer_.data()), headerBuffer_.size(),
									&method, &methodLen,
									&target, &targetLen,
									&minorVersion, headers, &numHeaders_, 0);

		if(ret <= 0) return false;

		method_ = std::string_view{method, methodLen};
		target_ = std::string_view{target, targetLen};
		if(minorVersion == 0) version_ = "1.0";
		else if(minorVersion == 1) version_ = "1.1";
		else version_ = "1.1";

		for(int i = 0; i < numHeaders_; ++i){
			std::string_view name {headers[i].name, headers[i].name_len};
			std::string_view value {headers[i].value, headers[i].value_len};
			fields_[i] = {name, value};
			// fields_[name].push_back(value);
		}

		auto cl = get(Http::Field::ContentLength);
		if(cl){
			std::size_t value;
			auto [ptr, ec] = std::from_chars(cl->data(), cl->data() + cl->size(), value);
			if (ec != std::errc{}) return false; //content length field but invalid
			contentLength_ = value;
		}
		if(!contentLength_){
			auto tes = getList(Field::TransferEncoding);
			for(const auto& te : tes){
				if(te.find("chunked") != te.npos){
					chunked_ = true;
					break;
				}
			}
		}

		const auto qPos = target_.find('?');
		std::string_view pathPart = target_.substr(0, qPos);
		std::size_t start = 0;
		while (start < pathPart.size()) {
			std::size_t slash = pathPart.find('/', start);
			if (slash == start) { start++; continue; } // skip leading /
			if (slash == std::string_view::npos) slash = pathPart.size();
			path_.emplace_back(pathPart.substr(start, slash - start));
			start = slash + 1;
		}
		if (pathPart.back() == '/') path_.emplace_back("");

		if (qPos == std::string_view::npos) return true;
		std::string_view query = target_.substr(qPos + 1);
		start = 0;
		while (start < query.size()) {
			std::size_t amp = query.find('&', start);
			if (amp == std::string_view::npos) amp = query.size();

			std::string_view pair = query.substr(start, amp - start);
			std::size_t eq = pair.find('=');
			if (eq != std::string_view::npos)
				params_.emplace_back(pair.substr(0, eq), pair.substr(eq + 1));
			else
				params_.emplace_back(pair, std::string_view{});

			start = amp + 1;
		}

		return true;
	}

	std::optional<std::string_view> Request::get(Field field) const noexcept {
		const auto& fieldStr = fieldStrArr_[static_cast<std::size_t>(field)];
		const auto& fieldStrLower = fieldStrArrLower_[static_cast<std::size_t>(field)];
		for(int i = 0; i < numHeaders_; ++i){
			const auto& [k, v] = fields_[i];
			if(k == fieldStr || k == fieldStrLower) return v;
		}
		return std::nullopt;
	}
	std::optional<std::string_view> Request::get(std::string_view field) const noexcept {
		for(int i = 0; i < numHeaders_; ++i){
			const auto& [k, v] = fields_[i];
			if(k == field) return v;
		}
		return std::nullopt;
	}
	std::vector<std::string_view> Request::getList(Field field) const noexcept {
		const auto& fieldStr = fieldStrArr_[static_cast<std::size_t>(field)];
		const auto& fieldStrLower = fieldStrArrLower_[static_cast<std::size_t>(field)];
		std::vector<std::string_view> ret;
		for(int i = 0; i < numHeaders_; ++i){
			const auto& [k, v] = fields_[i];
			if(k == fieldStr || k == fieldStrLower) ret.push_back(v);
		}
		return ret;
	}
	std::vector<std::string_view> Request::getList(std::string_view field) const noexcept{
		std::vector<std::string_view> ret;
		for(int i = 0; i < numHeaders_; ++i){
			const auto& [k, v] = fields_[i];
			if(k == field) ret.push_back(v);
		}
		return ret;
	}

	std::tuple<Error, bool, std::size_t> Request::consumeHeaderSome(std::span<const std::byte> data){
		std::size_t writeable = maxHeaderSize_ - headerBufferIdx_;
		bool readAll = writeable >= data.size();
		std::size_t maxRead = readAll ? data.size() : writeable;

		std::size_t numRead = maxRead;
		for(std::size_t i = 0; i < maxRead; ++i){
			const char& c = static_cast<char>(data[i]);
			if(c == '\r' && last_ != '\r') ++numFound_;
			else if(c == '\n' && last_ == '\r') ++numFound_;
			else numFound_ = 0;
			last_ = c;

			if(numFound_ == 4) {
				numRead = i + 1;
			}
		}
		bool finished = numFound_ == 4;

		if((numRead == writeable) && !finished) return {ErrorCode::INVALID_MESSAGE, true, 0};

		std::memcpy(headerBuffer_.data() + headerBufferIdx_, data.data(), numRead);
		headerBufferIdx_ += numRead;

		if(finished){
			bool success = deserialize_();
			if(!success) return {ErrorCode::INVALID_MESSAGE, true, 0};
		}
		return {{}, finished, numRead};
	}
	std::tuple<Error, bool, std::size_t> Request::consumeBodySome(std::span<const std::byte> data){
		if(contentLength_ && *contentLength_ > 0){
			if(*contentLength_ > maxBodySize_) return {ErrorCode::INVALID_MESSAGE, true, 0};
			if(body_.size() != *contentLength_) body_.resize(*contentLength_);
			std::size_t writeable = *contentLength_ - bodyIdx_;
			std::size_t numCopy = std::min(data.size(), writeable);

			std::memcpy(body_.data() + bodyIdx_, data.data(), numCopy);
			bodyIdx_ += numCopy;

			bool finished = bodyIdx_ >= *contentLength_;
			return {{}, finished, numCopy};
		}
		else if(chunked_){
			std::size_t writeable = maxBodySize_ - bodyIdx_;
			std::size_t numCopy = std::min(data.size(), writeable);

			if(body_.size() < 1024) body_.resize(1024);
			if(body_.size() < bodyIdx_ + numCopy) body_.resize(std::min(maxBodySize_, body_.size() * 2));
			if(writeable == 0) return {ErrorCode::INVALID_MESSAGE, true, 0};

			std::memcpy(body_.data() + bodyIdx_, data.data(), numCopy);
			int ret = phr_decode_chunked(&chunkedDecoder_, (char*)(body_.data() + bodyIdx_), &numCopy); //modify body_ in place;
			if(ret == -1) return {ErrorCode::INVALID_MESSAGE, true, 0};
			else if(ret == -2) {
				bodyIdx_ += numCopy;
				return {{}, false, numCopy};
			}
			else {
				std::size_t encoded = numCopy - ret;
				body_.resize(bodyIdx_ + encoded);
				return {{}, true, encoded};
			}
		}
		//there's no body here
		return {{}, true, 0};
	}
	std::tuple<Error, bool, std::size_t> Request::produceHeaderSome(std::span<std::byte> out){
		return {ErrorCode::INVALID_MESSAGE, true, 0};
	}
	std::tuple<Error, bool, std::size_t> Request::produceBodySome(std::span<std::byte> out){
		return {ErrorCode::INVALID_MESSAGE, true, 0};
	}


	/*~~~~~~~~~~~~~~~~~~~~~~~RESPONSE~~~~~~~~~~~~~~~~~~~~~~~*/
	void Response::init_(std::string_view status, std::string_view reason, std::string_view version){
		if(version == "1.0") defaultKeepAlive_ = false;

		static constexpr std::string_view first{"HTTP/"};
		static constexpr std::string_view rn{"\r\n"};
		static constexpr std::string_view space{" "};
		auto fieldSize = first.size() + version.size() + 1 + status.size() + 1 + reason.size() + rn.size();

		alloc_(fieldSize);

		std::memcpy(headerBuffer_.data() + headerBufferIdx_, first.data(), first.size()); headerBufferIdx_ += first.size();
		std::memcpy(headerBuffer_.data() + headerBufferIdx_, version.data(), version.size()); headerBufferIdx_ += version.size();
		std::memcpy(headerBuffer_.data() + headerBufferIdx_, space.data(), space.size()); headerBufferIdx_ += space.size();
		std::memcpy(headerBuffer_.data() + headerBufferIdx_, status.data(), status.size()); headerBufferIdx_ += status.size();
		std::memcpy(headerBuffer_.data() + headerBufferIdx_, space.data(), space.size()); headerBufferIdx_ += space.size();
		std::memcpy(headerBuffer_.data() + headerBufferIdx_, reason.data(), reason.size()); headerBufferIdx_ += reason.size();
		std::memcpy(headerBuffer_.data() + headerBufferIdx_, rn.data(), rn.size()); headerBufferIdx_ += rn.size();
	}

	void Response::alloc_(std::size_t num){
		if(headerBuffer_.size() <= headerBufferIdx_ + num){
			const std::size_t newSize = std::max(
				headerBuffer_.size() + (headerBuffer_.size() >> 1) + 1,
				headerBufferIdx_ + num
			);
			std::vector<std::byte> newBuf;
			newBuf.reserve(newSize);
			std::memcpy(newBuf.data(), headerBuffer_.data(), headerBufferIdx_);
			headerBufferOptionalData_.swap(newBuf);
			headerBuffer_ = std::span<std::byte>(headerBufferOptionalData_.data(), headerBufferOptionalData_.capacity());
		}
	}

	bool Response::serialize_(){
		serialized_ = true;
		bool hasBody = (body_.size() + stringBody_.size()) > 0;
		if(!hasContentType_ && hasBody){
			if(isBodyString_) set(Http::Field::ContentType, "text/plain");
			else set(Http::Field::ContentType, "application/octet-stream");
		}

		if(!hasConnection_){
			if(defaultKeepAlive_) set(Http::Field::Connection, "keep-alive");
			else set(Http::Field::Connection, "close");
		}

		if(/*hasBody && */contentLength_ == 0){
			char buf[32];
			if(isBodyString_) contentLength_ = stringBody_.size();
			else contentLength_ = body_.size();
			auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), contentLength_);
			set(Http::Field::ContentLength, {buf, static_cast<std::size_t>(ptr - buf)});
		}

		static constexpr std::string_view rn{"\r\n"};
		alloc_(rn.size());
		std::memcpy(headerBuffer_.data() + headerBufferIdx_, rn.data(), rn.size()); headerBufferIdx_ += rn.size();
		return true;
	}
	Response::Response(Http::Status status, std::string_view version, std::span<std::byte> headerBuffer) noexcept{
		headerBuffer_ = headerBuffer;
		const auto& [intStatus, strStatus, reason] = statusStrArr_[static_cast<std::size_t>(status)];
		init_(strStatus, reason, version);
	}
	Response::Response(std::pair<int, std::string_view> status, std::string_view version, std::span<std::byte> headerBuffer) noexcept{
		headerBuffer_ = headerBuffer;
		char buf[10];
		auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), status.first);
		init_({buf, static_cast<std::size_t>(ptr - buf)}, status.second, version);
	}
	Response::Response(Http::Status status, std::string_view version){
		alloc_(4096);
		const auto& [intStatus, strStatus, reason] = statusStrArr_[static_cast<std::size_t>(status)];
		init_(strStatus, reason, version);
	}
	Response::Response(std::pair<int, std::string_view> status, std::string_view version){
		alloc_(4096);
		char buf[10];
		auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), status.first);
		init_({buf, static_cast<std::size_t>(ptr - buf)}, status.second, version);
	}

	void Response::set(Http::Field field, std::string_view value){
		if(field == Http::Field::ContentType) hasContentType_ = true;
		else if(field == Http::Field::Connection) hasConnection_ = true;
		else if(field == Http::Field::ContentLength) {
			std::from_chars(value.data(), value.data() + value.size(), contentLength_);
		}

		static constexpr std::string_view colon{": "};
		static constexpr std::string_view rn{"\r\n"};
		const auto& fieldStr = fieldStrArr_[static_cast<std::size_t>(field)];
		auto fieldSize = fieldStr.size() + colon.size() + value.size() + rn.size();

		alloc_(fieldSize);

		std::memcpy(headerBuffer_.data() + headerBufferIdx_, fieldStr.data(), fieldStr.size()); headerBufferIdx_ += fieldStr.size();
		std::memcpy(headerBuffer_.data() + headerBufferIdx_, colon.data(), colon.size()); headerBufferIdx_ += colon.size();
		std::memcpy(headerBuffer_.data() + headerBufferIdx_, value.data(), value.size()); headerBufferIdx_ += value.size();
		std::memcpy(headerBuffer_.data() + headerBufferIdx_, rn.data(), rn.size());headerBufferIdx_ += rn.size();
	}
	void Response::set(std::string_view field, std::string_view value){
		if((field == "Content-Type") || (field == "content-type")) hasContentType_ = true;
		else if((field == "Connection") || (field == "connection")) hasConnection_ = true;
		else if((field == "Content-Length") || (field == "content-length")){
			std::from_chars(value.data(), value.data() + value.size(), contentLength_);
		}

		static constexpr std::string_view colon{": "};
		static constexpr std::string_view rn{"\r\n"};
		auto fieldSize = field.size() + colon.size() + value.size() + rn.size();

		alloc_(fieldSize);

		std::memcpy(headerBuffer_.data() + headerBufferIdx_, field.data(), field.size()); headerBufferIdx_ += field.size();
		std::memcpy(headerBuffer_.data() + headerBufferIdx_, colon.data(), colon.size()); headerBufferIdx_ += colon.size();
		std::memcpy(headerBuffer_.data() + headerBufferIdx_, value.data(), value.size()); headerBufferIdx_ += value.size();
		std::memcpy(headerBuffer_.data() + headerBufferIdx_, rn.data(), rn.size());headerBufferIdx_ += rn.size();
	}

	void Response::setBody(std::span<const std::byte> data){
		stringBody_.clear();
		body_ = std::vector<std::byte>(data.begin(), data.end());
		isBodyString_ = false;
	}
	void Response::setBody(std::vector<std::byte>&& data){
		stringBody_.clear();
		body_ = std::move(data);
		isBodyString_ = false;
	}
	void Response::setBody(const std::string& data){
		body_.clear();
		stringBody_ = data;
		isBodyString_ = true;
	}
	void Response::setBody(std::string&& data){
		body_.clear();
		stringBody_ = std::move(data);
		isBodyString_ = true;
	}

	std::tuple<Error, bool, std::size_t> Response::consumeHeaderSome(std::span<const std::byte> data){
		return {ErrorCode::INVALID_MESSAGE, true, 0};
	}
	std::tuple<Error, bool, std::size_t> Response::consumeBodySome(std::span<const std::byte> data){
		return {ErrorCode::INVALID_MESSAGE, true, 0};
	}
	std::tuple<Error, bool, std::size_t> Response::produceHeaderSome(std::span<std::byte> out){
		if (!serialized_) {
			serialize_();
			headerSize_ = headerBufferIdx_;
			headerBufferIdx_ = 0;

			// std::println("headerData: ");
			// for(int i = 0; i < headerSize_; ++i) std::print("{} ", std::to_integer<int>(headerBuffer_[i]));
			// std::print("\n");
		}

		std::size_t remaining = headerSize_ - headerBufferIdx_;
		std::size_t numCopy = std::min(out.size(), remaining);

		std::memcpy(out.data(), headerBuffer_.data() + headerBufferIdx_, numCopy);
		headerBufferIdx_ += numCopy;

		bool finished = headerBufferIdx_ >= headerSize_;
		if (finished) headerBufferIdx_ = 0;
		return {{}, finished, numCopy};
	}
	std::tuple<Error, bool, std::size_t> Response::produceBodySome(std::span<std::byte> out){
		if(isBodyString_){
			if((contentLength_ > 0) && (contentLength_ != stringBody_.size())) stringBody_.resize(contentLength_);
			if(stringBody_.size() == 0) return {{}, true, 0};
			std::size_t remaining = stringBody_.size() - bodyIdx_;
			std::size_t numCopy = std::min(out.size(), remaining);

			std::memcpy(out.data(), reinterpret_cast<std::byte*>(stringBody_.data()) + bodyIdx_, numCopy);
			bodyIdx_ += numCopy;

			bool finished = bodyIdx_ >= stringBody_.size();
			if (finished) bodyIdx_ = 0;
			return {{}, finished, numCopy};
		}
		else{
			if((contentLength_ > 0) && (contentLength_ != body_.size())) body_.resize(contentLength_);
			if(body_.size() == 0) return {{}, true, 0};
			std::size_t remaining = body_.size() - bodyIdx_;
			std::size_t numCopy = std::min(out.size(), remaining);

			std::memcpy(out.data(), body_.data() + bodyIdx_, numCopy);
			bodyIdx_ += numCopy;

			bool finished = bodyIdx_ >= body_.size();
			if (finished) bodyIdx_ = 0;
			return {{}, finished, numCopy};
		}
		return {{}, true, 0};
	}
};

