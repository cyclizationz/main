#include "msquic_server.h"
#ifdef HAVE_MSQUIC
#include <cstring>
#include <vector>

MsQuicServer::MsQuicServer() {}
MsQuicServer::~MsQuicServer() { stop(); }

bool MsQuicServer::start(const std::string& bind, uint16_t port, const std::string& alpn, OnSnapshot cb) {
    on_snapshot_ = std::move(cb);
    if (MsQuicOpen(&Api) != QUIC_STATUS_SUCCESS) return false;

    QUIC_REGISTRATION_CONFIG regCfg = { "hidra", QUIC_EXECUTION_PROFILE_LOW_LATENCY };
    if (Api->RegistrationOpen(&regCfg, &Registration) != QUIC_STATUS_SUCCESS) return false;

    QUIC_BUFFER alpnBuf; alpnBuf.Buffer = (uint8_t*)alpn.data(); alpnBuf.Length = (uint32_t)alpn.size();

    QUIC_SETTINGS settings{};
    settings.IdleTimeoutMs = 30000;
    settings.IsSet.IdleTimeoutMs = TRUE;
    settings.ServerResumptionLevel = QUIC_SERVER_RESUME_AND_ZERORTT;
    settings.IsSet.ServerResumptionLevel = TRUE;

    if (Api->ConfigurationOpen(Registration, &alpnBuf, 1, &settings, sizeof(settings), nullptr, &Configuration) != QUIC_STATUS_SUCCESS)
        return false;

    // For dev: use default cred config (self-signed). In real build, set certs here.
    QUIC_CREDENTIAL_CONFIG cred{};
    cred.Type = QUIC_CREDENTIAL_TYPE_NONE;
    cred.Flags = QUIC_CREDENTIAL_FLAG_NO_CERTIFICATE_VALIDATION;
    if (Api->ConfigurationLoadCredential(Configuration, &cred) != QUIC_STATUS_SUCCESS) return false;

    if (Api->ListenerOpen(Registration, listener_cb, this, &Listener) != QUIC_STATUS_SUCCESS) return false;

    QUIC_ADDR addr {};
    QuicAddrSetFamily(&addr, QUIC_ADDRESS_FAMILY_INET);
    QuicAddrSetPort(&addr, port);
    // bind address left as ANY; for specific bind, fill addr with inet_pton.

    if (Api->ListenerStart(Listener, &alpnBuf, 1, &addr) != QUIC_STATUS_SUCCESS) return false;
    return true;
}

void MsQuicServer::stop() {
    if (Listener) { Api->ListenerClose(Listener); Listener=nullptr; }
    if (Configuration) { Api->ConfigurationClose(Configuration); Configuration=nullptr; }
    if (Registration) { Api->RegistrationClose(Registration); Registration=nullptr; }
    if (Api) { MsQuicClose(Api); Api=nullptr; }
}

QUIC_STATUS QUIC_API MsQuicServer::listener_cb(HQUIC, void* ctx, QUIC_LISTENER_EVENT* ev) {
    auto* self = static_cast<MsQuicServer*>(ctx);
    if (ev->Type == QUIC_LISTENER_EVENT_NEW_CONNECTION) {
        self->Api->SetCallbackHandler(ev->NEW_CONNECTION.Connection, (void*)conn_cb, self);
        self->Api->ConnectionSetConfiguration(ev->NEW_CONNECTION.Connection, self->Configuration);
    }
    return QUIC_STATUS_SUCCESS;
}

QUIC_STATUS QUIC_API MsQuicServer::conn_cb(HQUIC conn, void* ctx, QUIC_CONNECTION_EVENT* ev) {
    auto* self = static_cast<MsQuicServer*>(ctx);
    switch (ev->Type) {
        case QUIC_CONNECTION_EVENT_CONNECTED:
            // ready
            break;
        case QUIC_CONNECTION_EVENT_PEER_STREAM_STARTED:
            self->Api->SetCallbackHandler(ev->PEER_STREAM_STARTED.Stream, (void*)stream_cb, self);
            break;
        case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_TRANSPORT:
        case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_PEER:
        case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE:
            self->Api->ConnectionClose(conn);
            break;
        default: break;
    }
    return QUIC_STATUS_SUCCESS;
}

QUIC_STATUS QUIC_API MsQuicServer::stream_cb(HQUIC stream, void* ctx, QUIC_STREAM_EVENT* ev) {
    auto* self = static_cast<MsQuicServer*>(ctx);
    switch (ev->Type) {
        case QUIC_STREAM_EVENT_RECEIVE: {
            for (uint32_t i = 0; i < ev->RECEIVE.BufferCount; ++i) {
                auto* b = ev->RECEIVE.Buffers + i;
                // Expect protobuf Snapshot payloads; parse elsewhere.
                if (self->on_snapshot_) {
                    self->on_snapshot_(b->Buffer, b->Length, /*t_ms*/0, /*seq*/0);
                }
            }
            break;
        }
        case QUIC_STREAM_EVENT_PEER_SEND_SHUTDOWN:
        case QUIC_STREAM_EVENT_SHUTDOWN_COMPLETE:
            self->Api->StreamClose(stream);
            break;
        default: break;
    }
    return QUIC_STATUS_SUCCESS;
}
#endif
