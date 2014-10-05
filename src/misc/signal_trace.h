#ifndef LORRIS_MISC_SIGNAL_TRACE_H
#define LORRIS_MISC_SIGNAL_TRACE_H

#include <stdlib.h>
#include <map>
#include <set>
#include <vector>
#include <string>

struct signal_trace
{
    struct block_info
    {
        size_t block_offset;
        size_t block_length;
        size_t repeat_count;

        block_info()
            : block_offset(0), block_length(0), repeat_count(0)
        {
        }

        block_info(size_t block_offset, size_t block_length, size_t repeat_count)
            : block_offset(block_offset), block_length(block_length), repeat_count(repeat_count)
        {
        }
    };

    struct sample_ptr
    {
        std::map<size_t, block_info>::const_iterator block_it;
        size_t data_offset;
        size_t block_sample_offset;
        size_t repeat_index;
        size_t repeat_offset;

        bool first_repeat;
        bool last_repeat;
    };

    std::map<size_t, block_info> blocks;
    std::vector<bool> data; // yes, I do mean `vector<bool>`

    double samples_per_second;
    uint64_t samples_from_epoch;

    size_t length() const;
    bool sample(size_t index) const;
    std::pair<bool, bool> multisample(size_t first, size_t last) const;

    sample_ptr get_sample_ptr(size_t sample) const;

    double start_time() const { return samples_from_epoch / samples_per_second; }
    double end_time() const { return (samples_from_epoch + this->length()) / samples_per_second; }
};

struct signal_trace_set
{
    typedef size_t channel_id_t;

    std::multimap<channel_id_t, signal_trace> traces;

    uint64_t first_sample_index() const
    {
        uint64_t res = (uint64_t)-1;
        for (auto const & trace: traces)
            res = (std::min)(res, trace.second.samples_from_epoch);
        return res;
    }

    void swap(signal_trace_set & d)
    {
        traces.swap(d.traces);
    }
};

#endif // TRACELYZER_H
