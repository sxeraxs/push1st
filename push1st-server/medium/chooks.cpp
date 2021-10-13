#include "chooks.h"
#include "../core/csyslog.h"

void cwebhook::Trigger(hook_t::type trigger, sid_t app, sid_t channel, sid_t session, json::value_t&& msg)
{ 
	webConnection->Write("POST", webEndpoint.url(), {
			{"Content-Type","application/json"},
			{"Connection","keep-alive"},
			{"Keep-Alive","30"} }, 
			json::serialize({
				{"event",str(trigger)},
				{"app",app},
				{"channel",channel},
				{"session",session},
				{"data", json::serialize(std::move(msg))},
			}));
}

cconnection::cconnection(const dsn_t& endpoint) : 
	fdHostPort{ endpoint.hostport() }
{
	if (endpoint.ishttps()) {
		inet::SslCreateClientContext(fdSslCtx);
	}
}

void cconnection::Write(const std::string_view& method, const std::string_view& uri, std::unordered_map<std::string_view, std::string>&& headers, std::string&& request) {
	ssize_t res{ -ECONNRESET };
	if (Reconnect()) {
		if (res = HttpWriteRequest({ fdEndpoint, fdSsl }, method, uri, std::move(headers), request); res == 0) {
			return;
		}
		else if (Reconnect()) {
			if (res = HttpWriteRequest({ fdEndpoint, fdSsl }, method, uri, std::move(headers), request); res == 0) {
				return;
			}
		}
	}
	syslog.error("[ HOOK ] Connection %s lost ( %s )\n", fdHostPort.c_str(), std::strerror(-(int)res));
}

inline bool cconnection::Reconnect() {
	if (fdEndpoint > 0 and inet::GetErrorNo(fdEndpoint) == 0) {
		return true;
	}
	if (fdEndpoint > 0) { fdSsl.reset(); inet::Close(fdEndpoint); }

	ssize_t res{ -1 };

	if (sockaddr_storage sa; (res = inet::GetSockAddr(sa, fdHostPort, fdSsl ? "443" : "80", AF_INET)) == 0 ) {
		if (!fdSslCtx) {
			if ((res = inet::TcpConnect(fdEndpoint, sa, false, 2500)) == 0) {

				inet::SetTcpCork(fdEndpoint, false);
				//inet::SetTcpNoDelay(fdEndpoint, true);

				::shutdown(fdEndpoint, SHUT_RD);
				return true;
			}
		}
		else if ((res = inet::SslConnect(fdEndpoint, sa, false, 2500,fdSslCtx,fdSsl)) == 0) {
			::shutdown(fdEndpoint, SHUT_RD);
			return true;
		}
	}

	
	syslog.error("[ HOOK ] Connect %s error ( %s )\n", fdHostPort.c_str(), std::strerror(-(int)res));
	return false;
}

void cluahook::Trigger(hook_t::type trigger, sid_t app, sid_t channel, sid_t session, json::value_t&& msg) {
	jitLua.luaExecute(luaScript, "OnTrigger", { str(trigger), app, channel, session, json::serialize(std::move(msg)) });
}