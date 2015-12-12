#ifndef LORRIS_CONNECTION_OMICRONANALCONN_H
#define LORRIS_CONNECTION_OMICRONANALCONN_H

#include "connection.h"
#include "tracelyzerconn.h"
#include "../LorrisProgrammer/shupitodesc.h"
#include "../misc/threadchannel.h"
#include <libyb/usb/usb_device.hpp>
#include <libyb/usb/interface_guard.hpp>
#include <libyb/async/async_channel.hpp>
#include <libyb/async/async_runner.hpp>

class OmicronAnalyzerConnection
    : public TracelyzerConnection
{
    Q_OBJECT

public:
    explicit OmicronAnalyzerConnection(yb::async_runner & runner);

    QString details() const;

    void setup(yb::usb_device_interface const & intf, ShupitoDesc const & ybdesc);
    void clear();

    int vid() const;
    int pid() const;
    QString serialNumber() const;
    QString intfName() const;

    QHash<QString, QVariant> config() const;
    bool canSaveToSession() const { return true; }

    size_t maxChannelCount() const override;
    void setChannel(size_t channel, size_t input) override;
    void setChannelCount(size_t channels) override;

    double maxFrequency() const override;

    void startTrace(signal_trace_set & output, double freq) override;
    void stopTrace() override;

    std::map<signal_trace_set::channel_id_t, std::string> const & inputNames() const override;
    std::vector<signal_trace_set::channel_id_t> defaultInputs() const override;

protected:
    void doOpen();
    void doClose();

private slots:
    void readChannelReceive();

private:
    void processData(std::vector<std::pair<uint64_t, std::vector<uint8_t> > > const & data);
    void storeSample(uint16_t sample, signal_trace::block_info & bi);
    void popSample(signal_trace::block_info & bi);
    void storeBi(signal_trace::block_info & bi);
    void closeImpl();

    std::map<signal_trace_set::channel_id_t, std::string> m_inputNames;

    uint8_t m_mux[16];
    signal_trace_set * m_trace_set;
    size_t m_channel_count;
    size_t m_rounded_channel_count;
    double m_samples_per_second;
    uint32_t m_start_addr;
    uint64_t m_start_index;

    uint64_t m_open_trace_marker;
    uint16_t m_compress_sample;
    uint64_t m_sample_index;
    enum compress_state_t { st_prefirst, st_idle, st_count } m_compress_state;
    std::vector<signal_trace *> m_open_traces;

    yb::async_runner & m_runner;
    yb::usb_device_interface m_intf;
    yb::usb_interface_guard m_intf_guard;
    uint8_t m_out_ep;
    size_t m_out_ep_size;
    uint8_t m_in_ep;
    uint8_t m_notify_ep;

    uint8_t m_buffer[64*1024];
    yb::async_future<void> m_read_loop;

    bool m_choked;
    uint32_t m_end_addr;
    yb::task<void> readContinuously();
    yb::task<void> readNext();
    yb::task<void> readMem();

    QMutex m_mutex;
    std::vector<std::pair<uint64_t, std::vector<uint8_t> > > m_data;
    ThreadChannel<void> m_notify_channel;

    QString m_details;
    ShupitoDesc m_desc;
};

#endif // LORRIS_CONNECTION_OMICRONANALCONN_H
