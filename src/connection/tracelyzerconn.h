#ifndef LORRIS_CONNECTION_TRACELYZERCONN_H
#define LORRIS_CONNECTION_TRACELYZERCONN_H

#include "../misc/signal_trace.h"
#include "connection.h"
#include <stdlib.h>
#include <map>
#include <vector>
#include <QStringList>

class TracelyzerConnection
    : public Connection
{
    Q_OBJECT

public:
    explicit TracelyzerConnection(ConnectionType type)
        : Connection(type)
    {
    }

    virtual size_t maxChannelCount() const = 0;
    virtual void setChannel(size_t channel, size_t input) = 0;
    virtual void setChannelCount(size_t channels) = 0;

    virtual double maxFrequency() const = 0;

    virtual void startTrace(signal_trace_set & output, double freq) = 0;
    virtual void stopTrace() = 0;

    virtual std::map<signal_trace_set::channel_id_t, std::string> const & inputNames() const = 0;
    virtual std::vector<signal_trace_set::channel_id_t> defaultInputs() const = 0;

signals:
    void onData();
};

#endif // LORRIS_CONNECTION_TRACELYZERCONN_H
